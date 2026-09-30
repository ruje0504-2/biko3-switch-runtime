/*The48BCBB scene adapter uses the same live actors, material instances,
 *face resources and audio slots as interactive ending stages.*/
#include "game/ending_gallery_presentation.h"
#include "scene/ending_gallery_effect.h"

static BkFaceState *gallery_face_state(EndingNormalScene *s) {
  return s->tertiary_assets ? bk_ending_tertiary_assets_face_state(s->tertiary_assets)
      : s->selected_assets ? bk_ending_selected_assets_face_state(s->selected_assets)
      : s->secondary_assets ? bk_ending_secondary_assets_face_state(s->secondary_assets)
      : s->auxiliary_assets ? bk_ending_auxiliary_assets_face_state(s->auxiliary_assets)
      : bk_ending_normal_assets_face_state(s->assets);
}
static BkFaceAssets *gallery_face_assets(EndingNormalScene *s) {
  return s->tertiary_assets ? bk_ending_tertiary_assets_face(s->tertiary_assets)
      : s->selected_assets ? bk_ending_selected_assets_face(s->selected_assets)
      : s->secondary_assets ? bk_ending_secondary_assets_face(s->secondary_assets)
      : s->auxiliary_assets ? bk_ending_auxiliary_assets_face(s->auxiliary_assets)
      : bk_ending_normal_assets_face(s->assets);
}
static unsigned gallery_role(BkEndingGalleryActor actor) {
  return actor == BK_ENDING_GALLERY_BACKGROUND ? 5 : (unsigned)actor;
}
static int gallery_advance(void *p, BkEndingGalleryActor actor, float seconds,
                            int reverse, char e[256]) {
  EndingNormalScene *s = p;
  unsigned role = gallery_role(actor);
  if (role > 5 || !scene_actor(s, role)) return fail(e, "gallery animation owner absent");
  if (reverse) {
    if (reverse != 1 || role >= 3 || !s->tertiary_assets)
      return fail(e, "gallery reverse scheduler requires third-stage actors");
    return bk_ending_tertiary_assets_advance_plain(s->tertiary_assets, role,
        seconds, BK_CLIP_PLAIN_SCHEDULED, e);
  }
  if (s->tertiary_assets)
    return bk_ending_tertiary_assets_advance(s->tertiary_assets, role, seconds, e);
  unsigned legacy = scene_legacy_role(s, role);
  if (s->selected_assets)
    return bk_ending_selected_assets_advance(s->selected_assets, legacy, seconds, e);
  if (s->secondary_assets)
    return bk_ending_secondary_assets_advance(s->secondary_assets, legacy, seconds, e);
  if (s->auxiliary_assets)
    return bk_ending_auxiliary_assets_advance(s->auxiliary_assets, legacy, seconds, e);
  if (role == 1)
    return bk_bom_assets_advance(bk_ending_normal_assets_bom(s->assets), seconds, e);
  BkActorPose *pose = scene_actor(s, role);
  const BkModel *model = bk_actor_pose_model(pose);
  if (!model || bk_model_chunk(model, "MATA") || bk_model_chunk(model, "MORP"))
    return fail(e, "normal gallery effects require an explicit owner");
  return bk_actor_pose_advance(pose, -1, seconds, e);
}
static int gallery_actor_active(void *p, BkEndingGalleryActor actor,
                                 int32_t *out, char e[256]) {
  BkClipState state;
  if (!out || !bk_actor_pose_state(scene_actor(p, gallery_role(actor)), &state))
    return fail(e, "gallery actor descriptor absent");
  *out = state.slot;
  return 1;
}
static int gallery_find(void *p, uint32_t root, const char *name,
                         uint32_t *out, char e[256]) {
  uint32_t found;
  if (!out || !bk_actor_forest_find(scene_forest(p), root, name, &found, e)) return 0;
  *out = found == BK_FRAME_NONE ? 0 : found;
  return 1;
}
static int gallery_hide(void *p, uint32_t node, uint32_t hidden, char e[256]) {
  return !node || bk_actor_forest_visibility(scene_forest(p), node, hidden, e);
}
static int gallery_material_slot(void *p, unsigned group, unsigned slot,
                                  uint32_t hidden, float alpha, char e[256]) {
  const char *name = bk_ending_tertiary_material_name(
      BK_ENDING_TERTIARY_MATERIAL_FOUR, group, slot);
  return name ? gallery_material(p, name, hidden, alpha, e)
              : fail(e, "gallery material name outside original table");
}
static int gallery_disable(void *p, unsigned index, uint32_t disabled, char e[256]) {
  EndingNormalScene *s = p;
  if (s->tertiary_assets) {
    const BkBomDualBinding *b = bk_bom_dual_assets_binding(
        bk_ending_tertiary_assets_bom(s->tertiary_assets), index);
    if (!b || b->nodes.target == BK_MODEL_NONE) return 1;
  } else if (s->assets) {
    const BkBomAssetBinding *b = bk_bom_assets_binding(
        bk_ending_normal_assets_bom(s->assets), index);
    if (!b || b->target == BK_MODEL_NONE) return 1;
  } else {
    return 1; /*These loaders have no BOM target, as in the native lookup.*/
  }
  if (index >= s->disabled_count) return fail(e, "gallery BOM enable owner absent");
  s->disabled[index] = (int32_t)disabled;
  return 1;
}
static int gallery_effect(void *p, char e[256]) {
  EndingNormalScene *s = p;
  BkEndingGalleryEffectBindings b = {&s->state->frame, &s->state->auxiliary,
      &s->state->control.variant, &s->process->gallery_effect,
      &s->voice_volume, &s->effect_volume};
  BkEndingGalleryEffectOps ops = {s, gallery_active, gallery_timing_unsigned,
                                   selected_audio, selected_random};
  return bk_ending_gallery_effect_step(&b, s->active_seconds, &ops, e);
}
static int gallery_rewind(void *p, unsigned clip, char e[256]) {
  BkEndingClipTiming timing;
  return gallery_timing_unsigned(p, clip, &timing, e) &&
      gallery_source(p, (int32_t)clip, timing.start, e);
}
static int gallery_publish(void *p, char e[256]) {
  return bk_actor_forest_refresh(scene_forest(p), e);
}
static int gallery_follow(void *p, unsigned index, int direction, char e[256]) {
  EndingNormalScene *s = p;
  BkActorPose *primary, *child;
  const BkBomAssetBinding *binding;
  if (s->tertiary_assets) {
    BkBomDualAssets *bom = bk_ending_tertiary_assets_bom(s->tertiary_assets);
    const BkBomDualBinding *b = bk_bom_dual_assets_binding(bom, index);
    if (!b || b->child_actor < 1 || b->child_actor > 2)
      return fail(e, "gallery dual BOM child absent");
    primary = bk_bom_dual_assets_actor(bom, 0);
    child = bk_bom_dual_assets_actor(bom, b->child_actor);
    binding = &b->nodes;
  } else {
    BkBomAssets *bom = bk_ending_normal_assets_bom(s->assets);
    binding = bk_bom_assets_binding(bom, index);
    primary = bk_bom_assets_actor(bom, 0);
    child = bk_bom_assets_actor(bom, 1);
  }
  if (!binding) return fail(e, "gallery BOM pair absent");
  const float *world = bk_actor_pose_frame(primary, binding->reference);
  BkNodeReference node;
  if (!world || !bk_actor_pose_node_reference(child, binding->child, &node, e))
    return fail(e, "gallery BOM reference absent");
  int ok = direction
      ? bk_node_reference_orientation(&node, world, (float[3]){0, 0, 1},
                                        (float[3]){0, 1, 0}, e)
      : bk_node_reference_position(&node, world, (float[3]){0, 0, 0}, e);
  return ok && bk_actor_pose_commit_reference(child, binding->child, &node, e);
}
static int gallery_eye_range(void *p, float minimum, float maximum, char e[256]) {
  EndingNormalScene *s = p;
  return bk_face_eye_range(gallery_face_state(s), minimum, maximum, s->random, e);
}
static int gallery_gaze(void *p, float minimum, float maximum, char e[256]) {
  EndingNormalScene *s = p;
  BkEyeAssets *eyes = scene_eyes(s);
  const BkEyeBinding *binding = bk_eye_assets_binding(eyes);
  /*645604 is the published camera anchor, independent of primary root.*/
  const float *world = bk_actor_forest_world(scene_forest(s), 1);
  if (!binding || !world) return fail(e, "gallery gaze owner absent");
  return bk_actor_pose_eyes(scene_primary(s), binding->frames, world,
      binding->texture_mode, bk_eye_assets_gaze_variant(eyes), minimum, maximum, e);
}
static int gallery_face_request(void *p, int32_t value, char e[256]) {
  EndingNormalScene *s = p;
  return bk_face_request(gallery_face_state(s), value, s->now_ms, e);
}
static int gallery_blink(void *p, uint32_t timestamp, char e[256]) {
  EndingNormalScene *s = p;
  BkFaceCommands commands;
  return bk_face_blink(gallery_face_state(s), timestamp, s->now_ms, s->random,
      &commands, e) && bk_face_assets_apply(gallery_face_assets(s), &commands, e);
}
static int gallery_mouth(void *p, float value, uint32_t timestamp, char e[256]) {
  EndingNormalScene *s = p;
  BkFaceCommands commands;
  (void)timestamp;
  return bk_face_mouth(gallery_face_state(s), value, s->now_ms, &commands, e) &&
      bk_face_assets_apply(gallery_face_assets(s), &commands, e);
}
static int gallery_presentation_step(EndingNormalScene *s, char e[256]) {
  if (!s->process || !s->presentation || !gallery_face_state(s))
    return fail(e, "gallery presentation process owners absent");
  uint32_t roots[] = {scene_root(s, 0), scene_root(s, 5),
                       scene_root(s, 1), scene_root(s, 2)};
  uint32_t hidden[3] = {0}, special = BK_MODEL_NONE;
  for (unsigned i = 0; i < 3; ++i) {
    uint32_t frame = s->tertiary_assets
        ? bk_ending_tertiary_assets_visible_node(s->tertiary_assets, i)
        : s->selected_assets ? bk_ending_selected_assets_visible_node(s->selected_assets, i)
        : s->secondary_assets ? bk_ending_secondary_assets_visible_node(s->secondary_assets, i)
        : s->auxiliary_assets ? bk_ending_auxiliary_assets_visible_node(s->auxiliary_assets, i)
        : BK_MODEL_NONE;
    if (!optional_primary_node(s, frame, &hidden[i], e)) return 0;
  }
  if (s->tertiary_assets) special = bk_ending_tertiary_assets_special(s->tertiary_assets);
  else if (s->selected_assets && s->state->auxiliary.variant)
    special = bk_ending_selected_assets_special(s->selected_assets);
  else if (s->auxiliary_assets) special = bk_ending_auxiliary_assets_oyu(s->auxiliary_assets);
  if (!optional_primary_node(s, special, &special, e)) return 0;
  uint32_t count = s->tertiary_assets
      ? bk_bom_dual_assets_count(bk_ending_tertiary_assets_bom(s->tertiary_assets))
      : s->assets ? bk_bom_assets_count(bk_ending_normal_assets_bom(s->assets)) : 0;
  if (count != s->disabled_count || count > INT32_MAX)
    return fail(e, "gallery BOM owners disagree");
  int32_t bindings = (int32_t)count;
  BkEndingRetainedFinal *r = &s->state->retained.final;
  BkEndingProcess *process = s->process;
  BkEndingGalleryPresentationBindings b = {
    &s->state->frame, &s->state->auxiliary, gallery_face_state(s),
    &s->state->control.variant, (const int8_t *)&r->byte_6ddce0,
    (const int8_t *)&r->byte_6d1be1, &r->word_6c7f74, r->workspace_6c7f80,
    BK_ENDING_RECORD_CAPACITY, &s->selected_action->diagnostic_crossings,
    &s->state->face_mode, &process->gallery_override, &s->state->eye_lower,
    &process->gallery_expression_latch, &process->gallery_mouth_falling,
    &process->gallery_mouth_level, s->state->control.toggles,
    &roots[0], &roots[1], &roots[2], &roots[3], hidden, &special, &bindings, count};
  BkEndingGalleryPresentationOps ops = {s, frame_clock, gallery_advance,
    gallery_actor_active, gallery_find, gallery_hide, gallery_material_slot,
    gallery_disable, gallery_effect, gallery_timing_unsigned, gallery_rewind,
    gallery_publish, gallery_follow, gallery_eye_range, gallery_gaze,
    gallery_face_request, gallery_blink, auxiliary_presentation_level, gallery_mouth};
  return bk_ending_gallery_presentation_step(&b, s->active_seconds, &ops, e);
}
