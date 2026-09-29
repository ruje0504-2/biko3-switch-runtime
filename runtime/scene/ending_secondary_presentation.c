#include "scene/ending_secondary_presentation.h"
#include <stdio.h>
#include <string.h>
static int fail(char e[256], const char *why) {
  snprintf(e, 256, "secondary presentation scene: %s", why);
  return 0;
}
static int clock_read(void *p, uint32_t *out, char e[256]) {
  const BkEndingSecondaryPresentationScene *s = p;
  return s->clock(s->clock_context, out, e);
}
static int advance(void *p, BkEndingPresentationActor actor, float dt,
                   char e[256]) {
  const BkEndingSecondaryPresentationScene *s = p;
  if (actor != BK_ENDING_PRESENT_PRIMARY && actor != BK_ENDING_PRESENT_BACKGROUND)
    return fail(e, "no secondary actor in this topology");
  return bk_ending_secondary_assets_advance(
      s->assets, actor == BK_ENDING_PRESENT_PRIMARY ? 0 : 3, dt, e);
}
static int find_node(void *p, uint32_t root, const char *name,
                     uint32_t *node, char e[256]) {
  const BkEndingSecondaryPresentationScene *s = p;
  uint32_t found;
  if (!bk_actor_forest_find(bk_ending_secondary_assets_forest(s->assets),
                            root, name, &found, e)) return 0;
  *node = found == BK_FRAME_NONE ? 0 : found;
  return 1;
}
static int hide(void *p, uint32_t node, uint32_t hidden, char e[256]) {
  const BkEndingSecondaryPresentationScene *s = p;
  return !node || bk_actor_forest_visibility(
      bk_ending_secondary_assets_forest(s->assets), node, hidden, e);
}
static int material(void *p, const char *name, uint32_t hidden, float alpha,
                    char e[256]) {
  const BkEndingSecondaryPresentationScene *s = p;
  for (unsigned owner = 0; owner < 2; ++owner) {
    BkMaterialPose *materials = bk_ending_secondary_assets_materials(
        s->assets, owner ? 3 : 0);
    const BkModel *model = bk_actor_pose_model(
        bk_ending_secondary_assets_pose(s->assets, owner ? 3 : 0));
    if (!model || !materials) return fail(e, "missing actual material registry");
    for (uint32_t i = 0; i < model->material_count; ++i) {
      if (strcmp(model->materials[i].name, name)) continue;
      const BkModelMaterial *m = bk_material_pose_material(materials, i);
      if (!m || m->id != model->materials[i].id)
        return fail(e, "material instance identity changed");
      BkMaterialValuesEdit edit = {.index = i, .id = m->id};
      memcpy(edit.values.diffuse, m->diffuse, 16);
      memcpy(edit.values.ambient, m->ambient, 16);
      memcpy(edit.values.specular, m->specular, 16);
      memcpy(edit.values.emissive, m->emissive, 16);
      edit.values.power = m->power;
      edit.values.diffuse[3] = hidden ? 0 : alpha;
      return bk_material_pose_values(materials, &edit, 1, e);
    }
  }
  return 1; /*Original missing-name no-op, including the empty entry.*/
}
static int publish(void *p, char e[256]) {
  return bk_actor_forest_refresh(bk_ending_secondary_assets_forest(
      ((const BkEndingSecondaryPresentationScene *)p)->assets), e);
}
static int face(void *p, BkEndingPresentationFace kind, float value,
                int32_t expression, uint32_t timestamp, char e[256]) {
  const BkEndingSecondaryPresentationScene *s = p;
  BkFaceState *state = bk_ending_secondary_assets_face_state(s->assets);
  BkFaceAssets *assets = bk_ending_secondary_assets_face(s->assets);
  BkFaceCommands commands;
  uint32_t now;
  if (kind == BK_ENDING_PRESENT_EYE_RANGE)
    return bk_face_eye_range(state, 0, value, s->random, e);
  if (kind == BK_ENDING_PRESENT_EXPRESSION && state->expression == expression)
    return bk_face_request(state, expression, 0, e);
  if (!s->clock(s->clock_context, &now, e)) return 0;
  if (kind == BK_ENDING_PRESENT_EXPRESSION)
    return bk_face_request(state, expression, now, e);
  if (kind == BK_ENDING_PRESENT_BLINK) {
    if (!bk_face_blink(state, timestamp, now, s->random, &commands, e)) return 0;
  } else if (kind == BK_ENDING_PRESENT_MOUTH) {
    if (!bk_face_mouth(state, value, now, &commands, e)) return 0;
  } else return fail(e, "unknown face operation");
  return bk_face_assets_apply(assets, &commands, e);
}
static int level(void *p, float *out, char e[256]) {
  const BkEndingSecondaryPresentationScene *s = p;
  return bk_ending_audio_level(s->audio, 0, s->voice, out, e);
}
static int active(void *p, int32_t *out, char e[256]) {
  const BkEndingSecondaryPresentationScene *s = p;
  BkClipState state;
  if (!bk_actor_pose_state(bk_ending_secondary_assets_pose(s->assets, 0), &state))
    return fail(e, "missing primary clip state");
  *out = state.slot;
  return 1;
}
static int timing(void *p, unsigned slot, BkEndingSecondaryTiming *out,
                  char e[256]) {
  const BkEndingSecondaryPresentationScene *s = p;
  BkClipPrediction prediction;
  if (!bk_actor_pose_prediction(bk_ending_secondary_assets_pose(s->assets, 0),
                                 slot, &prediction))
    return fail(e, "invalid active clip timing");
  *out = (BkEndingSecondaryTiming){prediction.end, prediction.source, prediction.rate};
  return 1;
}
int bk_ending_secondary_presentation_scene_step(
    const BkEndingSecondaryPresentationScene *s, BkEndingFrameState *frame,
    BkEndingAuxiliaryState *auxiliary, const int32_t *automatic,
    const uint8_t toggles[8], float seconds, char e[256]) {
  if (!s || !s->assets || !s->audio ||
      !s->state || !s->voice || !s->random || !s->clock)
    return fail(e, "incomplete actual scene owners");
  BkActorForest *forest = bk_ending_secondary_assets_forest(s->assets);
  uint32_t primary = bk_ending_secondary_assets_root(s->assets, 0);
  uint32_t background = bk_ending_secondary_assets_root(s->assets, 3);
  if (!forest || primary == BK_FRAME_NONE || background == BK_FRAME_NONE)
    return fail(e, "background must be loaded before presentation");
  uint32_t nodes[3];
  for (unsigned i = 0; i < 3; ++i) {
    uint32_t node = bk_ending_secondary_assets_visible_node(s->assets, i);
    nodes[i] = node == BK_MODEL_NONE ? 0 : bk_actor_forest_node(forest, 0, node);
    if (nodes[i] == BK_FRAME_NONE) return fail(e, "stale hidden-node binding");
  }
  BkEndingSecondaryPresentationBindings b = {
      frame, auxiliary, automatic, toggles, &primary, &background, nodes, s->random};
  BkEndingSecondaryPresentationOps ops = {
      (void *)s, clock_read, advance, find_node, hide, material, publish,
      face, level, active, timing};
  return bk_ending_secondary_presentation_step(s->state, &b, seconds, &ops, e);
}
