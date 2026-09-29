#include "scene/ending_tertiary_controller.h"
#include "scene/ending_target.h"
#include "scene/ending_ui_geometry.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

typedef struct {
  const BkEndingTertiaryControllerScene *scene;
  const BkEndingTertiaryActionBindings *bindings;
  BkEndingTertiaryActionOps ops;
  float seconds;
} Context;
static int fail(char e[256], const char *why) {
  if (e) snprintf(e, 256, "tertiary controller scene: %s", why);
  return 0;
}
void bk_ending_tertiary_controller_initialize(BkEndingTertiaryControllerRetained *s) {
  if (!s) return;
  s->control = bk_ending_tertiary_control_initial();
  s->action = (BkEndingTertiaryActionState){0, 30};
  s->motion = bk_ending_tertiary_motion_initial();
}
static BkActorPose *actor(Context *c, unsigned role) {
  return role < 3 ? bk_ending_tertiary_assets_pose(c->scene->assets, role) : NULL;
}
static int key(void *p, unsigned code, unsigned mode, uint32_t *out, char e[256]) {
  const BkEndingTertiaryControllerScene *s = ((Context *)p)->scene;
  return s->key ? s->key(s->input_context, code, mode, out, e)
                : fail(e, "missing actual key input");
}
static int present(void *p, unsigned slot, int *out, char e[256]) {
  return bk_ending_audio_present(((Context *)p)->scene->audio, slot, out)
      ? 1 : fail(e, "invalid speech presence query");
}
static int audio(Context *c, const BkEndingAudioCall *call, int *out, char e[256]) {
  const BkEndingTertiaryControlBindings *b = &c->bindings->control;
  return bk_ending_audio_call(c->scene->audio, b->frame->group,
      b->auxiliary->variant, b->auxiliary->selection, call, out, e);
}
static int status(void *p, unsigned slot, int *out, char e[256]) {
  return audio(p, &(BkEndingAudioCall){.operation = BK_ENDING_AUDIO_STATUS,
                                       .slot = slot}, out, e);
}
static int voice(void *p, int32_t cue, unsigned slot, int32_t bank,
                   int32_t select, char e[256]) {
  Context *c = p;
  char name[32];
  (void)bank; /*479739 never reads this argument.*/
  if (slot >= 2 || !bk_ending_sound_tertiary_voice(
      c->bindings->control.frame->group, cue, select, name, e))
    return slot >= 2 ? fail(e, "speech name slot outside live table") : 0;
  memcpy(c->scene->speech_names[slot], name, strlen(name) + 1);
  return bk_ending_audio_load_speech(c->scene->audio, slot, name, e);
}
static int play(void *p, unsigned slot, int32_t volume, char e[256]) {
  int ignored;
  return audio(p, &(BkEndingAudioCall){.operation = BK_ENDING_AUDIO_RESTART,
      .slot = slot, .flags = 0, .volume = volume}, &ignored, e);
}
static int effect(void *p, int32_t volume, char e[256]) {
  /*725034 is buffer40 of the SAME722334 bank, not an extra audio owner.*/
  return play(p, 40, volume, e);
}
static int effect_slot(void *p, unsigned slot, int32_t volume, char e[256]) {
  return slot < 46 ? play(p, slot + 2, volume, e)
                    : fail(e, "effect slot outside shared48-buffer bank");
}
static int expression(void *p, int32_t a, int32_t b, int32_t mode, char e[256]) {
  Context *c = p;
  c->bindings->control.auxiliary->expression_a = a;
  c->bindings->control.auxiliary->expression_b = b;
  /*4dfb96 does not evaluate face expressions at this boundary.*/
  return mode >= 0 ? bk_eye_assets_select(
      bk_ending_tertiary_assets_eyes(c->scene->assets), (unsigned)mode, e)
      : fail(e, "negative eye texture mode");
}
static int active(void *p, int32_t *out, char e[256]) {
  BkClipState value;
  if (!out || !bk_actor_pose_state(actor(p, 0), &value))
    return fail(e, "missing primary active clip");
  *out = value.slot;
  return 1;
}
static int source(void *p, unsigned slot, float *out, char e[256]) {
  BkClipTiming value;
  if (!out || !bk_actor_pose_timing(actor(p, 0), slot, &value))
    return fail(e, "missing primary source clock");
  *out = value.source;
  return 1;
}
static int request(void *p, unsigned role, int32_t slot, char e[256]) {
  BkActorPose *pose = actor(p, role);
  return pose && slot >= 0
      ? bk_actor_pose_request_mode(pose, (unsigned)slot, BK_CLIP_REQUEST_CONFIGURED, e)
      : fail(e, "missing requested actor/clip");
}
static int hidden(void *p, unsigned role, int flag, char e[256]) {
  Context *c = p;
  uint32_t root = role < 3
      ? bk_ending_tertiary_assets_root(c->scene->assets, role) : BK_FRAME_NONE;
  return root != BK_FRAME_NONE ? bk_actor_forest_visibility(
      bk_ending_tertiary_assets_forest(c->scene->assets), root, (uint32_t)flag, e)
      : fail(e, "missing requested actor root");
}
static int set_material(Context *c, BkEndingTertiaryMaterialTable table,
                          unsigned group, unsigned slot, int hidden,
                          float alpha, char e[256]) {
  const char *name = bk_ending_tertiary_material_name(table, group, slot);
  return name ? bk_ending_tertiary_assets_material_alpha(
      c->scene->assets, name, (uint32_t)hidden, alpha, e)
      : fail(e, "material selector outside original table");
}
static int appearance(void *p, BkEndingTertiaryAppearance table,
                        unsigned group, unsigned slot, int hidden, char e[256]) {
  BkEndingTertiaryMaterialTable mapped;
  switch (table) {
  case BK_ENDING_TERTIARY_APPEARANCE_PAIR: mapped = BK_ENDING_TERTIARY_MATERIAL_PAIR; break;
  case BK_ENDING_TERTIARY_APPEARANCE_SIX: mapped = BK_ENDING_TERTIARY_MATERIAL_SIX; break;
  default: return fail(e, "unknown appearance table");
  }
  return set_material(p, mapped, group, slot, hidden, 1, e);
}
static int material(void *p, BkEndingTertiaryActionMaterial table,
                      unsigned group, unsigned slot, int hidden, float alpha,
                      char e[256]) {
  BkEndingTertiaryMaterialTable mapped;
  switch (table) {
  case BK_ENDING_TERTIARY_ACTION_PAIR: mapped = BK_ENDING_TERTIARY_MATERIAL_PAIR; break;
  case BK_ENDING_TERTIARY_ACTION_SIX: mapped = BK_ENDING_TERTIARY_MATERIAL_SIX; break;
  case BK_ENDING_TERTIARY_ACTION_FOUR: mapped = BK_ENDING_TERTIARY_MATERIAL_FOUR; break;
  default: return fail(e, "unknown action material table");
  }
  return set_material(p, mapped, group, slot, hidden, alpha, e);
}
static int target(void *p, unsigned index, float out[3], char e[256]) {
  Context *c = p;
  uint32_t node = bk_ending_tertiary_assets_node(c->scene->assets, index);
  const float *world = node == BK_MODEL_NONE ? NULL
      : bk_actor_pose_frame(actor(c, 0), node);
  if (!out || !world) return fail(e, "missing old published target");
  memcpy(out, world + 12, 3 * sizeof(*out));
  return 1;
}
static int camera(void *p, BkEndingOpeningCamera kind, int32_t choice,
                    const uint32_t words[3], uint32_t extra, uint32_t *result,
                    char e[256]) {
  Context *c = p;
  const BkEndingTertiaryControllerScene *s = c->scene;
  const BkEndingTertiaryControlBindings *b = &c->bindings->control;
  BkActorForest *forest = bk_ending_tertiary_assets_forest(s->assets);
  BkEndingCameraAssets *assets = bk_ending_tertiary_assets_cameras(s->assets);
  uint32_t tracks[] = {
      bk_ending_tertiary_assets_registry(s->assets, BK_ENDING_TERTIARY_ASSET_TRACK0),
      bk_ending_tertiary_assets_registry(s->assets, BK_ENDING_TERTIARY_ASSET_TRACK1)};
  int complete = 0, ok;
  if (kind == BK_ENDING_OPENING_TRACK) {
    if (!*s->follow_target || !bk_actor_forest_world(forest, *s->follow_target))
      return fail(e, "missing live opening target");
    BkEndingCameraOpeningInput input = {c, key};
    ok = bk_ending_camera_assets_opening(assets, forest, tracks, b->camera,
        *s->follow_target, c->seconds, &input, &complete, e);
  } else if (kind == BK_ENDING_OPENING_PRESET) {
    float offset[3];
    memcpy(offset, words, sizeof(offset));
    BkEndingCameraPresetGate gate = {(uint8_t)*b->previous_flow,
        b->frame->phase, *s->selected, b->frame->state_721ee0,
        b->frame->state_721ee4, b->control->state_721eec,
        b->auxiliary->gate, *s->next_mode};
    (void)extra;
    ok = bk_ending_camera_assets_preset(assets, forest, tracks, b->camera,
        s->transitions, s->presets, BK_ENDING_PRESET, (unsigned)choice,
        offset, &gate, 0x10, c->seconds, &complete, e);
  } else return fail(e, "unknown camera operation");
  if (ok) *result = (uint32_t)complete;
  return ok;
}
static int pick(void *p, const float pointer[2], int32_t *out, char e[256]) {
  Context *c = p;
  const BkEndingTertiaryControllerScene *s = c->scene;
  const BkEndingTertiaryControlBindings *b = &c->bindings->control;
  BkClipState clip;
  BkNodeReference view;
  if (!bk_actor_pose_state(actor(c, 0), &clip) ||
      !bk_actor_forest_anchor_reference(bk_ending_tertiary_assets_forest(s->assets),
                                          1, &view, e))
    return fail(e, "missing held camera/active clip");
  BkEndingTargetBindings target = {b->frame, b->control, b->auxiliary,
      &clip.slot, bk_ending_tertiary_assets_config(s->assets)->actions,
      s->targets, s->action_kind, s->action_column,
      &s->ui->sprites[50], view.local, s->geometry};
  int result;
  if (!bk_ending_target_step(&target, pointer, &result, e)) return 0;
  *out = result;
  return 1;
}
static BkEndingSecondaryMenuGeometry menu(Context *c) {
  return (BkEndingSecondaryMenuGeometry){c->scene->width, c->scene->height,
      *c->bindings->scale, c->scene->ui->sprites[51].rect[2]};
}
static int choose(void *p, const int32_t point[2], int32_t *out, char e[256]) {
  BkEndingSecondaryMenuGeometry geometry = menu(p);
  return bk_ending_secondary_menu_zone(&geometry, point, out, e);
}
static int place_menu(void *p, int32_t angle, int32_t step,
                        const int32_t point[2], char e[256]) {
  Context *c = p;
  BkEndingSecondaryMenuGeometry geometry = menu(c);
  return bk_ending_radial_menu_place(&geometry, angle, step, point, c->scene->points, e);
}
static int hit(void *p, unsigned slot, const int32_t pointer[2], int *out, char e[256]) {
  Context *c = p;
  if (slot >= 3) return fail(e, "menu point outside live table");
  const int32_t *point = c->scene->points[slot];
  float center[] = {(float)point[0], (float)point[1]};
  float cursor[] = {(float)pointer[0], (float)pointer[1]}, distance = 0;
  float radius = (float)((double)c->scene->ui->sprites[51].rect[2] * .5);
  return bk_ending_ui_circle_hit(center, radius, cursor, out, &distance)
      ? 1 : fail(e, "invalid actual menu hit geometry");
}
static int motion_clip(BkActorPose *pose, BkClipState *state,
                         BkEndingTertiaryMotionClip *out, char e[256]) {
  BkClipTiming timing;
  BkClipPrediction prediction;
  if (!pose || !bk_actor_pose_state(pose, state) || state->slot < 0 ||
      !bk_actor_pose_timing(pose, (unsigned)state->slot, &timing) ||
      !bk_actor_pose_prediction(pose, (unsigned)state->slot, &prediction))
    return fail(e, "missing actual controlled clip");
  *out = (BkEndingTertiaryMotionClip){prediction.duration,
      timing.start, timing.end, timing.source, prediction.rate, state->elapsed};
  return 1;
}
static int manual(void *p, const BkEndingFrameInput *input,
                     const int32_t target[2], char e[256]) {
  Context *c = p;
  BkActorPose *pose = actor(c, 0);
  BkClipState state;
  BkEndingTertiaryMotionClip clip;
  int32_t pointer[2];
  memcpy(pointer, &input->words[9], sizeof(pointer));
  if (!motion_clip(pose, &state, &clip, e) ||
      !bk_ending_tertiary_motion_pointer(&clip, target, c->scene->points[0], pointer, e))
    return 0;
  BkClipEdit edit = {.slot = (unsigned)state.slot,
      .fields = BK_CLIP_EDIT_SOURCE, .source = clip.source};
  return bk_actor_pose_edit_clips(pose, &edit, 1, e);
}
static int drag(void *p, unsigned role, const uint32_t words[2],
                  unsigned mode, char e[256]) {
  Context *c = p;
  BkActorPose *pose = actor(c, role);
  BkClipState state;
  BkEndingTertiaryMotionClip clip;
  BkEndingTertiaryMotionState next = c->scene->retained->motion;
  int32_t motion[2];
  memcpy(motion, words, sizeof(motion));
  if (!motion_clip(pose, &state, &clip, e) ||
      !bk_ending_tertiary_motion_drag(&next, c->bindings->control.frame->group,
          mode, c->seconds, motion, &clip, e) ||
      !bk_actor_pose_set_clock(pose, (unsigned)state.slot, clip.elapsed, clip.source, e))
    return 0;
  c->scene->retained->motion = next;
  return 1;
}
static int rewind_source(void *p, unsigned role, unsigned slot, char e[256]) {
  BkActorPose *pose = actor(p, role);
  BkClipTiming timing;
  if (!pose || !bk_actor_pose_timing(pose, slot, &timing))
    return fail(e, "missing source rewind descriptor");
  BkClipEdit edit = {.slot = slot, .fields = BK_CLIP_EDIT_SOURCE, .source = timing.start};
  return bk_actor_pose_edit_clips(pose, &edit, 1, e);
}
static int refresh(void *p, unsigned role, char e[256]) {
  return bk_ending_tertiary_assets_advance_plain(((Context *)p)->scene->assets,
      role, 0, BK_CLIP_PLAIN_SCHEDULED, e);
}
static int begin(void *p, char e[256]) {
  Context *c = p;
  return bk_ending_tertiary_action_begin(c->bindings, &c->ops, e);
}
static int action(void *p, const BkEndingFrameInput *input, float seconds, char e[256]) {
  Context *c = p;
  return bk_ending_tertiary_action_step(&c->scene->retained->action,
      c->bindings, input, seconds, &c->ops, e);
}
int bk_ending_tertiary_controller_scene_step(
    const BkEndingTertiaryControllerScene *s,
    const BkEndingTertiaryActionBindings *b, const BkEndingFrameInput *input,
    float seconds, char e[256]) {
  if (!s || !b || !s->assets || !s->audio || !s->retained || !s->transitions ||
      !s->presets || !s->ui || !s->geometry || !s->width || !s->height ||
      !s->speech_names || !s->targets || !s->points || !s->action_kind ||
      !s->action_column || !s->follow_target || !s->selected || !s->next_mode ||
      !b->scale || !isfinite(*b->scale) || *b->scale <= 0 ||
      (const int32_t (*)[2])s->targets != b->control.targets)
    return fail(e, "incomplete or inconsistent actual controller owners");
  /*4dac12 scans709db8;476720/47811c/478eab compare709dcc, one row
   * later. Bind both views to the SAME resource table, never shift picking.*/
  BkEndingTertiaryActionBindings bindings = *b;
  bindings.control.actions = bk_ending_tertiary_assets_config(s->assets)->actions + 5;
  bindings.control.initial_targets = bk_ending_tertiary_initial_targets();
  Context context = {.scene = s, .bindings = &bindings, .seconds = seconds};
  context.ops = (BkEndingTertiaryActionOps){
      {&context, key, present, status, voice, play, effect, expression, active,
       source, request, hidden, appearance, target, camera, pick, choose, begin, action},
      effect_slot, material, manual, drag, hit, rewind_source, refresh, place_menu};
  return bk_ending_tertiary_control_step(&s->retained->control,
      &bindings.control, input, seconds, &context.ops.control, e);
}
