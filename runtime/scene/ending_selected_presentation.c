#include "scene/ending_selected_presentation.h"
#include "core/random.h"
#include <stdio.h>

typedef struct {
  const BkEndingSelectedPresentationScene *scene;
  BkEndingState *state;
  float seconds;
} Context;
static int fail(char e[256], const char *why) {
  if (e) snprintf(e, 256, "selected presentation scene: %s", why);
  return 0;
}
static BkActorPose *primary(Context *c) {
  return bk_ending_selected_assets_pose(c->scene->assets, 0);
}
static BkFaceState *face(Context *c) {
  return bk_ending_selected_assets_face_state(c->scene->assets);
}
static int clock_read(void *p, uint32_t *out, char e[256]) {
  const BkEndingSelectedPresentationScene *s = ((Context *)p)->scene;
  return s->clock(s->clock_context, out, e);
}
static int active(void *p, int32_t *out, char e[256]) {
  BkClipState state;
  if (!out || !bk_actor_pose_state(primary(p), &state))
    return fail(e, "missing actual primary clip");
  *out = state.slot;
  return 1;
}
static int prediction(void *p, unsigned slot, BkEndingAuxiliaryPrediction *out,
                         char e[256]) {
  BkClipPrediction state;
  if (!out || !bk_actor_pose_prediction(primary(p), slot, &state))
    return fail(e, "missing actual primary prediction");
  *out = (BkEndingAuxiliaryPrediction){state.duration, state.end,
                                        state.source, state.rate};
  return 1;
}
static int duration(void *p, unsigned slot, int32_t *out, char e[256]) {
  BkClipPrediction state;
  if (!out || !bk_actor_pose_prediction(primary(p), slot, &state))
    return fail(e, "missing actual primary duration");
  *out = state.duration;
  return 1;
}
static int audio(void *p, const BkEndingAudioCall *call, int *out, char e[256]) {
  Context *c = p;
  return bk_ending_audio_call(c->scene->audio, c->state->frame.group,
      c->state->auxiliary.variant, c->state->auxiliary.selection, call, out, e);
}
static int random_value(void *p, int32_t *out, char e[256]) {
  if (!out) return fail(e, "missing RNG destination");
  *out = bk_random_next(((Context *)p)->scene->random);
  return 1;
}
static int auxiliary_tick(void *p, char e[256]) {
  Context *c = p;
  const BkEndingSelectedPresentationScene *s = c->scene;
  BkEndingSelectedCycleBindings b = {
      &c->state->frame, &c->state->auxiliary, &c->state->control.variant,
      s->cycle, &s->shared_cycle->scale,
      &c->state->retained.auxiliary.word_5546a0,
      s->voice_volume, s->effect_volume};
  BkEndingSelectedCycleOps ops = {p, active, prediction, audio, random_value};
  return bk_ending_selected_cycle_step(&b, c->seconds, &ops, e);
}
static int advance(void *p, BkEndingPresentationActor actor, float dt, char e[256]) {
  Context *c = p;
  if (actor != BK_ENDING_PRESENT_PRIMARY && actor != BK_ENDING_PRESENT_BACKGROUND)
    return fail(e, "selected topology has no auxiliary actor");
  return bk_ending_selected_assets_advance(c->scene->assets,
      actor == BK_ENDING_PRESENT_PRIMARY ? 0 : 3, dt, e);
}
static int controlled(void *p, BkClipPlainMode mode, char e[256]) {
  return bk_ending_selected_assets_advance_plain(
      ((Context *)p)->scene->assets, 0, mode, e);
}
static int find_node(void *p, uint32_t root, const char *name, uint32_t *out,
                        char e[256]) {
  Context *c = p;
  uint32_t found;
  if (!out || !bk_actor_forest_find(bk_ending_selected_assets_forest(
                                     c->scene->assets), root, name, &found, e))
    return 0;
  *out = found == BK_FRAME_NONE ? 0 : found;
  return 1;
}
static int hide(void *p, uint32_t node, uint32_t hidden, char e[256]) {
  return !node || bk_actor_forest_visibility(bk_ending_selected_assets_forest(
      ((Context *)p)->scene->assets), node, hidden, e);
}
static int material(void *p, const char *name, uint32_t hidden, float alpha,
                       char e[256]) {
  return bk_ending_selected_assets_material_alpha(
      ((Context *)p)->scene->assets, name, hidden, alpha, e);
}
static int publish(void *p, char e[256]) {
  return bk_actor_forest_refresh(bk_ending_selected_assets_forest(
      ((Context *)p)->scene->assets), e);
}
static int expression(void *p, int32_t value, char e[256]) {
  BkFaceState *state = face(p);
  uint32_t now = 0;
  if (state->expression != value && !clock_read(p, &now, e)) return 0;
  return bk_face_request(state, value, now, e);
}
static int eye_range(void *p, float minimum, float maximum, char e[256]) {
  return bk_face_eye_range(face(p), minimum, maximum,
      ((Context *)p)->scene->random, e);
}
static int gaze(void *p, float minimum, float maximum, char e[256]) {
  Context *c = p;
  BkEyeAssets *eyes = bk_ending_selected_assets_eyes(c->scene->assets);
  const BkEyeBinding *binding = bk_eye_assets_binding(eyes);
  if (!binding) return fail(e, "missing actual eye binding");
  return bk_actor_pose_eyes(primary(c), binding->frames,
      bk_actor_forest_world(bk_ending_selected_assets_forest(c->scene->assets), 1),
      binding->texture_mode, bk_eye_assets_gaze_variant(eyes), minimum, maximum, e);
}
static int blink(void *p, uint32_t timestamp, char e[256]) {
  Context *c = p;
  BkFaceCommands commands;
  uint32_t now;
  return clock_read(p, &now, e) &&
      bk_face_blink(face(c), timestamp, now, c->scene->random, &commands, e) &&
      bk_face_assets_apply(bk_ending_selected_assets_face(c->scene->assets),
                             &commands, e);
}
static int level(void *p, float *out, char e[256]) {
  const BkEndingSelectedPresentationScene *s = ((Context *)p)->scene;
  return bk_ending_audio_level(s->audio, 0, s->voice, out, e);
}
static int mouth(void *p, float value, uint32_t timestamp, char e[256]) {
  Context *c = p;
  BkFaceCommands commands;
  uint32_t now;
  (void)timestamp; /*410c8e samples its own clock, not the blink timestamp.*/
  return clock_read(p, &now, e) && bk_face_mouth(face(c), value, now, &commands, e) &&
      bk_face_assets_apply(bk_ending_selected_assets_face(c->scene->assets),
                             &commands, e);
}
int bk_ending_selected_presentation_scene_step(
    const BkEndingSelectedPresentationScene *s, BkEndingState *v,
    float seconds, char e[256]) {
  if (!s || !v || !s->assets || !s->audio || !s->voice || !s->controller ||
      !s->cycle || !s->shared_cycle || !s->random || !s->plain_scheduled ||
      !s->voice_volume || !s->effect_volume || !s->secondary_node || !s->clock)
    return fail(e, "missing actual presentation owners");
  BkActorForest *forest = bk_ending_selected_assets_forest(s->assets);
  uint32_t primary_root = bk_ending_selected_assets_root(s->assets, 0);
  uint32_t background_root = bk_ending_selected_assets_root(s->assets, 3);
  if (!forest || primary_root == BK_FRAME_NONE || background_root == BK_FRAME_NONE ||
      (*s->secondary_node && !bk_actor_forest_world(forest, *s->secondary_node)))
    return fail(e, "missing background or stale secondary node");
  uint32_t hidden[3];
  for (unsigned i = 0; i < 3; ++i) {
    uint32_t frame = bk_ending_selected_assets_visible_node(s->assets, i);
    hidden[i] = frame == BK_MODEL_NONE ? 0 : bk_actor_forest_node(forest, 0, frame);
    if (hidden[i] == BK_FRAME_NONE) return fail(e, "stale hidden-node binding");
  }
  BkEndingSelectedPresentationBindings b = {
      &v->frame, &v->auxiliary, bk_ending_selected_assets_face_state(s->assets),
      &v->ui_controller.auxiliary.mode, s->plain_scheduled, &v->face_mode,
      &s->controller->expression_override, &v->eye_lower,
      &v->retained.auxiliary.word_6ea354, &s->shared_cycle->scale,
      v->control.toggles, &primary_root, &background_root, hidden, s->secondary_node};
  Context context = {s, v, seconds};
  BkEndingSelectedPresentationOps ops = {&context, clock_read, advance, find_node,
      hide, auxiliary_tick, active, duration, controlled, material, publish,
      expression, eye_range, gaze, blink, level, mouth};
  return bk_ending_selected_presentation_step(&b, seconds, &ops, e);
}
