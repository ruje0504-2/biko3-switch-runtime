#include "scene/ending_opening.h"
#include <stdio.h>
#include <string.h>
typedef struct {
  const BkEndingOpeningScene *scene;
  const BkEndingOpeningBindings *bindings;
  float seconds;
} Context;
static int fail(char e[256], const char *why) {
  if (e)
    snprintf(e, 256, "ending opening scene: %s", why);
  return 0;
}
static uint32_t node(const BkEndingOpeningScene *s, unsigned index) {
  uint32_t frame = bk_ending_normal_assets_node(s->assets, index);
  return frame == BK_MODEL_NONE
             ? BK_FRAME_NONE
             : bk_actor_forest_node(bk_ending_normal_assets_forest(s->assets),
                                      0, frame);
}
static int camera(void *p, BkEndingOpeningCamera kind, int32_t choice,
                   const uint32_t words[3], uint32_t extra, uint32_t *result,
                   char e[256]) {
  Context *c = p;
  const BkEndingOpeningScene *s = c->scene;
  const BkEndingOpeningBindings *b = c->bindings;
  const uint32_t tracks[2] = {2, 3};
  BkActorForest *forest = bk_ending_normal_assets_forest(s->assets);
  BkEndingCameraAssets *assets = bk_ending_normal_assets_cameras(s->assets);
  int complete = 0, ok;
  if (kind == BK_ENDING_OPENING_TRACK) {
    uint32_t target = node(s, 1);
    if (target == BK_FRAME_NONE)
      return fail(e, "actual opening target is unavailable");
    ok = bk_ending_camera_assets_opening(assets, forest, tracks, b->camera,
                                        target, c->seconds, &s->input,
                                        &complete, e);
  } else if (kind == BK_ENDING_OPENING_PRESET) {
    float offset[3];
    memcpy(offset, words, sizeof(offset));
    BkEndingCameraPresetGate gate = {
        (uint8_t)*b->previous_flow, b->frame->phase, *s->selected,
        b->frame->state_721ee0, b->frame->state_721ee4,
        b->control->state_721eec, b->auxiliary->gate, *b->next_mode};
    /*4e0ecb's sixth argument is unused; retain that actual ABI behavior. */
    (void)extra;
    ok = bk_ending_camera_assets_preset(
        assets, forest, tracks, b->camera, s->transitions, s->presets,
        BK_ENDING_PRESET, (unsigned)choice, offset, &gate, 0x10, c->seconds,
        &complete, e);
  } else
    return fail(e, "unknown opening camera service");
  if (ok)
    *result = (uint32_t)complete;
  return ok;
}
static int target(void *p, float out[3], char e[256]) {
  const BkEndingOpeningScene *s = ((Context *)p)->scene;
  uint32_t target_node = node(s, 5);
  const float *world = bk_actor_forest_world(
      bk_ending_normal_assets_forest(s->assets), target_node);
  if (target_node == BK_FRAME_NONE || !world)
    return fail(e, "actual old721f08 target is unavailable");
  memcpy(out, world + 12, 3 * sizeof(*out));
  return 1;
}
static int present(void *p, unsigned slot, int *out, char e[256]) {
  return bk_ending_audio_present(((Context *)p)->scene->audio, slot, out)
             ? 1 : fail(e, "speech presence query failed");
}
static int status(void *p, unsigned slot, int *out, char e[256]) {
  Context *c = p;
  const BkEndingOpeningBindings *b = c->bindings;
  BkEndingAudioCall call = {.operation = BK_ENDING_AUDIO_STATUS, .slot = slot};
  return bk_ending_audio_call(c->scene->audio, b->frame->group,
                               b->auxiliary->variant, b->auxiliary->selection,
                               &call, out, e);
}
static int load(void *p, unsigned slot, const char *name, char e[256]) {
  return bk_ending_audio_load_speech(((Context *)p)->scene->audio, slot,
                                      name, e);
}
static int play(void *p, unsigned slot, int32_t volume, char e[256]) {
  Context *c = p;
  const BkEndingOpeningBindings *b = c->bindings;
  BkEndingAudioCall call = {.operation = BK_ENDING_AUDIO_RESTART,
                           .slot = slot, .volume = volume};
  int ignored = 0;
  return bk_ending_audio_call(c->scene->audio, b->frame->group,
                               b->auxiliary->variant, b->auxiliary->selection,
                               &call, &ignored, e);
}
static int voice_name(void *p, char name[32], char e[256]) {
  Context *c = p;
  return bk_ending_sound_contact_voice(c->bindings->frame->group, 0, 0, 0,
                                        name, e);
}
int bk_ending_opening_scene_step(const BkEndingOpeningScene *s,
                                  const BkEndingOpeningBindings *b,
                                  float seconds, char e[256]) {
  if (!s || !s->assets || !s->audio || !s->transitions || !s->presets ||
      !s->selected)
    return fail(e, "missing actual opening scene owners");
  Context context = {s, b, seconds};
  BkEndingOpeningOps ops = {&context, camera, target, present, status,
                            load, play, voice_name};
  return bk_ending_opening_step(b, seconds, &ops, e);
}
