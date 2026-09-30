#include "world/actor_pose.h"
#include "core/matrix.h"
#include "world/actor_pose_internal.h"
#include "world/eye_pose.h"
#include "world/node_reference.h"
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
struct BkActorPose {
  const BkModel *model;
  BkModelPlayback *playback;
  uint32_t root, head, count;
  BkActorPlacement placement;
  float *published, *parent_world, *composed;
  uint32_t *hidden, *pending_hidden;
};
static int fail(char *error, const char *message) {
  snprintf(error, 256, "actor pose: %s", message);
  return 0;
}
const BkModel *bk_actor_pose_model(const BkActorPose *a) {
  return a ? a->model : NULL;
}
void bk_actor_pose_destroy(BkActorPose *a) {
  if (!a)
    return;
  bk_model_playback_destroy(a->playback);
  free(a->published);
  free(a->parent_world);
  free(a->composed);
  free(a->hidden);
  free(a->pending_hidden);
  free(a);
}
static BkActorPose *create(const BkModel *model, const BkClipSet *clips,
                           uint32_t root, const char *head_node,
                           const float position[3], float yaw,
                           unsigned initial_clip, int instant, int loaded,
                           char error[256]) {
  BkActorPlacement placement;
  uint32_t head = BK_MODEL_NONE;
  if (!model || root >= model->frame_count ||
      model->frames[root].parent_index != BK_MODEL_NONE ||
      (instant != 0 && instant != 1) ||
      !bk_actor_placement(&placement, position, yaw)) {
    fail(error, "invalid root, placement or policy");
    return NULL;
  }
  if (head_node && !bk_model_find_frame(model, head_node, &head, error))
    return NULL;
  uint32_t ancestor = head;
  for (uint32_t n = 0;
       head != BK_MODEL_NONE && ancestor != root && n < model->frame_count;
       n++) {
    if (ancestor >= model->frame_count)
      break;
    ancestor = model->frames[ancestor].parent_index;
  }
  if (head != BK_MODEL_NONE && (ancestor != root || head == root)) {
    fail(error, "head is not a descendant of actor root");
    return NULL;
  }
  BkActorPose *a = calloc(1, sizeof(*a));
  if (!a) {
    fail(error, "allocation failed");
    return NULL;
  }
  a->root = root;
  a->model = model;
  a->head = head;
  a->count = model->frame_count;
  a->placement = placement;
  a->playback = loaded ? bk_model_playback_create_loaded(model, clips, error)
                       : bk_model_playback_create_started(
                             model, clips, initial_clip, instant, error);
  if (!a->playback)
    goto bad;
  a->published = malloc((size_t)a->count * 16 * sizeof(float));
  a->parent_world = malloc((size_t)a->count * 16 * sizeof(float));
  a->composed = malloc((size_t)a->count * 16 * sizeof(float));
  a->hidden = calloc(a->count, sizeof(*a->hidden));
  a->pending_hidden = malloc((size_t)a->count * sizeof(*a->pending_hidden));
  if (!a->published || !a->parent_world || !a->composed || !a->hidden ||
      !a->pending_hidden) {
    fail(error, "pose allocation failed");
    goto bad;
  }
  bk_actor_pose_publish(a);
  BkModelRootTransform transform = {.frame = root};
  memcpy(transform.world, placement.world, 64);
  if (!bk_model_playback_place(a->playback, &transform, error))
    goto bad;
  memcpy(a->published + root * 16, placement.world, 64);
  return a;
bad:
  bk_actor_pose_destroy(a);
  return NULL;
}
BkActorPose *bk_actor_pose_create(const BkModel *model, const BkClipSet *clips,
                                  uint32_t root, const char *head_node,
                                  const float position[3], float yaw,
                                  unsigned initial_clip, int instant,
                                  char error[256]) {
  return create(model, clips, root, head_node, position, yaw, initial_clip,
                instant, 0, error);
}
BkActorPose *bk_actor_pose_create_loaded(const BkModel *model,
                                         const BkClipSet *clips, uint32_t root,
                                         const float position[3], float yaw,
                                         char error[256]) {
  return create(model, clips, root, NULL, position, yaw, 0, 0, 1, error);
}
int bk_actor_pose_step(BkActorPose *a, const float position[3], float yaw,
                       int request, float seconds, char error[256]) {
  return bk_actor_pose_step_mode(a, position, yaw, request,
                                 BK_CLIP_REQUEST_TEN_TICKS, seconds, error);
}
int bk_actor_pose_place(BkActorPose *a, const float position[3], float yaw,
                        char error[256]) {
  BkActorPlacement placement;
  if (!a || !bk_actor_placement(&placement, position, yaw))
    return fail(error, "invalid placement");
  return bk_actor_pose_place_exact(a, &placement, error);
}
static int root_from_world(BkActorPose *a, const float world[16],
                           BkModelRootTransform *transform, char error[256]) {
  float inverse[16];
  if (!bk_matrix_inverse(inverse, a->parent_world + a->root * 16))
    return fail(error, "singular cached parent for world placement");
  transform->frame = a->root;
  bk_matrix_multiply(transform->world, world, inverse);
  return 1;
}
int bk_actor_pose_place_exact(BkActorPose *a, const BkActorPlacement *p,
                              char error[256]) {
  if (!a || !p || !isfinite(p->yaw_degrees))
    return fail(error, "invalid placement");
  for (unsigned i = 0; i < 16; i++)
    if (!isfinite(p->world[i]))
      return fail(error, "nonfinite root matrix");
  for (unsigned i = 0; i < 3; i++)
    if (!isfinite(p->position[i]))
      return fail(error, "nonfinite placement");
  BkActorPlacement placement = *p;
  BkModelRootTransform transform = {.frame = a->root};
  if (!root_from_world(a, placement.world, &transform, error) ||
      !bk_model_playback_place(a->playback, &transform, error))
    return 0;
  a->placement = placement;
  memcpy(a->published + a->root * 16, placement.world, 64);
  return 1;
}
static int advance(BkActorPose *a, const BkActorPlacement *p, int request,
                   BkClipRequestMode mode, float seconds, char error[256]) {
  if (!isfinite(seconds) || seconds < 0 || (double)seconds * 60 >= INT32_MAX)
    return fail(error, "invalid timestep");
  if (!a || !p)
    return fail(error, "invalid placement");
  BkActorPlacement placement = *p;
  BkModelRootTransform transform = {.frame = a->root};
  if (!root_from_world(a, placement.world, &transform, error))
    return 0;
  int ok = a->hidden[a->root]
               ? bk_model_playback_hold_mode(a->playback, request, mode,
                                             &transform, error)
               : bk_model_playback_step_mode(a->playback, request, mode,
                                             seconds, &transform, error);
  if (!ok)
    return 0;
  a->placement = placement;
  memcpy(a->published + a->root * 16, placement.world, 64);
  return 1;
}
int bk_actor_pose_advance(BkActorPose *a, int request, float seconds,
                          char error[256]) {
  if (!a || !isfinite(seconds) || seconds < 0 ||
      (double)seconds * 60 >= INT32_MAX)
    return fail(error, "invalid timestep or actor");
  /* 4026fe writes animated locals, never the root world or parent cache.
   * Keeping the current local is essential for camera-parented weather. */
  BkModelRootTransform root = {.frame = a->root};
  memcpy(root.world, bk_model_playback_local(a->playback, a->root), 64);
  return a->hidden[a->root]
             ? bk_model_playback_hold_mode(a->playback, request,
                                           BK_CLIP_REQUEST_TEN_TICKS, &root,
                                           error)
             : bk_model_playback_step_mode(a->playback, request,
                                           BK_CLIP_REQUEST_TEN_TICKS, seconds,
                                           &root, error);
}
int bk_actor_pose_step_mode(BkActorPose *a, const float position[3], float yaw,
                            int request, BkClipRequestMode mode, float seconds,
                            char error[256]) {
  BkActorPlacement placement;
  if (!bk_actor_placement(&placement, position, yaw))
    return fail(error, "invalid placement");
  return advance(a, &placement, request, mode, seconds, error);
}
int bk_actor_pose_select(BkActorPose *a, unsigned slot, int instant,
                         char error[256]) {
  if (!a)
    return fail(error, "missing actor");
  return bk_model_playback_select(a->playback, slot, instant, error);
}
int bk_actor_pose_request(BkActorPose *a, unsigned slot, char error[256]) {
  return bk_actor_pose_request_mode(a, slot, BK_CLIP_REQUEST_TEN_TICKS, error);
}
int bk_actor_pose_request_mode(BkActorPose *a, unsigned slot,
                               BkClipRequestMode mode, char error[256]) {
  if (!a)
    return fail(error, "missing actor");
  return bk_model_playback_request_mode(a->playback, slot, mode, error);
}
int bk_actor_pose_edit_clips(BkActorPose *a, const BkClipEdit *edits,
                             size_t count, char error[256]) {
  return a ? bk_model_playback_edit_clips(a->playback, edits, count, error)
           : fail(error, "missing actor");
}
int bk_actor_pose_reset_sources(BkActorPose *a, const unsigned *slots,
                                size_t count, char error[256]) {
  return a ? bk_model_playback_reset_sources(a->playback, slots, count, error)
           : fail(error, "missing actor");
}
int bk_actor_pose_set_clock(BkActorPose *a, unsigned slot, float elapsed,
                             float source, char error[256]) {
  return a ? bk_model_playback_set_clock(a->playback, slot, elapsed, source, error)
           : fail(error, "missing actor");
}
int bk_actor_pose_clip_link(const BkActorPose *a, unsigned slot, int32_t *chain,
                            int32_t *next) {
  return a && bk_model_playback_link(a->playback, slot, chain, next);
}
static void publish(BkActorPose *a, const float *world, const float *external) {
  if (a)
    for (uint32_t i = 0; i < a->count; i++) {
      uint32_t parent = a->model->frames[i].parent_index;
      while (parent != BK_MODEL_NONE && !a->hidden[parent])
        parent = a->model->frames[parent].parent_index;
      if (parent == BK_MODEL_NONE) {
        memcpy(a->published + i * 16, world + i * 16, 64);
        uint32_t p = a->model->frames[i].parent_index;
        static const float identity[16] = {1, 0, 0, 0, 0, 1, 0, 0,
                                           0, 0, 1, 0, 0, 0, 0, 1};
        memcpy(a->parent_world + i * 16,
               p == BK_MODEL_NONE
                   ? (i == a->root && external ? external : identity)
                   : world + p * 16,
               64);
      }
    }
}
void bk_actor_pose_publish(BkActorPose *a) {
  if (a)
    publish(a, bk_model_playback_frame(a->playback, 0), NULL);
}
static int affine(const float *m) {
  if (!m || m[3] || m[7] || m[11] || m[15] != 1)
    return 0;
  for (unsigned i = 0; i < 16; ++i)
    if (!isfinite(m[i]))
      return 0;
  return 1;
}
int bk_actor_pose_root_local(BkActorPose *a, const float local[16],
                             char error[256]) {
  if (!a || !affine(local))
    return fail(error, "invalid root local");
  float world[16];
  bk_matrix_multiply(world, local, a->parent_world + a->root * 16);
  if (!affine(world))
    return fail(error, "root local composition overflow");
  BkModelRootTransform transform = {.frame = a->root};
  memcpy(transform.world, local, 64);
  if (!bk_model_playback_place(a->playback, &transform, error))
    return 0;
  memcpy(a->placement.world, world, 64);
  memcpy(a->placement.position, world + 12, 12);
  memcpy(a->published + a->root * 16, world, 64);
  return 1;
}
int bk_actor_pose_publish_under(BkActorPose *a, const float external[16],
                                char error[256]) {
  if (!a || !affine(external))
    return fail(error, "invalid external parent");
  /* external may alias one of the current published caches. */
  float parent[16];
  memcpy(parent, external, 64);
  if (!bk_model_pose_world_matrices_under(
          a->model, bk_model_playback_local(a->playback, 0), a->root, parent,
          a->composed, (size_t)a->count * 16, error))
    return 0;
  publish(a, a->composed, parent);
  return 1;
}
void bk_actor_pose_commit_composed(BkActorPose *a, uint32_t frame,
                                   const float world[16],
                                   const float parent[16]) {
  memcpy(a->published + frame * 16, world, 64);
  memcpy(a->parent_world + frame * 16, parent, 64);
}
const float *bk_actor_pose_world(const BkActorPose *a, size_t *count) {
  if (!a || !count)
    return NULL;
  *count = (size_t)a->count * 16;
  return a->published;
}
int bk_actor_pose_publish_node(BkActorPose *a, uint32_t frame,
                               const float external[16], char error[256]) {
  if (!a || frame >= a->count || !external)
    return fail(error, "invalid node publication");
  float parent[16], world[16];
  memcpy(parent, external, sizeof(parent));
  for (unsigned i = 0; i < 16; ++i)
    if (!isfinite(parent[i]))
      return fail(error, "nonfinite node parent");
  bk_matrix_multiply(world, bk_actor_pose_local(a, frame), parent);
  for (unsigned i = 0; i < 16; ++i)
    if (!isfinite(world[i]))
      return fail(error, "node publication overflow");
  memcpy(a->published + frame * 16, world, sizeof(world));
  memcpy(a->parent_world + frame * 16, parent, sizeof(parent));
  return 1;
}
int bk_actor_pose_visibility(BkActorPose *a, const BkActorVisibilityEdit *edits,
                             size_t count, char error[256]) {
  if (!a || (count && !edits))
    return fail(error, "invalid visibility edits");
  memcpy(a->pending_hidden, a->hidden, (size_t)a->count * sizeof(*a->hidden));
  for (size_t e = 0; e < count; e++) {
    if (edits[e].frame >= a->count)
      return fail(error, "invalid visibility frame");
    for (uint32_t i = 0; i < a->count; i++) {
      uint32_t ancestor = i;
      while (ancestor != BK_MODEL_NONE && ancestor != edits[e].frame)
        ancestor = a->model->frames[ancestor].parent_index;
      if (ancestor != BK_MODEL_NONE)
        a->pending_hidden[i] = edits[e].hidden;
    }
  }
  uint32_t *old = a->hidden;
  a->hidden = a->pending_hidden;
  a->pending_hidden = old;
  return 1;
}
int bk_actor_pose_hidden(const BkActorPose *a, uint32_t frame,
                         uint32_t *hidden) {
  if (!a || !hidden || frame >= a->count)
    return 0;
  *hidden = a->hidden[frame];
  return 1;
}
const float *bk_actor_pose_frame(const BkActorPose *a, uint32_t frame) {
  return a && frame < a->count ? a->published + frame * 16 : NULL;
}
const float *bk_actor_pose_local(const BkActorPose *a, uint32_t frame) {
  return a ? bk_model_playback_local(a->playback, frame) : NULL;
}
const float *bk_actor_pose_head(const BkActorPose *a) {
  return a && a->head != BK_MODEL_NONE ? a->published + a->head * 16 + 12
                                       : NULL;
}
const BkActorPlacement *bk_actor_pose_placement(const BkActorPose *a) {
  return a ? &a->placement : NULL;
}
int bk_actor_pose_state(const BkActorPose *a, BkClipState *state) {
  return a && bk_model_playback_state(a->playback, state);
}
int bk_actor_pose_timing(const BkActorPose *a, unsigned slot,
                         BkClipTiming *timing) {
  return a && bk_model_playback_timing(a->playback, slot, timing);
}
int bk_actor_pose_loops(const BkActorPose *a, unsigned slot, int32_t *loops) {
  return a && bk_model_playback_loops(a->playback, slot, loops);
}
int bk_actor_pose_loop_mode(const BkActorPose *a, unsigned slot, int32_t *mode) {
  return a && bk_model_playback_loop_mode(a->playback, slot, mode);
}
int bk_actor_pose_completed_chain(const BkActorPose *a, unsigned slot, int *out) {
  return a && bk_model_playback_completed_chain(a->playback, slot, out);
}
int bk_actor_pose_prediction(const BkActorPose *a, unsigned slot,
                             BkClipPrediction *out) {
  return a && bk_model_playback_prediction(a->playback, slot, out);
}
const float *bk_actor_pose_parent_world(const BkActorPose *a, uint32_t frame) {
  return a && frame < a->count ? a->parent_world + frame * 16 : NULL;
}
int bk_actor_pose_eyes(BkActorPose *a, const uint32_t frames[2],
                       const float target[16], uint32_t mode, uint32_t variant,
                       float pitch, float yaw, char error[256]) {
  if (!a || !frames)
    return fail(error, "missing eye instance/indices");
  if (frames[0] == BK_MODEL_NONE || frames[1] == BK_MODEL_NONE)
    return 1;
  if (frames[0] >= a->count || frames[1] >= a->count || frames[0] == frames[1])
    return fail(error, "invalid eye indices");
  BkEyePoseFrame eyes[2];
  BkModelLocalEdit edits[2];
  for (unsigned i = 0; i < 2; i++) {
    memcpy(eyes[i].local, bk_actor_pose_local(a, frames[i]), 64);
    memcpy(eyes[i].world, a->published + frames[i] * 16, 64);
    memcpy(eyes[i].parent_world, a->parent_world + frames[i] * 16, 64);
  }
  if (!bk_eye_pose_aim(eyes, target, mode, variant, pitch, yaw, error))
    return 0;
  for (unsigned i = 0; i < 2; i++) {
    edits[i].frame = frames[i];
    memcpy(edits[i].local, eyes[i].local, 64);
  }
  if (!bk_model_playback_edit_locals(a->playback, edits, 2, error))
    return 0;
  for (unsigned i = 0; i < 2; i++)
    memcpy(a->published + frames[i] * 16, eyes[i].world, 64);
  return 1;
}

