#include "scene/ending_secondary_controller.h"
#include <limits.h>
#include <stdio.h>
#include <string.h>

typedef struct {
  const BkEndingSecondaryControllerScene *scene;
  float seconds;
} Context;

static int fail(char e[256], const char *why) {
  if (e)
    snprintf(e, 256, "secondary controller scene: %s", why);
  return 0;
}

static BkActorPose *primary(const BkEndingSecondaryControllerScene *s) {
  return bk_ending_secondary_assets_pose(s->assets, 0);
}

static int key(void *p, unsigned code, unsigned mode, uint32_t *out,
                 char e[256]) {
  const BkEndingSecondaryControllerScene *s = ((Context *)p)->scene;
  return s->key ? s->key(s->input_context, code, mode, out, e)
                 : fail(e, "missing physical key input");
}

static int present(void *p, unsigned slot, int *out, char e[256]) {
  return bk_ending_audio_present(((Context *)p)->scene->audio, slot, out)
      ? 1 : fail(e, "invalid real speech presence query");
}

static int audio(Context *c, const BkEndingAudioCall *call, int *out,
                   char e[256]) {
  const BkEndingState *v = c->scene->state;
  return bk_ending_audio_call(c->scene->audio, v->frame.group,
                               v->auxiliary.variant, v->auxiliary.selection,
                               call, out, e);
}

static int status(void *p, unsigned slot, int *out, char e[256]) {
  return audio(p, &(BkEndingAudioCall){.operation = BK_ENDING_AUDIO_STATUS,
                                      .slot = slot}, out, e);
}

static int load(void *p, unsigned slot, const char *name, char e[256]) {
  return bk_ending_audio_load_speech(((Context *)p)->scene->audio, slot, name, e);
}

static int play(void *p, unsigned slot, int32_t flags, int32_t volume,
                  char e[256]) {
  int ignored;
  return audio(p, &(BkEndingAudioCall){.operation = BK_ENDING_AUDIO_RESTART,
      .slot = slot, .flags = flags, .volume = volume}, &ignored, e);
}

static int voice(void *p, unsigned cue, unsigned slot, int32_t flags,
                   char e[256]) {
  Context *c = p;
  const BkEndingSecondaryControllerScene *s = c->scene;
  if (cue > INT32_MAX)
    return fail(e, "speech cue outside signed native range");
  BkEndingSecondarySpeechOps ops = {c, load, play};
  return bk_ending_secondary_speech(s->state->frame.group, (int32_t)cue, slot,
      (uint32_t)flags, s->state->speech_names, s->voice_volume, &ops, e);
}

static int eyes(void *p, unsigned slot, char e[256]) {
  return bk_eye_assets_select(bk_ending_secondary_assets_eyes(
      ((Context *)p)->scene->assets), slot, e);
}

static int request(void *p, int32_t clip, char e[256]) {
  return clip >= 0 ? bk_actor_pose_request_mode(primary(((Context *)p)->scene),
      (unsigned)clip, BK_CLIP_REQUEST_CONFIGURED, e)
      : fail(e, "negative configured clip request");
}

static int restart(void *p, int32_t clip, char e[256]) {
  return clip >= 0 ? bk_actor_pose_select(primary(((Context *)p)->scene),
                                           (unsigned)clip, 0, e)
                    : fail(e, "negative forced clip selection");
}

static int active(void *p, int32_t *out, char e[256]) {
  BkClipState state;
  if (!out || !bk_actor_pose_state(primary(((Context *)p)->scene), &state))
    return fail(e, "missing actual active clip");
  *out = state.slot;
  return 1;
}

static int timing(void *p, int32_t clip, BkEndingSecondaryControlTiming *out,
                    char e[256]) {
  BkClipTiming value;
  if (!out || clip < 0 || !bk_actor_pose_timing(primary(((Context *)p)->scene),
                                                (unsigned)clip, &value))
    return fail(e, "invalid actual clip timing");
  *out = (BkEndingSecondaryControlTiming){value.end, value.source};
  return 1;
}

static int target(void *p, float out[3], char e[256]) {
  BkEndingSecondaryAssets *assets = ((Context *)p)->scene->assets;
  uint32_t frame = bk_ending_secondary_assets_node(assets, 0);
  BkActorForest *forest = bk_ending_secondary_assets_forest(assets);
  uint32_t node = frame == BK_MODEL_NONE ? BK_FRAME_NONE
      : bk_actor_forest_node(forest, 0, frame);
  const float *world = bk_actor_forest_world(forest, node);
  if (!world)
    return fail(e, "missing old published node0 target");
  memcpy(out, world + 12, 3 * sizeof(*out));
  return 1;
}

