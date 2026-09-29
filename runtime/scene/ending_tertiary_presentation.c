#include "scene/ending_tertiary_presentation.h"
#include <limits.h>
#include <stdio.h>

static int fail(char e[256], const char *why) {
  if (e) snprintf(e, 256, "tertiary presentation scene: %s", why);
  return 0;
}
static int clock_read(void *p, uint32_t *out, char e[256]) {
  const BkEndingTertiaryPresentationScene *s = p;
  return s->clock(s->clock_context, out, e);
}
static int advance(void *p, BkEndingTertiaryActor actor, float dt, char e[256]) {
  const BkEndingTertiaryPresentationScene *s = p;
  return bk_ending_tertiary_assets_advance(s->assets,
      bk_ending_tertiary_assets_role(actor), dt, e);
}
static int active(void *p, BkEndingTertiaryActor actor, int32_t *out, char e[256]) {
  const BkEndingTertiaryPresentationScene *s = p;
  BkClipState state;
  if (!out || !bk_actor_pose_state(bk_ending_tertiary_assets_pose(s->assets,
      bk_ending_tertiary_assets_role(actor)), &state))
    return fail(e, "missing active actor clip");
  *out = state.slot;
  return 1;
}
static int hide(void *p, uint32_t node, uint32_t hidden, char e[256]) {
  const BkEndingTertiaryPresentationScene *s = p;
  return !node || bk_actor_forest_visibility(
      bk_ending_tertiary_assets_forest(s->assets), node, hidden, e);
}
static int disable(void *p, unsigned index, uint32_t disabled, char e[256]) {
  const BkEndingTertiaryPresentationScene *s = p;
  const BkBomDualBinding *binding = bk_bom_dual_assets_binding(
      bk_ending_tertiary_assets_bom(s->assets), index);
  if (!binding || binding->nodes.target == BK_MODEL_NONE) return 1;
  if (index >= s->bom_count) return fail(e, "missing live BOM enable cell");
  s->bom_disabled[index] = (int32_t)disabled;
  return 1;
}
static int controlled(void *p, BkEndingTertiaryActor actor,
                         BkEndingTertiaryControlledAdvance kind, char e[256]) {
  const BkEndingTertiaryPresentationScene *s = p;
  BkClipPlainMode mode;
  switch (kind) {
  case BK_ENDING_TERTIARY_4E18AD: mode = BK_CLIP_PLAIN_SOURCE; break;
  case BK_ENDING_TERTIARY_4A9019: mode = BK_CLIP_PLAIN_FORCE_CHAIN; break;
  default: return fail(e, "unknown controlled scheduler");
  }
  return bk_ending_tertiary_assets_advance_plain(s->assets,
      bk_ending_tertiary_assets_role(actor), 0, mode, e);
}
static int material(void *p, unsigned group, unsigned slot, uint32_t hidden,
                      float alpha, char e[256]) {
  const BkEndingTertiaryPresentationScene *s = p;
  const char *name = bk_ending_tertiary_material_name(
      BK_ENDING_TERTIARY_MATERIAL_FOUR, group, slot);
  return name ? bk_ending_tertiary_assets_material_alpha(s->assets, name, hidden, alpha, e)
              : fail(e, "material selector outside original table");
}
static int publish(void *p, char e[256]) {
  return bk_actor_forest_refresh(bk_ending_tertiary_assets_forest(
      ((const BkEndingTertiaryPresentationScene *)p)->assets), e);
}
static int follow(void *p, unsigned index, int direction, char e[256]) {
  const BkEndingTertiaryPresentationScene *s = p;
  BkBomDualAssets *bom = bk_ending_tertiary_assets_bom(s->assets);
  const BkBomDualBinding *binding = bk_bom_dual_assets_binding(bom, index);
  if (!binding || binding->child_actor < 1 || binding->child_actor > 2)
    return fail(e, "invalid actual BOM child owner");
  BkActorPose *child = bk_bom_dual_assets_actor(bom, binding->child_actor);
  const float *reference = bk_actor_pose_frame(bk_bom_dual_assets_actor(bom, 0),
                                                binding->nodes.reference);
  BkNodeReference node;
  if (!reference || !bk_actor_pose_node_reference(child, binding->nodes.child, &node, e))
    return fail(e, "missing actual BOM node/reference");
  int ok = direction
      ? bk_node_reference_orientation(&node, reference, (float[3]){0, 0, 1},
                                        (float[3]){0, 1, 0}, e)
      : bk_node_reference_position(&node, reference, (float[3]){0, 0, 0}, e);
  return ok && bk_actor_pose_commit_reference(child, binding->nodes.child, &node, e);
}
static int expression(void *p, int32_t value, char e[256]) {
  const BkEndingTertiaryPresentationScene *s = p;
  BkFaceState *state = bk_ending_tertiary_assets_face_state(s->assets);
  uint32_t now = 0;
  if (state->expression != value && !clock_read(p, &now, e)) return 0;
  return bk_face_request(state, value, now, e);
}
static int eye_range(void *p, float minimum, float maximum, char e[256]) {
  const BkEndingTertiaryPresentationScene *s = p;
  return bk_face_eye_range(bk_ending_tertiary_assets_face_state(s->assets),
                             minimum, maximum, s->random, e);
}
static int gaze(void *p, float minimum, float maximum, char e[256]) {
  const BkEndingTertiaryPresentationScene *s = p;
  BkEyeAssets *eyes = bk_ending_tertiary_assets_eyes(s->assets);
  const BkEyeBinding *binding = bk_eye_assets_binding(eyes);
  if (!binding) return fail(e, "missing actual eye binding");
  return bk_actor_pose_eyes(bk_ending_tertiary_assets_pose(s->assets,
      BK_ENDING_TERTIARY_ASSET_PRIMARY), binding->frames,
      bk_actor_forest_world(bk_ending_tertiary_assets_forest(s->assets), 1),
      binding->texture_mode, bk_eye_assets_gaze_variant(eyes), minimum, maximum, e);
}
static int blink(void *p, uint32_t timestamp, char e[256]) {
  const BkEndingTertiaryPresentationScene *s = p;
  BkFaceCommands commands;
  uint32_t now;
  return clock_read(p, &now, e) && bk_face_blink(
      bk_ending_tertiary_assets_face_state(s->assets), timestamp, now,
      s->random, &commands, e) &&
      bk_face_assets_apply(bk_ending_tertiary_assets_face(s->assets), &commands, e);
}
static int level(void *p, float *out, char e[256]) {
  const BkEndingTertiaryPresentationScene *s = p;
  return bk_ending_audio_level(s->audio, 0, s->voice, out, e);
}
static int mouth(void *p, float value, uint32_t timestamp, char e[256]) {
  const BkEndingTertiaryPresentationScene *s = p;
  BkFaceCommands commands;
  uint32_t now;
  (void)timestamp; /*410c8e samples its own clock, unlike the blink argument.*/
  return clock_read(p, &now, e) && bk_face_mouth(
      bk_ending_tertiary_assets_face_state(s->assets), value, now, &commands, e) &&
      bk_face_assets_apply(bk_ending_tertiary_assets_face(s->assets), &commands, e);
}
int bk_ending_tertiary_presentation_scene_step(
    const BkEndingTertiaryPresentationScene *s, BkEndingState *v,
    const int32_t *face_mode, int32_t *eye_lower, float seconds, char e[256]) {
  if (!s || !v || !s->assets || !s->audio || !s->voice || !s->random || !s->clock ||
      !face_mode || !eye_lower || (s->bom_count && !s->bom_disabled))
    return fail(e, "incomplete actual presentation owners");
  BkActorForest *forest = bk_ending_tertiary_assets_forest(s->assets);
  uint32_t primary = bk_ending_tertiary_assets_registry(s->assets,
      BK_ENDING_TERTIARY_ASSET_PRIMARY);
  uint32_t background = bk_ending_tertiary_assets_root(s->assets,
      BK_ENDING_TERTIARY_ASSET_BACKGROUND);
  uint32_t count = bk_bom_dual_assets_count(bk_ending_tertiary_assets_bom(s->assets));
  if (!forest || primary == BK_FRAME_NONE || background == BK_FRAME_NONE ||
      count > INT32_MAX || count != s->bom_count)
    return fail(e, "missing background or inconsistent binding ownership");
  uint32_t nodes[3], special = bk_ending_tertiary_assets_special(s->assets);
  for (unsigned i = 0; i < 3; ++i) {
    uint32_t node = bk_ending_tertiary_assets_visible_node(s->assets, i);
    nodes[i] = node == BK_MODEL_NONE ? 0 : bk_actor_forest_node(forest, primary, node);
    if (nodes[i] == BK_FRAME_NONE) return fail(e, "stale hidden-node binding");
  }
  special = special == BK_MODEL_NONE ? 0 : bk_actor_forest_node(forest, primary, special);
  if (special == BK_FRAME_NONE) return fail(e, "stale719b48 special node");
  int32_t bindings = (int32_t)count;
  BkEndingRetainedStage2 *retained = &v->retained.stage2;
  BkEndingTertiaryPresentationBindings b = {
      &v->frame, &v->auxiliary, bk_ending_tertiary_assets_face_state(s->assets),
      &v->stage3_state, face_mode, &retained->word_6a3c24, eye_lower,
      &retained->words_6afcfc[2], v->control.toggles, &background, nodes,
      &special, &bindings, count};
  BkEndingTertiaryPresentationOps ops = {(void *)s, clock_read, advance, active,
      hide, disable, controlled, material, publish, follow, expression,
      eye_range, gaze, blink, level, mouth};
  return bk_ending_tertiary_presentation_step(&b, seconds, &ops, e);
}
