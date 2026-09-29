#include "scene/ending_camera_assets.h"
#include "game/ending_camera_config.h"
#include "model/x_pose.h"
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
struct BkEndingCameraAssets {
  BkModel *models[2];
  BkClipSet *clips[2];
  BkActorPose *poses[2];
  uint32_t roots[2], nodes[2];
  float rotated[2][16];
  BkEndingCameraPresets presets;
  int attached;
};
static int fail(char e[256], const char *why) {
  snprintf(e, 256, "ending camera assets: %s", why);
  return 0;
}
void bk_ending_camera_assets_destroy(BkEndingCameraAssets *a) {
  if (!a)
    return;
  for (unsigned i = 0; i < 2; ++i) {
    bk_actor_pose_destroy(a->poses[i]);
    bk_clip_set_destroy(a->clips[i]);
    bk_model_destroy(a->models[i]);
  }
  free(a);
}
BkActorPose *bk_ending_camera_assets_pose(BkEndingCameraAssets *a, unsigned i) {
  return a && i < 2 ? a->poses[i] : NULL;
}
uint32_t bk_ending_camera_assets_root(const BkEndingCameraAssets *a,
                                      unsigned i) {
  return a && i < 2 ? a->roots[i] : BK_MODEL_NONE;
}
uint32_t bk_ending_camera_assets_node(const BkEndingCameraAssets *a,
                                      unsigned i) {
  return a && i < 2 ? a->nodes[i] : BK_MODEL_NONE;
}
BkEndingCameraAssets *
bk_ending_camera_assets_create(BkResourceStore *store, unsigned group,
                               unsigned variant, int32_t degrees, char e[256]) {
  BkEndingCameraPresets presets;
  if (!store || !bk_ending_camera_config(&presets, group, variant)) {
    fail(e, "invalid profile/variant/store");
    return NULL;
  }
  BkEndingCameraAssets *a = calloc(1, sizeof(*a));
  if (!a) {
    fail(e, "allocation failed");
    return NULL;
  }
  a->presets = presets;
  const char *names[] = {"cam00_00.xan", "cam00_03.xan"};
  const char *nodes[] = {"Cam_AUTO", "locator1"};
  BkBlob raw = {0};
  for (unsigned i = 0; i < 2; ++i) {
    if (bk_resources_read(store, "bk3_04", names[i], &raw, e) != BK_RESOURCE_OK)
      goto bad;
    a->clips[i] = bk_clip_set_decode(raw.data, raw.size, e);
    bk_blob_free(&raw);
    if (!a->clips[i] ||
        bk_resources_read(store, "bk3_04", bk_clip_model_name(a->clips[i]),
                          &raw, e) != BK_RESOURCE_OK)
      goto bad;
    BkModelResult result =
        raw.size >= 16 && !memcmp(raw.data, "xof 0302txt 0032", 16)
            ? bk_model_x_pose_decode(raw.data, raw.size, &a->models[i], e)
            : bk_model_decode(raw.data, raw.size, &a->models[i], e);
    bk_blob_free(&raw);
    if (result != BK_MODEL_OK)
      goto bad;
    a->roots[i] = BK_MODEL_NONE;
    for (uint32_t n = 0; n < a->models[i]->frame_count; ++n)
      if (a->models[i]->frames[n].parent_index == BK_MODEL_NONE) {
        if (a->roots[i] != BK_MODEL_NONE) {
          fail(e, "multiple roots");
          goto bad;
        }
        a->roots[i] = n;
      }
    if (a->roots[i] == BK_MODEL_NONE ||
        !bk_model_find_frame(a->models[i], nodes[i], &a->nodes[i], e))
      goto bad;
    const float *local = a->models[i]->frames[a->roots[i]].local;
    if (!bk_ending_camera_root_rotation(a->rotated[i], local, degrees)) {
      fail(e, "invalid root rotation");
      goto bad;
    }
    a->poses[i] = bk_actor_pose_create_loaded(a->models[i], a->clips[i],
                                              a->roots[i], (float[3]){0}, 0, e);
    if (!a->poses[i] || !bk_actor_pose_root_local(a->poses[i], local, e))
      goto bad;
  }
  return a;
bad:
  bk_blob_free(&raw);
  bk_ending_camera_assets_destroy(a);
  return NULL;
}
static int bound(BkEndingCameraAssets *a, BkActorForest *forest,
                 const uint32_t indices[2], int detached, char e[256]) {
  if (!a || !forest || !indices)
    return fail(e, "missing forest binding");
  const float *global = bk_actor_forest_world(forest, 0);
  for (unsigned i = 0; i < 16; ++i)
    if (global[i] != (float)(i % 5 == 0))
      return fail(e, "nonidentity global anchor");
  const BkFrameTree *tree = bk_actor_forest_tree(forest);
  for (unsigned i = 0; i < 2; ++i) {
    uint32_t root = bk_actor_forest_node(forest, indices[i], a->roots[i]);
    if (root == BK_FRAME_NONE ||
        bk_actor_forest_world(forest, root) !=
            bk_actor_pose_frame(a->poses[i], a->roots[i]) ||
        bk_frame_tree_parent(tree, root) != (detached ? BK_FRAME_NONE : 0))
      return fail(e, "track not bound at required insertion point");
  }
  return 1;
}
int bk_ending_camera_assets_attach(BkEndingCameraAssets *a,
                                   BkActorForest *forest,
                                   const uint32_t indices[2], BkMenuCamera *s,
                                   BkEndingCameraPresets *presets,
                                   char e[256]) {
  if (!s || !presets || !a || a->attached)
    return fail(e, "missing output or already attached");
  if (!bound(a, forest, indices, 1, e))
    return 0;
  for (unsigned i = 0; i < 2; ++i)
    if (!bk_actor_forest_attach(
            forest, 0, bk_actor_forest_node(forest, indices[i], a->roots[i]),
            e))
      return 0;
  for (unsigned i = 0; i < 2; ++i)
    if (!bk_actor_pose_select(a->poses[i], 0, 1, e))
      return 0;
  for (unsigned i = 0; i < 2; ++i)
    if (!bk_actor_pose_root_local(a->poses[i], a->rotated[i], e))
      return 0;
  s->pose.position[0] = s->pose.position[2] = 0;
  s->pose.position[1] = 20;
  s->yaw = a->presets.active[0][0];
  s->pitch = a->presets.active[1][0];
  s->radius = a->presets.active[2][0];
  s->height = a->presets.active[3][0];
  s->fov = 1;
  *presets = a->presets;
  a->attached = 1;
  return 1;
}
int bk_ending_camera_assets_step(BkEndingCameraAssets *a, BkActorForest *forest,
                                 const uint32_t indices[2], BkMenuCamera *s,
                                 BkEndingCameraKind kind, const float offset[3],
                                 const float motion[2], unsigned buttons,
                                 uint32_t target_node, float seconds,
                                 char e[256]) {
  if (!a || !a->attached || !s || kind < BK_ENDING_CAMERA_ORBIT ||
      kind > BK_ENDING_CAMERA_FIXED || !isfinite(seconds) || seconds < 0 ||
      (double)seconds * 60 >= INT32_MAX)
    return fail(e, "invalid controller/timestep/state");
  if (!bound(a, forest, indices, 0, e))
    return 0;
  const float *target = NULL;
  if (kind == BK_ENDING_CAMERA_AUTO || kind == BK_ENDING_CAMERA_MANUAL) {
    target = bk_actor_forest_world(forest, target_node);
    if (!target || target_node < 2)
      return fail(e, "missing target node binding");
    target += 12;
  }
  int ok;
  switch (kind) {
  case BK_ENDING_CAMERA_ORBIT:
    ok = bk_ending_camera_orbit(s, motion, buttons, offset, 0x10, seconds, e);
    break;
  case BK_ENDING_CAMERA_AUTO:
    if (!bk_actor_pose_advance(a->poses[0], -1, seconds, e))
      return 0;
    ok = bk_ending_camera_track_pose(
        s, bk_actor_pose_frame(a->poses[0], a->nodes[0]) + 12, target, seconds,
        e);
    break;
  case BK_ENDING_CAMERA_MANUAL:
    ok =
        bk_ending_camera_manual(s, motion, buttons, offset, target, seconds, e);
    break;
  case BK_ENDING_CAMERA_FIXED:
    ok = bk_ending_camera_fixed(s, offset, e);
    break;
  default:
    return fail(e, "unsupported controller");
  }
  return ok && bk_actor_forest_anchor(forest, 1, s->pose.world, 0, e);
}
int bk_ending_camera_assets_opening(
    BkEndingCameraAssets *a, BkActorForest *forest, const uint32_t indices[2],
    BkMenuCamera *s, uint32_t target_node, float seconds,
    const BkEndingCameraOpeningInput *input, int *complete, char e[256]) {
  if (!a || !a->attached || !s || !complete || !isfinite(seconds) ||
      seconds < 0 || (double)seconds * 60 >= INT32_MAX)
    return fail(e, "invalid opening controller/timestep/state");
  if (!bound(a, forest, indices, 0, e) ||
      !bk_actor_pose_advance(a->poses[1], -1, seconds, e))
    return 0;
  const float *target = bk_actor_forest_world(forest, target_node);
  const float *track = bk_actor_pose_frame(a->poses[1], a->nodes[1]);
  if (!target || target_node < 2 || !track)
    return fail(e, "missing opening camera target/track binding");
  BkMenuCamera next = *s;
  if (!bk_ending_camera_opening_pose(&next, track + 12, target + 12, seconds,
                                     e) ||
      !bk_actor_forest_anchor(forest, 1, next.pose.world, 0, e))
    return 0;
  *s = next;
  if (!input || !input->key)
    return fail(e, "missing opening camera key service");
  static const unsigned keys[] = {0, 0x5a, 0x33450, 1};
  BkClipState active;
  BkClipTiming timing;
  for (unsigned i = 0; i < sizeof(keys) / sizeof(*keys); ++i) {
    uint32_t pressed = 0;
    if (!input->key(input->context, keys[i], 1, &pressed, e))
      return 0;
    if (!(pressed & 255u))
      continue;
    if (!bk_actor_pose_state(a->poses[1], &active) ||
        !bk_actor_pose_timing(a->poses[1], active.slot, &timing))
      return fail(e, "opening camera active timing is unavailable");
    BkClipEdit edit = {.slot = (unsigned)active.slot,
                       .fields = BK_CLIP_EDIT_SOURCE,
                       .source = timing.end};
    if (!bk_actor_pose_edit_clips(a->poses[1], &edit, 1, e))
      return 0;
    break;
  }
  if (!bk_actor_pose_state(a->poses[1], &active) ||
      !bk_actor_pose_timing(a->poses[1], active.slot, &timing))
    return fail(e, "opening camera completion timing is unavailable");
  *complete = !((double)timing.end - 9.0 > (double)timing.source);
  return 1;
}
int bk_ending_camera_assets_preset(
    BkEndingCameraAssets *a, BkActorForest *forest, const uint32_t indices[2],
    BkMenuCamera *s, BkEndingCameraTransitions *transitions,
    const BkEndingCameraPresets *presets, BkEndingPresetKind kind,
    unsigned choice, const float offset[3],
    const BkEndingCameraPresetGate *gate, uint8_t flow, float seconds,
    int *complete, char e[256]) {
  if (!a || !a->attached || !s || !transitions || !complete ||
      (kind != BK_ENDING_PRESET && kind != BK_ENDING_PRESET_ZOOM))
    return fail(e, "invalid preset controller/state");
  if (!bound(a, forest, indices, 0, e))
    return 0;
  BkMenuCamera next = *s;
  BkEndingCameraTransitions nt = *transitions;
  int done;
  int ok = kind == BK_ENDING_PRESET
               ? bk_ending_camera_preset(&next, &nt, presets, choice, offset,
                                         gate, seconds, &done, e)
               : bk_ending_camera_preset_zoom(&next, &nt, presets, choice,
                                              offset, flow, seconds, &done, e);
  if (!ok || !bk_actor_forest_anchor(forest, 1, next.pose.world, 0, e))
    return 0;
  *s = next;
  *transitions = nt;
  *complete = done;
  return 1;
}
