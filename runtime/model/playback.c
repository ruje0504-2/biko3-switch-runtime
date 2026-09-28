#include "model/playback.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct BkModelPlayback {
  const BkModel *model;
  BkModelAnimation *animation;
  BkClipPlayer *player, *pending;
  float *world, *next_world, *local, *next_local;
  float plain_time;
};
static int fail(char *error, const char *message) {
  snprintf(error, 256, "model playback: %s", message);
  return 0;
}
void bk_model_playback_destroy(BkModelPlayback *p) {
  if (!p)
    return;
  bk_model_animation_destroy(p->animation);
  bk_clip_player_destroy(p->player);
  bk_clip_player_destroy(p->pending);
  free(p->world);
  free(p->next_world);
  free(p->local);
  free(p->next_local);
  free(p);
}
static BkModelPlayback *create(const BkModel *model, const BkClipSet *clips,
                               int authored, int initial_slot, int instant,
                               char error[256]) {
  if (!model || !clips || (authored != 0 && authored != 1)) {
    fail(error, "invalid assets or playback policy");
    return NULL;
  }
  BkModelPlayback *p = calloc(1, sizeof(*p));
  if (!p) {
    fail(error, "allocation failed");
    return NULL;
  }
  p->model = model;
  /* 0x409660 zero-initializes the animation group, including last plain
   * sample time. A first plain request at zero therefore preserves base. */
  p->animation = bk_model_animation_create(model, error);
  if (!p->animation)
    goto bad;
  p->player = initial_slot == -2 ? bk_clip_player_create_loaded(clips, error)
              : initial_slot >= 0
                  ? bk_clip_player_create_authored_start(
                        clips, (unsigned)initial_slot, instant, error)
              : authored ? bk_clip_player_create_authored(clips, error)
                         : bk_clip_player_create(clips, error);
  if (!p->player)
    goto bad;
  p->pending = bk_clip_player_create(clips, error);
  if (!p->pending)
    goto bad;
  size_t count = (size_t)model->frame_count * 16;
  p->world = malloc(count * sizeof(float));
  p->next_world = malloc(count * sizeof(float));
  p->local = malloc(count * sizeof(float));
  p->next_local = malloc(count * sizeof(float));
  if (!p->world || !p->next_world || !p->local || !p->next_local) {
    fail(error, "pose allocation failed");
    goto bad;
  }
  if (!bk_model_world_matrices(model, p->world, count, error))
    goto bad;
  for (uint32_t i = 0; i < model->frame_count; i++)
    memcpy(p->local + i * 16, model->frames[i].local, 64);
  return p;
bad:
  bk_model_playback_destroy(p);
  return NULL;
}
BkModelPlayback *bk_model_playback_create(const BkModel *model,
                                          const BkClipSet *clips, int authored,
                                          char error[256]) {
  return create(model, clips, authored, -1, 0, error);
}
BkModelPlayback *bk_model_playback_create_loaded(const BkModel *model,
                                                 const BkClipSet *clips,
                                                 char error[256]) {
  return create(model, clips, 1, -2, 0, error);
}
BkModelPlayback *bk_model_playback_create_started(const BkModel *model,
                                                  const BkClipSet *clips,
                                                  unsigned slot, int instant,
                                                  char error[256]) {
  if (slot >= BK_CLIP_SLOTS || (instant != 0 && instant != 1)) {
    fail(error, "invalid initial selection");
    return NULL;
  }
  return create(model, clips, 1, (int)slot, instant, error);
}
int bk_model_playback_select(BkModelPlayback *p, unsigned slot, int instant,
                             char error[256]) {
  return p ? bk_clip_select(p->player, slot, instant, error)
           : fail(error, "missing instance");
}
int bk_model_playback_request_active(BkModelPlayback *p, unsigned slot,
                                     char error[256]) {
  return p ? bk_clip_request_active(p->player, slot, error)
           : fail(error, "missing instance");
}
int bk_model_playback_request(BkModelPlayback *p, unsigned slot,
                              char error[256]) {
  return bk_model_playback_request_mode(p, slot, BK_CLIP_REQUEST_TEN_TICKS,
                                        error);
}
int bk_model_playback_request_mode(BkModelPlayback *p, unsigned slot,
                                   BkClipRequestMode mode, char error[256]) {
  return p ? bk_clip_request_mode(p->player, slot, mode, error)
           : fail(error, "missing instance");
}
int bk_model_playback_edit_clips(BkModelPlayback *p, const BkClipEdit *edits,
                                 size_t count, char error[256]) {
  return p ? bk_clip_edit(p->player, edits, count, error)
           : fail(error, "missing instance");
}
int bk_model_playback_link(const BkModelPlayback *p, unsigned slot,
                           int32_t *chain, int32_t *next) {
  return p && bk_clip_link(p->player, slot, chain, next);
}
int bk_model_playback_advance(BkModelPlayback *p, float seconds,
                              const BkModelRootTransform *root,
                              char error[256]) {
  return bk_model_playback_step(p, -1, seconds, root, error);
}
int bk_model_playback_step(BkModelPlayback *p, int requested_slot,
                           float seconds, const BkModelRootTransform *root,
                           char error[256]) {
  return bk_model_playback_step_mode(
      p, requested_slot, BK_CLIP_REQUEST_TEN_TICKS, seconds, root, error);
}
static int commit_sample(BkModelPlayback *p, BkClipSample clip,
                         const BkModelRootTransform *root, char error[256]) {
  BkModelPoseSample sample = {clip.from, clip.to, clip.weight, clip.blend, 1};
  /* 0x4097d6 suppresses a plain sample equal to group+0x74. 0x409a94
   * updates locals without touching that cache, so a later matching plain
   * time must retain the most recent BLENDED locals too. Recompose only to
   * apply the current root; clip state still advances independently. */
  int plain_submitted = !clip.blend && p->plain_time != clip.from;
  int sampled = clip.blend || plain_submitted;
  size_t count = (size_t)p->model->frame_count * 16;
  int ok = bk_model_animation_update(p->animation, sampled ? &sample : NULL,
                                     root, p->local, p->next_world,
                                     p->next_local, count, error);
  if (!ok)
    return 0;
  BkClipPlayer *old = p->player;
  p->player = p->pending;
  p->pending = old;
  float *previous = p->world;
  p->world = p->next_world;
  p->next_world = previous;
  previous = p->local;
  p->local = p->next_local;
  p->next_local = previous;
  if (plain_submitted) {
    p->plain_time = clip.from;
  }
  return 1;
}
static int step_effects(BkModelPlayback *p, int requested_slot,
                        BkClipRequestMode mode, float seconds,
                        const BkModelRootTransform *root,
                        BkPlaybackEffects *out, char error[256]) {
  if (requested_slot < -1 || requested_slot >= 128 ||
      (mode != BK_CLIP_REQUEST_TEN_TICKS && mode != BK_CLIP_REQUEST_CONFIGURED))
    return fail(error, "invalid requested slot");
  if (!p || !bk_clip_player_copy(p->pending, p->player))
    return fail(error, "invalid instance state");
  if (requested_slot >= 0 &&
      !bk_clip_request_mode(p->pending, (unsigned)requested_slot, mode, error))
    return 0;
  BkClipState before;
  BkClipTiming timing;
  BkPlaybackEffects effects;
  if ((out && !bk_clip_state(p->pending, &before)) ||
      !bk_clip_advance(p->pending, seconds, &effects.pose, error) ||
      (out && !bk_clip_timing(p->pending, (unsigned)before.slot, &timing)) ||
      !commit_sample(p, effects.pose, root, error))
    return 0;
  if (out) {
    effects.source = timing.source;
    *out = effects;
  }
  return 1;
}
int bk_model_playback_step_mode(BkModelPlayback *p, int requested_slot,
                                BkClipRequestMode mode, float seconds,
                                const BkModelRootTransform *root,
                                char error[256]) {
  return step_effects(p, requested_slot, mode, seconds, root, NULL, error);
}
int bk_model_playback_step_effects(BkModelPlayback *p, int requested_slot,
                                   BkClipRequestMode mode, float seconds,
                                   const BkModelRootTransform *root,
                                   BkPlaybackEffects *out, char error[256]) {
  return out ? step_effects(p, requested_slot, mode, seconds, root, out, error)
             : fail(error, "missing effect output");
}
int bk_model_playback_advance_frame(BkModelPlayback *p,
                                    const BkModelRootTransform *root,
                                    BkClipSample *out, char error[256]) {
  if (!p || !out || !bk_clip_player_copy(p->pending, p->player))
    return fail(error, "invalid frame instance/sample");
  BkClipSample clip;
  if (!bk_clip_advance_frame(p->pending, &clip, error) ||
      !commit_sample(p, clip, root, error))
    return 0;
  *out = clip;
  return 1;
}
int bk_model_playback_hold_mode(BkModelPlayback *p, int requested_slot,
                                BkClipRequestMode mode,
                                const BkModelRootTransform *root,
                                char error[256]) {
  if (requested_slot < -1 || requested_slot >= 128 ||
      (mode != BK_CLIP_REQUEST_TEN_TICKS && mode != BK_CLIP_REQUEST_CONFIGURED))
    return fail(error, "invalid requested slot");
  if (!p || !bk_clip_player_copy(p->pending, p->player))
    return fail(error, "invalid instance state");
  if (requested_slot >= 0 &&
      !bk_clip_request_mode(p->pending, (unsigned)requested_slot, mode, error))
    return 0;
  if (!bk_model_playback_place(p, root, error))
    return 0;
  BkClipPlayer *old = p->player;
  p->player = p->pending;
  p->pending = old;
  return 1;
}
int bk_model_playback_place(BkModelPlayback *p,
                            const BkModelRootTransform *root, char error[256]) {
  if (!p)
    return fail(error, "missing instance");
  size_t count = (size_t)p->model->frame_count * 16;
  int ok =
      bk_model_animation_update(p->animation, NULL, root, p->local,
                                p->next_world, p->next_local, count, error);
  if (!ok)
    return 0;
  float *previous = p->world;
  p->world = p->next_world;
  p->next_world = previous;
  previous = p->local;
  p->local = p->next_local;
  p->next_local = previous;
  return 1;
}
const float *bk_model_playback_frame(const BkModelPlayback *p, uint32_t frame) {
  return p && frame < p->model->frame_count ? p->world + frame * 16 : NULL;
}
const float *bk_model_playback_local(const BkModelPlayback *p, uint32_t frame) {
  return p && frame < p->model->frame_count ? p->local + frame * 16 : NULL;
}
int bk_model_playback_state(const BkModelPlayback *p, BkClipState *state) {
  return p && bk_clip_state(p->player, state);
}
int bk_model_playback_timing(const BkModelPlayback *p, unsigned slot,
                             BkClipTiming *timing) {
  return p && bk_clip_timing(p->player, slot, timing);
}
int bk_model_playback_loops(const BkModelPlayback *p, unsigned slot,
                            int32_t *loops) {
  return p && bk_clip_loops(p->player, slot, loops);
}

int bk_model_playback_edit_locals(BkModelPlayback *p,
                                  const BkModelLocalEdit *edits, size_t count,
                                  char error[256]) {
  if (!p || (count && !edits))
    return fail(error, "invalid local edits");
  if (!count)
    return 1;
  size_t floats = (size_t)p->model->frame_count * 16;
  memcpy(p->next_local, p->local, floats * sizeof(float));
  for (size_t i = 0; i < count; i++) {
    if (edits[i].frame >= p->model->frame_count ||
        p->model->frames[edits[i].frame].parent_index == BK_MODEL_NONE)
      return fail(error, "local edit must target a non-root frame");
    for (unsigned j = 0; j < 16; j++)
      if (!isfinite(edits[i].local[j]))
        return fail(error, "nonfinite local edit");
    memcpy(p->next_local + edits[i].frame * 16, edits[i].local, 64);
  }
  if (!bk_model_pose_world_matrices(p->model, p->next_local, p->next_world,
                                    floats, error))
    return 0;
  float *old = p->local;
  p->local = p->next_local;
  p->next_local = old;
  old = p->world;
  p->world = p->next_world;
  p->next_world = old;
  return 1;
}