static int camera(void *p, BkEndingOpeningCamera kind, int32_t choice,
                   const uint32_t words[3], uint32_t extra, uint32_t *result,
                   char e[256]) {
  Context *c = p;
  const BkEndingSecondaryControllerScene *s = c->scene;
  BkEndingState *v = s->state;
  BkActorForest *forest = bk_ending_secondary_assets_forest(s->assets);
  BkEndingCameraAssets *assets = bk_ending_secondary_assets_cameras(s->assets);
  const uint32_t tracks[] = {1, 2};
  int complete = 0, ok;
  if (kind == BK_ENDING_OPENING_TRACK) {
    uint32_t target_node = v->retained.normal.follow_target;
    if (!target_node || !bk_actor_forest_world(forest, target_node))
      return fail(e, "missing actual719448 opening target");
    BkEndingCameraOpeningInput input = {c, key};
    ok = bk_ending_camera_assets_opening(assets, forest, tracks, s->camera,
        target_node, c->seconds, &input, &complete, e);
  } else if (kind == BK_ENDING_OPENING_PRESET) {
    float offset[3];
    memcpy(offset, words, sizeof(offset));
    BkEndingCameraPresetGate gate = {(uint8_t)*s->previous_flow,
        v->frame.phase, v->selected, v->frame.state_721ee0,
        v->frame.state_721ee4, v->control.state_721eec,
        v->auxiliary.gate, v->next_mode};
    /*4e0ecb ignores its sixth argument. Do not reinterpret it as a target. */
    (void)extra;
    ok = bk_ending_camera_assets_preset(assets, forest, tracks, s->camera,
        s->transitions, s->presets, BK_ENDING_PRESET, (unsigned)choice,
        offset, &gate, 0x10, c->seconds, &complete, e);
  } else {
    return fail(e, "unknown original camera service");
  }
  if (ok)
    *result = (uint32_t)complete;
  return ok;
}

static int pick(void *p, const float pointer[2], int32_t preferred,
                  int32_t *result, char e[256]) {
  const BkEndingSecondaryControllerScene *s = ((Context *)p)->scene;
  BkEndingState *v = s->state;
  uint32_t node = (uint32_t)v->retained.normal.word_719b40;
  const float *alternate = node ? bk_actor_forest_world(
      bk_ending_secondary_assets_forest(s->assets), node) : NULL;
  if (node && !alternate)
    return fail(e, "retired or invalid719b40 target binding");
  BkEndingSecondaryPickBindings b = {&v->frame, s->action_kind,
      s->action_column, v->targets, v->alternate, &s->ui->sprites[50],
      s->geometry, alternate};
  return bk_ending_secondary_pick(&b, pointer, preferred, result, e);
}

static BkEndingSecondaryMenuGeometry menu(const BkEndingSecondaryControllerScene *s) {
  return (BkEndingSecondaryMenuGeometry){s->width, s->height,
      (float)((double)s->width / 1280.0), s->ui->sprites[51].rect[2]};
}

static int choose(void *p, const int32_t point[2], int32_t *result, char e[256]) {
  BkEndingSecondaryMenuGeometry g = menu(((Context *)p)->scene);
  return bk_ending_secondary_menu_zone(&g, point, result, e);
}

static int action(void *p, char e[256]) {
  const BkEndingSecondaryControllerScene *s = ((Context *)p)->scene;
  BkEndingState *v = s->state;
  BkEndingSecondaryMenuGeometry g = menu(s);
  return bk_ending_secondary_menu(&g, v->frame.camera_event,
      s->retained->automatic, v->frame.camera_cached, v->targets,
      v->alternate, v->choices, v->points, e);
}

int bk_ending_secondary_controller_scene_step(
    const BkEndingSecondaryControllerScene *s, const BkEndingFrameInput *input,
    float seconds, char e[256]) {
  if (!s || !s->assets || !s->audio || !s->state || !s->retained || !s->rate ||
      !s->action_kind || !s->action_column || !s->camera || !s->transitions ||
      !s->presets || !s->ui || !s->geometry || !s->width || !s->height ||
      !s->previous_flow || !s->voice_volume || !s->random)
    return fail(e, "missing actual secondary controller owners");
  BkEndingState *v = s->state;
  BkEndingSecondaryControlBindings b = {
      &v->frame, &v->control, &v->auxiliary, s->camera,
      &v->retained.stage3.byte_6bbe34, v->retained.stage3.words_6bbe2c,
      s->rate, &v->next_mode, s->previous_flow, &v->open, v->targets,
      v->points, v->choices, v->alternate, &s->ui->sprites[51].rect[2], s->random};
  Context context = {s, seconds};
  BkEndingSecondaryControlOps ops = {&context, key, present, status, voice, eyes,
      request, restart, active, timing, target, camera, pick, choose, action};
  return bk_ending_secondary_control_step(s->retained, &b, input, seconds, &ops, e);
}
