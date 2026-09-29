#include "scene/ending_tertiary_assets.h"
#include "game/ending_normal.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef BkEndingBackgroundData TertiaryActor;
struct BkEndingTertiaryAssets {
  BkEndingTertiaryConfig config;
  TertiaryActor actors[3], background;
  BkEndingBackgroundAssets *background_owner;
  BkActorForest *forest;
  BkEndingCameraAssets *cameras;
  BkBomDualAssets *bom;
  BkFaceAssets *face;
  BkEyeAssets *eyes;
  BkFaceState face_state;
  uint32_t registry[BK_ENDING_TERTIARY_ASSET_ROLES], tracks[2];
  uint32_t nodes[BK_ENDING_NORMAL_NODES], visible[3], follow, special;
  const char *background_name;
  unsigned missing_visible, background_state, background_first;
  float targets[3][3];
};
static int fail(char error[256], const char *why) {
  snprintf(error, 256, "tertiary ending assets: %s", why);
  return 0;
}
static void destroy_actor(TertiaryActor *p) {
  bk_morph_group_destroy(p->morph);
  bk_material_animation_destroy(p->animation);
  bk_material_pose_destroy(p->materials);
  bk_actor_pose_destroy(p->pose);
  bk_clip_set_destroy(p->clips);
  bk_model_destroy(p->model);
}
void bk_ending_tertiary_assets_destroy(BkEndingTertiaryAssets *a) {
  if (!a) return;
  bk_actor_forest_destroy(a->forest);
  bk_bom_dual_assets_destroy(a->bom);
  bk_ending_camera_assets_destroy(a->cameras);
  bk_eye_assets_destroy(a->eyes);
  bk_face_assets_destroy(a->face);
  for (unsigned i = 0; i < 3; ++i) destroy_actor(&a->actors[i]);
  bk_ending_background_assets_destroy(a->background_owner);
  free(a);
}
static int load_actor(TertiaryActor *p, BkResourceStore *store, const char *name,
                       int effects, char error[256]) {
  BkBlob blob = {0};
  int ok = 0;
  if (bk_resources_read(store, "bk3_11", name, &blob, error) != BK_RESOURCE_OK)
    goto done;
  p->clips = bk_clip_set_decode(blob.data, blob.size, error);
  bk_blob_free(&blob);
  if (!p->clips || bk_resources_read(store, "bk3_11", bk_clip_model_name(p->clips),
      &blob, error) != BK_RESOURCE_OK ||
      bk_model_decode(blob.data, blob.size, &p->model, error) != BK_MODEL_OK)
    goto done;
  if (bk_model_chunk(p->model, "LIGA") || bk_model_chunk(p->model, "TEXA")) {
    fail(error, "animated resource effect needs its own owner");
    goto done;
  }
  if (effects) {
    p->materials = bk_material_pose_create(p->model, error);
    if (!p->materials || (bk_model_chunk(p->model, "MATA") &&
        !(p->animation = bk_material_animation_create(p->model, error))) ||
        (bk_model_chunk(p->model, "MORP") &&
        !(p->morph = bk_morph_group_create(p->model, error))))
      goto done;
  }
  p->root = BK_MODEL_NONE;
  for (uint32_t i = 0; i < p->model->frame_count; ++i)
    if (p->model->frames[i].parent_index == BK_MODEL_NONE) {
      if (p->root != BK_MODEL_NONE) {
        fail(error, "multiple actor roots");
        goto done;
      }
      p->root = i;
    }
  if (p->root == BK_MODEL_NONE) {
    fail(error, "missing actor root");
    goto done;
  }
  p->pose = bk_actor_pose_create_loaded(p->model, p->clips, p->root,
                                         (float[3]){0}, 0, error);
  if (!p->pose || !bk_actor_pose_root_local(p->pose, p->model->frames[p->root].local, error))
    goto done;
  ok = 1;
done:
  bk_blob_free(&blob);
  return ok;
}
static void snapshot(const TertiaryActor *actor, BkNodeReference *node) {
  memcpy(node->local, bk_actor_pose_local(actor->pose, actor->root), 64);
  memcpy(node->world, bk_actor_pose_frame(actor->pose, actor->root), 64);
  memcpy(node->parent_world, bk_actor_pose_parent_world(actor->pose, actor->root), 64);
}
static int attach_actor(BkEndingTertiaryAssets *a, TertiaryActor *actor,
                         unsigned role, char error[256]) {
  if (!bk_actor_forest_attach(a->forest, 0,
      bk_actor_forest_node(a->forest, a->registry[role], actor->root), error))
    return 0;
  BkNodeReference root;
  snapshot(actor, &root);
  return bk_node_reference_orientation(&root, bk_actor_forest_world(a->forest, 0),
           (float[3]){0, 0, 1}, (float[3]){0, 1, 0}, error) &&
         bk_actor_pose_root_local(actor->pose, root.local, error);
}
static int load_bom(BkEndingTertiaryAssets *a, BkResourceStore *store, char error[256]) {
  const char *names[] = {"jouhansin.bom", "kahansin.bom"};
  BkBomActor actors[3];
  for (unsigned i = 0; i < 3; ++i)
    actors[i] = (BkBomActor){a->actors[i].pose, bk_clip_model_name(a->actors[i].clips),
                              a->registry[i], a->actors[i].root};
  for (unsigned i = 0; i < 2; ++i) {
    BkBlob blob = {0};
    BkBomConfig config;
    int ok = bk_resources_read(store, "fambom", names[i], &blob, error) == BK_RESOURCE_OK &&
             bk_bom_decode(blob.data, blob.size, &config, error);
    bk_blob_free(&blob);
    if (!ok) return 0;
    if (i == 0) {
      a->bom = bk_bom_dual_assets_create(store, "bk3_11", &config, actors, a->forest, error);
      if (!a->bom) return 0;
    } else if (!bk_bom_dual_assets_append(a->bom, store, "bk3_11", &config, a->forest, error))
      return 0;
  }
  /*4d30fd selects both before hiding either; no sampling/publication here. */
  for (unsigned i = 1; i < 3; ++i)
    if (!bk_actor_pose_request_mode(a->actors[i].pose, 1, BK_CLIP_REQUEST_CONFIGURED, error))
      return 0;
  for (unsigned i = 1; i < 3; ++i) {
    BkActorVisibilityEdit hide = {a->actors[i].root, 1};
    if (!bk_actor_pose_visibility(a->actors[i].pose, &hide, 1, error)) return 0;
  }
  return 1;
}
static BkEndingTertiaryAssets *create_assets(
    BkResourceStore *store, unsigned group, unsigned variant,
    const uint32_t clocks[4], uint32_t *random, BkMenuCamera *camera,
    BkEndingCameraPresets *presets, BkEndingBackgroundAssets *retained, char error[256]) {
  BkEndingTertiaryConfig config;
  if (!store || !clocks || !random || !camera || !presets ||
      !bk_ending_tertiary_config(&config, group, variant)) {
    fail(error, "invalid construction inputs");
    return NULL;
  }
  BkEndingTertiaryAssets *a = calloc(1, sizeof(*a));
  if (!a) {
    fail(error, "allocation failed");
    return NULL;
  }
  a->config = config;
  a->background_first = retained != NULL;
  a->background_name = bk_ending_normal_background(group, variant);
  a->follow = a->special = BK_MODEL_NONE;
  for (unsigned i = 0; i < BK_ENDING_TERTIARY_ASSET_ROLES; ++i) a->registry[i] = BK_FRAME_NONE;
  BkMenuCamera next_camera = *camera;
  BkEndingCameraPresets next_presets;
  uint32_t rng = *random;
  TertiaryActor *primary = &a->actors[0];
  if (!load_actor(primary, store, config.primary, 1, error)) goto bad;
  const char *model_name = bk_clip_model_name(primary->clips);
  a->face = bk_face_assets_create_ending(store, "fambom", config.face, "bk3_11",
                                         primary->model, model_name, error);
  if (!a->face) goto bad;
  for (uint32_t i = 0; i < primary->model->submesh_count; ++i)
    if (bk_morph_group_mesh(primary->morph, i) && bk_face_assets_mesh(a->face, i)) {
      fail(error, "model and face MORP require a shared target owner");
      goto bad;
    }
  a->eyes = bk_eye_assets_create(store, "bk3_11", primary->model, model_name,
                                  bk_face_assets_config(a->face), error);
  a->cameras = bk_ending_camera_assets_create(store, group, 5, config.camera_yaw, error);
  if (!a->eyes || !a->cameras) goto bad;
  if (group == 2)
    for (unsigned i = 1; i < 3; ++i)
      if (!load_actor(&a->actors[i], store, config.auxiliaries[i - 1], 0, error))
        goto bad;
  if (retained) {
    a->background_owner = bk_ending_background_assets_retain(retained);
    const BkEndingBackgroundData *background = bk_ending_background_assets_data(a->background_owner);
    if (!background) {
      fail(error, "cannot retain actual outer background");
      goto bad;
    }
    a->background = *background;
  }
  BkActorPose *poses[BK_ENDING_TERTIARY_ASSET_ROLES];
  uint32_t count = 0;
  for (unsigned i = 0; i < 3; ++i)
    if (a->actors[i].pose) {
      a->registry[i] = count;
      poses[count++] = a->actors[i].pose;
    }
  for (unsigned i = 0; i < 2; ++i) {
    a->tracks[i] = a->registry[BK_ENDING_TERTIARY_ASSET_TRACK0 + i] = count;
    poses[count++] = bk_ending_camera_assets_pose(a->cameras, i);
  }
  if (a->background_owner) {
    a->registry[BK_ENDING_TERTIARY_ASSET_BACKGROUND] = count;
    poses[count++] = a->background.pose;
  }
  a->forest = bk_actor_forest_create(poses, count, error);
  if (!a->forest || (retained && !bk_actor_forest_restore_global(a->forest,
      a->registry[BK_ENDING_TERTIARY_ASSET_BACKGROUND], a->background.root, error)) ||
      !attach_actor(a, primary, BK_ENDING_TERTIARY_ASSET_PRIMARY, error) ||
      !bk_face_assets_initialize(a->face, &a->face_state, clocks, &rng, error))
    goto bad;
  if (group == 2)
    for (unsigned i = 1; i < 3; ++i)
      if (!attach_actor(a, &a->actors[i], i, error)) goto bad;
  BkNodeReference root;
  snapshot(primary, &root);
  float rotated[16];
  if (!bk_node_reference_position(&root, bk_actor_forest_world(a->forest, 0), config.position, error) ||
      !bk_ending_camera_root_rotation(rotated, root.local, config.yaw) ||
      !bk_actor_pose_root_local(primary->pose, rotated, error) ||
      !bk_eye_assets_select(a->eyes, (unsigned)config.expression_mode, error) ||
      (group == 2 && !load_bom(a, store, error)))
    goto bad;
  for (unsigned i = 0; i < 3; ++i) {
    if (!bk_model_find_frame_first(primary->model, primary->root, config.visible_nodes[i],
                                     &a->visible[i], error)) goto bad;
    if (a->visible[i] == BK_MODEL_NONE) ++a->missing_visible;
    else if (!bk_actor_forest_visibility(a->forest,
        bk_actor_forest_node(a->forest, a->registry[0], a->visible[i]), 0, error))
      goto bad;
  }
  if (!bk_actor_pose_request_mode(primary->pose, 1, BK_CLIP_REQUEST_CONFIGURED, error) ||
      !bk_ending_camera_assets_attach(a->cameras, a->forest, a->tracks,
                                         &next_camera, &next_presets, error))
    goto bad;
  for (unsigned i = 0; i < BK_ENDING_NORMAL_NODES; ++i)
    if (!bk_model_find_frame_first(primary->model, primary->root,
                                    bk_ending_normal_node_name(i), &a->nodes[i], error))
      goto bad;
  if (group == 2 && !bk_model_find_frame_first(primary->model, primary->root,
      "cris_baiza_A_Layer1", &a->special, error)) goto bad;
  const unsigned targets[] = {5, 13, 0};
  for (unsigned i = 0; i < 3; ++i) {
    if (a->nodes[targets[i]] == BK_MODEL_NONE) {
      fail(error, "missing required cached target");
      goto bad;
    }
    memcpy(a->targets[i], bk_actor_pose_frame(primary->pose, a->nodes[targets[i]]) + 12, 12);
  }
  if (!bk_ending_camera_assets_step(a->cameras, a->forest, a->tracks, &next_camera,
      BK_ENDING_CAMERA_FIXED, a->targets[0], NULL, 0, BK_FRAME_NONE, 0, error) ||
      !bk_model_find_frame_first(primary->model, primary->root, "A_kuch", &a->follow, error))
    goto bad;
  if (a->follow == BK_MODEL_NONE) {
    fail(error, "missing required A_kuch follow target");
    goto bad;
  }
  a->background_state = retained != NULL;
  *camera = next_camera;
  *presets = next_presets;
  *random = rng;
  return a;
bad:
  bk_ending_tertiary_assets_destroy(a);
  return NULL;
}
BkEndingTertiaryAssets *bk_ending_tertiary_assets_create(
    BkResourceStore *store, unsigned group, unsigned variant, const uint32_t clocks[4],
    uint32_t *random, BkMenuCamera *camera, BkEndingCameraPresets *presets, char error[256]) {
  return create_assets(store, group, variant, clocks, random, camera, presets, NULL, error);
}
BkEndingTertiaryAssets *bk_ending_tertiary_assets_create_reloaded(
    BkResourceStore *store, unsigned group, unsigned variant, BkEndingBackgroundAssets *background,
    const uint32_t clocks[4], uint32_t *random, BkMenuCamera *camera,
    BkEndingCameraPresets *presets, char error[256]) {
  const char *actual = bk_ending_background_assets_name(background);
  const char *first = bk_ending_normal_background(group, 0);
  const char *second = bk_ending_normal_background(group, 1);
  if (!actual || !first || !second || (strcmp(actual, first) && strcmp(actual, second))) {
    fail(error, "stage reload requires this group's actual retained background");
    return NULL;
  }
  return create_assets(store, group, variant, clocks, random, camera, presets, background, error);
}
int bk_ending_tertiary_assets_load_background(BkEndingTertiaryAssets *a,
                                               BkResourceStore *store, char error[256]) {
  if (!a || !store || a->background_state == 2) return fail(error, "invalid or failed background owner");
  if (a->background_state == 1) return 1;
  BkEndingBackgroundAssets *owner = bk_ending_background_assets_create(store, a->background_name, error);
  const BkEndingBackgroundData *data = bk_ending_background_assets_data(owner);
  uint32_t index;
  if (!data || !bk_actor_forest_append(a->forest, data->pose, &index, error)) {
    bk_ending_background_assets_destroy(owner);
    return 0;
  }
  a->background_owner = owner;
  a->background = *data;
  a->background_state = 2;
  if (index != a->tracks[1] + 1) return fail(error, "background registry changed outside owner");
  a->registry[BK_ENDING_TERTIARY_ASSET_BACKGROUND] = index;
  if (!attach_actor(a, &a->background, BK_ENDING_TERTIARY_ASSET_BACKGROUND, error) ||
      !bk_actor_pose_request_mode(a->background.pose, 0, BK_CLIP_REQUEST_CONFIGURED, error))
    return 0;
  a->background_state = 1;
  return 1;
}
const BkEndingTertiaryConfig *bk_ending_tertiary_assets_config(const BkEndingTertiaryAssets *a) {
  return a ? &a->config : NULL;
}
BkEndingBackgroundAssets *bk_ending_tertiary_assets_background(const BkEndingTertiaryAssets *a) {
  return a ? a->background_owner : NULL;
}
BkActorForest *bk_ending_tertiary_assets_forest(BkEndingTertiaryAssets *a) { return a ? a->forest : NULL; }
uint32_t bk_ending_tertiary_assets_registry(const BkEndingTertiaryAssets *a, unsigned role) {
  return a && role < BK_ENDING_TERTIARY_ASSET_ROLES ? a->registry[role] : BK_FRAME_NONE;
}
BkEndingTertiaryRole bk_ending_tertiary_assets_role(BkEndingTertiaryActor actor) {
  switch (actor) {
  case BK_ENDING_TERTIARY_PRIMARY: return BK_ENDING_TERTIARY_ASSET_PRIMARY;
  case BK_ENDING_TERTIARY_AUXILIARY_FIRST: return BK_ENDING_TERTIARY_ASSET_UPPER;
  case BK_ENDING_TERTIARY_AUXILIARY_SECOND: return BK_ENDING_TERTIARY_ASSET_LOWER;
  case BK_ENDING_TERTIARY_BACKGROUND: return BK_ENDING_TERTIARY_ASSET_BACKGROUND;
  default: return BK_ENDING_TERTIARY_ASSET_ROLES;
  }
}
static const TertiaryActor *effect_actor(const BkEndingTertiaryAssets *a, unsigned role) {
  if (!a) return NULL;
  const TertiaryActor *p = role < 3 ? &a->actors[role] :
      role == BK_ENDING_TERTIARY_ASSET_BACKGROUND ? &a->background : NULL;
  return p && p->pose ? p : NULL;
}
BkActorPose *bk_ending_tertiary_assets_pose(BkEndingTertiaryAssets *a, unsigned role) {
  if (!a) return NULL;
  if (role == BK_ENDING_TERTIARY_ASSET_TRACK0 || role == BK_ENDING_TERTIARY_ASSET_TRACK1)
    return bk_ending_camera_assets_pose(a->cameras, role - BK_ENDING_TERTIARY_ASSET_TRACK0);
  const TertiaryActor *p = effect_actor(a, role);
  return p ? p->pose : NULL;
}
uint32_t bk_ending_tertiary_assets_root(const BkEndingTertiaryAssets *a, unsigned role) {
  if (a && (role == BK_ENDING_TERTIARY_ASSET_TRACK0 || role == BK_ENDING_TERTIARY_ASSET_TRACK1))
    return bk_actor_forest_node(a->forest, a->registry[role],
        bk_ending_camera_assets_root(a->cameras, role - BK_ENDING_TERTIARY_ASSET_TRACK0));
  const TertiaryActor *p = effect_actor(a, role);
  return p ? bk_actor_forest_node(a->forest, a->registry[role], p->root) : BK_FRAME_NONE;
}
const char *bk_ending_tertiary_assets_model_name(const BkEndingTertiaryAssets *a, unsigned role) {
  const TertiaryActor *p = effect_actor(a, role);
  return p ? bk_clip_model_name(p->clips) : NULL;
}
int bk_ending_tertiary_assets_advance(BkEndingTertiaryAssets *a, unsigned role,
                                    float seconds, char error[256]) {
  const TertiaryActor *p = effect_actor(a, role);
  if (!p) return fail(error, "missing effect actor");
  if (role == BK_ENDING_TERTIARY_ASSET_UPPER || role == BK_ENDING_TERTIARY_ASSET_LOWER)
    return bk_bom_dual_assets_advance(a->bom, role, seconds, error);
  if (role == BK_ENDING_TERTIARY_ASSET_BACKGROUND)
    return bk_ending_background_assets_advance(a->background_owner, seconds, error);
  BkPlaybackEffects effects;
  int submitted;
  if (!bk_actor_pose_advance_effects(p->pose, seconds, &effects, &submitted, error)) return 0;
  if (!submitted) return 1;
  if (effects.pose.blend && p->morph && !bk_morph_group_blend(p->morph,
      effects.pose.from, effects.pose.to, effects.pose.weight, NULL, 0, error)) return 0;
  if (p->animation && !bk_material_animation_sample(p->animation, effects.source, p->materials, error))
    return 0;
  return effects.pose.blend || !p->morph || bk_morph_group_sample(p->morph, effects.source, NULL, 0, error);
}
int bk_ending_tertiary_assets_advance_plain(BkEndingTertiaryAssets *a, unsigned role,
                                           float seconds, BkClipPlainMode mode,
                                           char error[256]) {
  if (role >= 3) return fail(error, "controlled actor must be primary or auxiliary");
  const TertiaryActor *p = effect_actor(a, role);
  if (!p) return fail(error, "missing controlled actor");
  if (role) return bk_bom_dual_assets_advance_plain(a->bom, role, seconds, mode, error);
  BkPlaybackEffects effects;
  int submitted;
  if (!bk_actor_pose_advance_plain(p->pose, seconds, mode, &effects, &submitted, error)) return 0;
  if (!submitted) return 1;
  if (p->animation && !bk_material_animation_sample(p->animation, effects.source, p->materials, error))
    return 0;
  return !p->morph || bk_morph_group_sample(p->morph, effects.source, NULL, 0, error);
}
BkMaterialPose *bk_ending_tertiary_assets_materials(BkEndingTertiaryAssets *a, unsigned role) {
  const TertiaryActor *p = effect_actor(a, role);
  if (!p) return NULL;
  return role == 1 || role == 2 ? bk_bom_dual_assets_materials(a->bom, role) : p->materials;
}
int bk_ending_tertiary_assets_material_alpha(BkEndingTertiaryAssets *a,
                                             const char *name, uint32_t hidden,
                                             float alpha, char error[256]) {
  if (!a || !name) return fail(error, "invalid material lookup");
  if (!*name) return 1; /*429c6b rejects an empty lookup before registry access.*/
  const unsigned fresh[] = {0, 1, 2, BK_ENDING_TERTIARY_ASSET_BACKGROUND};
  const unsigned retained[] = {BK_ENDING_TERTIARY_ASSET_BACKGROUND, 0, 1, 2};
  const unsigned *order = a->background_first ? retained : fresh;
  for (unsigned owner = 0; owner < 4; ++owner) {
    const TertiaryActor *p = effect_actor(a, order[owner]);
    if (!p) continue;
    BkMaterialPose *materials = bk_ending_tertiary_assets_materials(a, order[owner]);
    if (!materials) return fail(error, "missing live material instance");
    for (uint32_t i = 0; i < p->model->material_count; ++i) {
      if (strcmp(p->model->materials[i].name, name)) continue;
      const BkModelMaterial *m = bk_material_pose_material(materials, i);
      if (!m || m->id != p->model->materials[i].id || (!hidden && !isfinite(alpha)))
        return fail(error, "invalid material identity/alpha");
      BkMaterialValuesEdit edit = {.index = i, .id = m->id};
      memcpy(edit.values.diffuse, m->diffuse, sizeof(edit.values.diffuse));
      memcpy(edit.values.ambient, m->ambient, sizeof(edit.values.ambient));
      memcpy(edit.values.specular, m->specular, sizeof(edit.values.specular));
      memcpy(edit.values.emissive, m->emissive, sizeof(edit.values.emissive));
      edit.values.power = m->power;
      edit.values.diffuse[3] = hidden ? 0 : alpha;
      return bk_material_pose_values(materials, &edit, 1, error);
    }
  }
  return 1;
}
const BkMaterialAnimation *bk_ending_tertiary_assets_material_animation(const BkEndingTertiaryAssets *a,
                                                                       unsigned role) {
  const TertiaryActor *p = effect_actor(a, role);
  if (!p) return NULL;
  return role == 1 || role == 2 ? bk_bom_dual_assets_material_animation(a->bom, role) : p->animation;
}
const BkMorphGroup *bk_ending_tertiary_assets_morph(const BkEndingTertiaryAssets *a, unsigned role) {
  const TertiaryActor *p = effect_actor(a, role);
  if (!p) return NULL;
  return role == 1 || role == 2 ? bk_bom_dual_assets_morph(a->bom, role) : p->morph;
}
const BkMorphMesh *bk_ending_tertiary_assets_mesh(const BkEndingTertiaryAssets *a,
                                                 unsigned role, uint32_t submesh) {
  const BkMorphMesh *mesh = bk_morph_group_mesh(bk_ending_tertiary_assets_morph(a, role), submesh);
  return mesh || !a || role != 0 ? mesh : bk_face_assets_mesh(a->face, submesh);
}
BkBomDualAssets *bk_ending_tertiary_assets_bom(BkEndingTertiaryAssets *a) { return a ? a->bom : NULL; }
BkFaceAssets *bk_ending_tertiary_assets_face(BkEndingTertiaryAssets *a) { return a ? a->face : NULL; }
BkFaceState *bk_ending_tertiary_assets_face_state(BkEndingTertiaryAssets *a) { return a ? &a->face_state : NULL; }
BkEyeAssets *bk_ending_tertiary_assets_eyes(BkEndingTertiaryAssets *a) { return a ? a->eyes : NULL; }
BkEndingCameraAssets *bk_ending_tertiary_assets_cameras(BkEndingTertiaryAssets *a) { return a ? a->cameras : NULL; }
uint32_t bk_ending_tertiary_assets_node(const BkEndingTertiaryAssets *a, unsigned i) {
  return a && i < BK_ENDING_NORMAL_NODES ? a->nodes[i] : BK_MODEL_NONE;
}
uint32_t bk_ending_tertiary_assets_follow(const BkEndingTertiaryAssets *a) { return a ? a->follow : BK_MODEL_NONE; }
uint32_t bk_ending_tertiary_assets_special(const BkEndingTertiaryAssets *a) { return a ? a->special : BK_MODEL_NONE; }
uint32_t bk_ending_tertiary_assets_visible_node(const BkEndingTertiaryAssets *a, unsigned i) {
  return a && i < 3 ? a->visible[i] : BK_MODEL_NONE;
}
unsigned bk_ending_tertiary_assets_missing_visible(const BkEndingTertiaryAssets *a) { return a ? a->missing_visible : 3; }
const float *bk_ending_tertiary_assets_target(const BkEndingTertiaryAssets *a, unsigned i) {
  return a && i < 3 ? a->targets[i] : NULL;
}