int bk_actor_pose_align_reference(BkActorPose *a, uint32_t frame,
                                  const float reference[16], char error[256]) {
  if (!a || frame >= a->count || frame == a->root)
    return fail(error, "invalid reference node");
  BkNodeReference node;
  memcpy(node.local, bk_actor_pose_local(a, frame), 64);
  memcpy(node.world, a->published + frame * 16, 64);
  memcpy(node.parent_world, a->parent_world + frame * 16, 64);
  memset(node.local + 12, 0, 12);
  if (!bk_node_reference_position(&node, reference, (float[]){0, 0, 0},
                                  error) ||
      !bk_node_reference_orientation(&node, reference, (float[]){0, 0, 1},
                                     (float[]){0, 1, 0}, error))
    return 0;
  BkModelLocalEdit edit = {.frame = frame};
  memcpy(edit.local, node.local, 64);
  if (!bk_model_playback_edit_locals(a->playback, &edit, 1, error))
    return 0;
  memcpy(a->published + frame * 16, node.world, 64);
  return 1;
}

int bk_actor_pose_request_active(BkActorPose *a, unsigned slot,
                                 char error[256]) {
  return a ? bk_model_playback_request_active(a->playback, slot, error)
           : fail(error, "missing actor");
}
int bk_actor_pose_node_reference(const BkActorPose *a, uint32_t frame,
                                 BkNodeReference *out, char error[256]) {
  if (!a || !out || frame >= a->count || frame == a->root)
    return fail(error, "invalid procedural node");
  memcpy(out->local, bk_actor_pose_local(a, frame), 64);
  memcpy(out->world, a->published + frame * 16, 64);
  memcpy(out->parent_world, a->parent_world + frame * 16, 64);
  return 1;
}
void bk_actor_pose_commit_hidden(BkActorPose *a, uint32_t frame, uint32_t hidden) {
  a->hidden[frame] = hidden;
}
int bk_actor_pose_commit_reference(BkActorPose *a, uint32_t frame,
                                   const BkNodeReference *node,
                                   char error[256]) {
  if (!a || !node || frame >= a->count || frame == a->root ||
      memcmp(node->parent_world, a->parent_world + frame * 16, 64))
    return fail(error, "invalid/stale procedural node");
  for (unsigned i = 0; i < 16; i++)
    if (!isfinite(node->local[i]) || !isfinite(node->world[i]))
      return fail(error, "nonfinite procedural node");
  BkModelLocalEdit edit = {.frame = frame};
  memcpy(edit.local, node->local, 64);
  if (!bk_model_playback_edit_locals(a->playback, &edit, 1, error))
    return 0;
  memcpy(a->published + frame * 16, node->world, 64);
  return 1;
}
int bk_actor_pose_advance_frame(BkActorPose *a, BkClipSample *sample,
                                int *submitted, char error[256]) {
  if (!a || !sample || !submitted)
    return fail(error, "missing frame actor/output");
  if (a->hidden[a->root]) {
    *submitted = 0;
    return 1;
  }
  BkModelRootTransform root = {.frame = a->root};
  memcpy(root.world, bk_model_playback_local(a->playback, a->root), 64);
  if (!bk_model_playback_advance_frame(a->playback, &root, sample, error))
    return 0;
  *submitted = 1;
  return 1;
}
int bk_actor_pose_advance_effects(BkActorPose *a, float seconds,
                                  BkPlaybackEffects *effects, int *submitted,
                                  char error[256]) {
  if (!a || !effects || !submitted || !isfinite(seconds) || seconds < 0 ||
      (double)seconds * 60 >= INT32_MAX)
    return fail(error, "invalid effect actor/timestep");
  if (a->hidden[a->root]) {
    *submitted = 0;
    return 1;
  }
  BkModelRootTransform root = {.frame = a->root};
  memcpy(root.world, bk_model_playback_local(a->playback, a->root), 64);
  if (!bk_model_playback_step_effects(a->playback, -1,
                                      BK_CLIP_REQUEST_TEN_TICKS, seconds, &root,
                                      effects, error))
    return 0;
  *submitted = 1;
  return 1;
}
int bk_actor_pose_advance_plain(BkActorPose *a, float seconds,
                                 BkClipPlainMode mode, BkPlaybackEffects *effects,
                                 int *submitted, char error[256]) {
  if (!a || !effects || !submitted || !isfinite(seconds) ||
      (seconds < 0 && mode != BK_CLIP_PLAIN_SCHEDULED) ||
      fabs((double)seconds * 60) >= INT32_MAX || mode < BK_CLIP_PLAIN_SCHEDULED ||
      mode > BK_CLIP_PLAIN_FORCE_CHAIN)
    return fail(error, "invalid plain actor/timestep");
  if (a->hidden[a->root]) {
    *submitted = 0;
    return 1;
  }
  BkModelRootTransform root = {.frame = a->root};
  memcpy(root.world, bk_model_playback_local(a->playback, a->root), 64);
  if (!bk_model_playback_advance_plain(a->playback, seconds, mode, &root,
                                       effects, error))
    return 0;
  *submitted = 1;
  return 1;
}
