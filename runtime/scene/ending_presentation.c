#include "scene/ending_presentation.h"
#include <limits.h>
#include <stdio.h>
#include <string.h>
static int fail(char e[256], const char *why) {
  snprintf(e, 256, "ending presentation scene: %s", why);
  return 0;
}
static int clock_read(void *p, uint32_t *out, char e[256]) {
  const BkEndingPresentationScene *s = p;
  return s->clock(s->clock_context, out, e);
}
static int advance(void *p, BkEndingPresentationActor actor, float seconds,
                   char e[256]) {
  const BkEndingPresentationScene *s = p;
  if (actor == BK_ENDING_PRESENT_SECONDARY)
    return bk_bom_assets_advance(bk_ending_normal_assets_bom(s->assets),
                                  seconds, e);
  unsigned index = actor == BK_ENDING_PRESENT_PRIMARY ? 0 : 4;
  BkActorPose *pose = bk_ending_normal_assets_pose(s->assets, index);
  const BkModel *model = bk_actor_pose_model(pose);
  if (!model)
    return fail(e, "missing animation owner");
  /* These ordinary normal models contain SRT; a replacement with additional
   * time-dependent effects requires its real controller, not an ANIM-only
   * successful stand-in. Face MORP resources are separately owned below. */
  if (bk_model_chunk(model, "MATA") || bk_model_chunk(model, "MORP"))
    return fail(e, "primary/background effects need an explicit owner");
  return bk_actor_pose_advance(pose, -1, seconds, e);
}
static int find_node(void *p, uint32_t root, const char *name, uint32_t *node,
                     char e[256]) {
  const BkEndingPresentationScene *s = p;
  uint32_t found;
  if (!bk_actor_forest_find(bk_ending_normal_assets_forest(s->assets), root,
                            name, &found, e))
    return 0;
  *node = found == BK_FRAME_NONE ? 0 : found;
  return 1;
}
static int hide(void *p, uint32_t node, uint32_t hidden, char e[256]) {
  const BkEndingPresentationScene *s = p;
  if (!node)
    return 1; /*423a99's null node is an actual no-op.*/
  return bk_actor_forest_visibility(bk_ending_normal_assets_forest(s->assets),
                                      node, hidden, e);
}
static int material(void *p, const char *name, uint32_t hidden, float alpha,
                    char e[256]) {
  const BkEndingPresentationScene *s = p;
  const unsigned order[] = {0, 1, 4};
  for (unsigned a = 0; a < 3; ++a) {
    const BkModel *model = bk_actor_pose_model(
        bk_ending_normal_assets_pose(s->assets, order[a]));
    if (!model)
      return fail(e, "missing material registry owner");
    for (uint32_t i = 0; i < model->material_count; ++i) {
      if (strcmp(model->materials[i].name, name))
        continue;
      if (a)
        return fail(e, "normal material belongs to an unsupported owner");
      const BkModelMaterial *m =
          bk_material_pose_material(s->primary_materials, i);
      if (!m || m->id != model->materials[i].id)
        return fail(e, "material registry identity changed");
      BkMaterialValuesEdit edit = {.index = i, .id = m->id};
      memcpy(edit.values.diffuse, m->diffuse, 16);
      memcpy(edit.values.ambient, m->ambient, 16);
      memcpy(edit.values.specular, m->specular, 16);
      memcpy(edit.values.emissive, m->emissive, 16);
      edit.values.power = m->power;
      edit.values.diffuse[3] = hidden ? 0 : alpha;
      return bk_material_pose_values(s->primary_materials, &edit, 1, e);
    }
  }
  return 1;
}
static int disable(void *p, unsigned i, int32_t disabled, char e[256]) {
  const BkEndingPresentationScene *s = p;
  const BkBomAssetBinding *b =
      bk_bom_assets_binding(bk_ending_normal_assets_bom(s->assets), i);
  /* Original lookup tests708104 independently of active binding count. The
   * asset owner exposes absent cells as no target, never an array access. */
  if (!b || b->target == BK_MODEL_NONE)
    return 1;
  if (i >= s->bom_count)
    return fail(e, "missing live BOM enable cell");
  s->bom_disabled[i] = disabled;
  return 1;
}
static int publish(void *p, char e[256]) {
  return bk_actor_forest_refresh(bk_ending_normal_assets_forest(
      ((const BkEndingPresentationScene *)p)->assets), e);
}
static int manual(void *p, unsigned kind, const BkEndingFrameInput *in,
                  uint32_t reference, uint32_t node, int32_t flip,
                  char e[256]) {
  const BkEndingPresentationScene *s = p;
  int32_t dx, dy;
  memcpy(&dx, &in->words[6], 4);
  memcpy(&dy, &in->words[7], 4);
  unsigned gain = s->stick_motion_gain ? s->stick_motion_gain : 1;
  int64_t x = (int64_t)dx * gain, y = (int64_t)dy * gain;
  if (x < INT32_MIN || x > INT32_MAX || y < INT32_MIN || y > INT32_MAX)
    return fail(e, "manual motion outside native integer range");
  dx = (int32_t)x;
  dy = (int32_t)y;
  if (kind < 2) {
    BkBomAssets *bom = bk_ending_normal_assets_bom(s->assets);
    if (kind >= bk_bom_assets_count(bom))
      return 1; /*missing native node pair*/
    return bk_bom_assets_manual(bom, &s->retained->manual[kind],
                                (BkBomManualKind)kind, kind, dx, dy, .09f,
                                7.5f, flip, e);
  }
  if (kind != 2)
    return fail(e, "unknown manual controller");
  BkActorForest *forest = bk_ending_normal_assets_forest(s->assets);
  const float *world = reference ? bk_actor_forest_world(forest, reference) : NULL;
  uint32_t actor, frame;
  BkNodeReference value;
  BkActorPose *pose = NULL;
  if (node) {
    if (!bk_actor_forest_binding(forest, node, &actor, &frame) ||
        !(pose = bk_ending_normal_assets_pose(s->assets, actor)) ||
        !bk_actor_pose_node_reference(pose, frame, &value, e))
      return fail(e, "invalid actual direct node binding");
  }
  if (reference && !world)
    return fail(e, "invalid actual direct reference binding");
  if (node && reference == node)
    world = value.world;
  if (!bk_bom_manual_step(&s->retained->manual[2], BK_BOM_MANUAL_DIRECT,
                           node ? &value : NULL, world, dx, dy, .09f, 7.5f,
                           flip, e))
    return 0;
  return !pose || !world ||
         bk_actor_pose_commit_reference(pose, frame, &value, e);
}
static int follow(void *p, char e[256]) {
  return bk_bom_assets_follow_references(bk_ending_normal_assets_bom(
      ((const BkEndingPresentationScene *)p)->assets), e);
}
static int face(void *p, BkEndingPresentationFace kind, float value,
                int32_t expression, uint32_t timestamp, char e[256]) {
  const BkEndingPresentationScene *s = p;
  BkFaceState *state = bk_ending_normal_assets_face_state(s->assets);
  BkFaceAssets *assets = bk_ending_normal_assets_face(s->assets);
  BkFaceCommands commands;
  uint32_t now;
  if (kind == BK_ENDING_PRESENT_EYE_RANGE)
    return bk_face_eye_range(state, 0, value, s->random, e);
  if (kind == BK_ENDING_PRESENT_EXPRESSION && state->expression == expression)
    return bk_face_request(state, expression, 0, e);
  if (!s->clock(s->clock_context, &now, e))
    return 0;
  if (kind == BK_ENDING_PRESENT_EXPRESSION)
    return bk_face_request(state, expression, now, e);
  if (kind == BK_ENDING_PRESENT_BLINK) {
    if (!bk_face_blink(state, timestamp, now, s->random, &commands, e))
      return 0;
  } else if (kind == BK_ENDING_PRESENT_MOUTH) {
    if (!bk_face_mouth(state, value, now, &commands, e))
      return 0;
  } else
    return fail(e, "unknown face controller");
  return bk_face_assets_apply(assets, &commands, e);
}
static int gaze(void *p, char e[256]) {
  const BkEndingPresentationScene *s = p;
  BkEyeAssets *eyes = bk_ending_normal_assets_eyes(s->assets);
  const BkEyeBinding *binding = bk_eye_assets_binding(eyes);
  if (!binding)
    return fail(e, "missing actual eye binding");
  return bk_actor_pose_eyes(
      bk_ending_normal_assets_pose(s->assets, 0), binding->frames,
      bk_actor_forest_world(bk_ending_normal_assets_forest(s->assets), 1),
      binding->texture_mode, bk_eye_assets_gaze_variant(eyes), .001f, .2f, e);
}
static int level(void *p, float *out, char e[256]) {
  const BkEndingPresentationScene *s = p;
  return bk_ending_audio_level(s->audio, 0, &s->retained->voice, out, e);
}
int bk_ending_presentation_scene_step(const BkEndingPresentationScene *s,
                                       const BkEndingPresentationBindings *b,
                                       const BkEndingFrameInput *in,
                                       float seconds, char e[256]) {
  if (!s || !s->assets || !s->audio || !s->primary_materials || !s->retained ||
      !s->random || !s->clock || (s->bom_count && !s->bom_disabled) ||
      s->bom_count != bk_bom_assets_count(bk_ending_normal_assets_bom(s->assets)))
    return fail(e, "incomplete actual scene owners");
  BkEndingPresentationOps ops = {(void *)s, clock_read, advance, find_node,
      hide, material, disable, publish, manual, follow, face, gaze, level};
  return bk_ending_presentation_step(b, in, seconds, &ops, e);
}
