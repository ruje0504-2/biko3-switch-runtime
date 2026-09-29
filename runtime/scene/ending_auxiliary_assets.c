#include "scene/ending_auxiliary_assets.h"
#include "game/ending_normal.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef BkEndingBackgroundData AuxiliaryActor;
struct BkEndingAuxiliaryAssets {
  BkEnding4d39Config config;
  AuxiliaryActor primary, background;
  BkEndingBackgroundAssets *background_owner; /* background is a borrowed view */
  BkActorForest *forest;
  BkEndingCameraAssets *cameras;
  BkFaceAssets *face;
  BkEyeAssets *eyes;
  BkFaceState face_state;
  const char *background_name;
  unsigned background_state, missing_visible;
  uint32_t nodes[BK_ENDING_NORMAL_NODES], visible[3], anchor, follow, oyu;
  float targets[3][3];
};
static int fail(char error[256], const char *why) {
  snprintf(error, 256, "auxiliary ending assets: %s", why);
  return 0;
}
static void destroy_actor(AuxiliaryActor *actor) {
  bk_morph_group_destroy(actor->morph);
  bk_material_animation_destroy(actor->animation);
  bk_material_pose_destroy(actor->materials);
  bk_actor_pose_destroy(actor->pose);
  bk_clip_set_destroy(actor->clips);
  bk_model_destroy(actor->model);
}
void bk_ending_auxiliary_assets_destroy(BkEndingAuxiliaryAssets *a) {
  if (!a) return;
  bk_actor_forest_destroy(a->forest);
  bk_ending_camera_assets_destroy(a->cameras);
  bk_eye_assets_destroy(a->eyes);
  bk_face_assets_destroy(a->face);
  destroy_actor(&a->primary);
  bk_ending_background_assets_destroy(a->background_owner);
  free(a);
}
static int load_actor(AuxiliaryActor *actor, BkResourceStore *store,
                       const char *pack, const char *name, char error[256]) {
  BkBlob blob = {0};
  int ok = 0;
  if (bk_resources_read(store, pack, name, &blob, error) != BK_RESOURCE_OK)
    goto done;
  actor->clips = bk_clip_set_decode(blob.data, blob.size, error);
  bk_blob_free(&blob);
  if (!actor->clips ||
      bk_resources_read(store, pack, bk_clip_model_name(actor->clips), &blob,
                          error) != BK_RESOURCE_OK ||
      bk_model_decode(blob.data, blob.size, &actor->model, error) != BK_MODEL_OK)
    goto done;
  /* h02_12 carries a TEXA table. The model keeps that opaque table alive, but
   * 4D39E6 does not invoke its texture scheduler until the 47DC79 controller
   * is active. LIGA still changes material ownership and must remain an
   * explicit construction failure. */
  if (bk_model_chunk(actor->model, "LIGA")) {
    fail(error, "animated light effect needs its own owner");
    goto done;
  }
  actor->materials = bk_material_pose_create(actor->model, error);
  if (!actor->materials ||
      (bk_model_chunk(actor->model, "MATA") &&
       !(actor->animation = bk_material_animation_create(actor->model, error))) ||
      (bk_model_chunk(actor->model, "MORP") &&
       !(actor->morph = bk_morph_group_create(actor->model, error))))
    goto done;
  actor->root = BK_MODEL_NONE;
  for (uint32_t i = 0; i < actor->model->frame_count; ++i)
    if (actor->model->frames[i].parent_index == BK_MODEL_NONE) {
      if (actor->root != BK_MODEL_NONE) {
        fail(error, "multiple actor roots");
        goto done;
      }
      actor->root = i;
    }
  if (actor->root == BK_MODEL_NONE) {
    fail(error, "missing actor root");
    goto done;
  }
  actor->pose = bk_actor_pose_create_loaded(actor->model, actor->clips,
                                            actor->root, (float[3]){0}, 0, error);
  if (!actor->pose || !bk_actor_pose_root_local(actor->pose,
                        actor->model->frames[actor->root].local, error))
    goto done;
  ok = 1;
done:
  bk_blob_free(&blob);
  return ok;
}
static void snapshot(const AuxiliaryActor *actor, BkNodeReference *node) {
  memcpy(node->local, bk_actor_pose_local(actor->pose, actor->root), 64);
  memcpy(node->world, bk_actor_pose_frame(actor->pose, actor->root), 64);
  memcpy(node->parent_world, bk_actor_pose_parent_world(actor->pose, actor->root), 64);
}
static int attach_actor(BkEndingAuxiliaryAssets *a, AuxiliaryActor *actor,
                         unsigned index, char error[256]) {
  if (!bk_actor_forest_attach(a->forest, 0,
        bk_actor_forest_node(a->forest, index, actor->root), error))
    return 0;
  BkNodeReference root;
  snapshot(actor, &root);
  return bk_node_reference_orientation(&root, bk_actor_forest_world(a->forest, 0),
             (float[3]){0, 0, 1}, (float[3]){0, 1, 0}, error) &&
         bk_actor_pose_root_local(actor->pose, root.local, error);
}
static BkEndingAuxiliaryAssets *create_assets(
    BkResourceStore *store, unsigned group, unsigned background_variant,
    const uint32_t clocks[4], uint32_t *random, BkMenuCamera *camera,
    BkEndingCameraPresets *presets, BkEndingBackgroundAssets *retained,
    char error[256]) {
  BkEnding4d39Config config;
  if (!store || !clocks || !random || !camera || !presets ||
      background_variant > 1 ||
      !bk_ending_4d39e6_config(&config, group, background_variant)) {
    fail(error, "invalid construction input");
    return NULL;
  }
  BkEndingAuxiliaryAssets *a = calloc(1, sizeof(*a));
  if (!a) {
    fail(error, "allocation failed");
    return NULL;
  }
  a->config = config;
  a->background_name = bk_ending_normal_background(group, background_variant);
  a->oyu = a->anchor = a->follow = BK_MODEL_NONE;
  BkMenuCamera next_camera = *camera;
  BkEndingCameraPresets next_presets;
  uint32_t rng = *random;
  if (!load_actor(&a->primary, store, "bk3_12", config.primary, error))
    goto bad;
  const char *model_name = bk_clip_model_name(a->primary.clips);
  a->face = bk_face_assets_create_ending(store, "fambom", config.face,
                                        "bk3_12", a->primary.model, model_name, error);
  if (!a->face) goto bad;
  /* Independent target buffers are only valid for disjoint model/FAM meshes.
   * A shared target needs a common vertex owner, never last-writer selection
   * between two unrelated snapshots. */
  for (uint32_t i = 0; i < a->primary.model->submesh_count; ++i)
    if (bk_morph_group_mesh(a->primary.morph, i) &&
        bk_face_assets_mesh(a->face, i)) {
      fail(error, "model and face MORP require a shared target owner");
      goto bad;
    }
  a->eyes = bk_eye_assets_create(store, "bk3_12", a->primary.model, model_name,
                                 bk_face_assets_config(a->face), error);
  a->cameras = bk_ending_camera_assets_create(store, group, 0, config.camera_yaw, error);
  if (!a->eyes || !a->cameras)
    goto bad;
  if (retained) {
    a->background_owner = bk_ending_background_assets_retain(retained);
    const BkEndingBackgroundData *background =
        bk_ending_background_assets_data(a->background_owner);
    if (!background) {
      fail(error, "cannot retain outer background");
      goto bad;
    }
    a->background = *background;
  }
  BkActorPose *poses[] = {a->primary.pose,
      bk_ending_camera_assets_pose(a->cameras, 0),
      bk_ending_camera_assets_pose(a->cameras, 1), a->background.pose};
  a->forest = bk_actor_forest_create(poses, a->background_owner ? 4 : 3, error);
  if (!a->forest ||
      (retained && !bk_actor_forest_restore_global(
          a->forest, 3, a->background.root, error)) ||
      !attach_actor(a, &a->primary, 0, error) ||
      !bk_face_assets_initialize(a->face, &a->face_state, clocks, &rng, error))
    goto bad;
  BkNodeReference root;
  snapshot(&a->primary, &root);
  float rotated[16];
  if (!bk_node_reference_position(&root, bk_actor_forest_world(a->forest, 0),
                                   config.position, error) ||
      !bk_ending_camera_root_rotation(rotated, root.local, config.yaw) ||
      !bk_actor_pose_root_local(a->primary.pose, rotated, error) ||
      /*4D080E/4D081E ->4DFB96 ->4A07A9, before A_okosi lookup. The
       * expression words are external state; the eye texture is this owner. */
      !bk_eye_assets_select(a->eyes, 1, error) ||
      !bk_model_find_frame_first(a->primary.model, a->primary.root, "A_okosi",
                                  &a->anchor, error))
    goto bad;
  if (a->anchor == BK_MODEL_NONE) {
    /* h02_12 has no A_okosi frame. The native 4D39E6 path still initializes
     * the target buffer, but its later camera stage follows A_kuch. Keep the
     * target finite without inventing a cross-actor frame binding. */
    a->anchor = a->primary.root;
  }
  for (unsigned i = 0; i < 3; ++i) {
    if (!bk_model_find_frame_first(a->primary.model, a->primary.root,
                                    config.visible_nodes[i], &a->visible[i], error))
      goto bad;
    if (a->visible[i] == BK_MODEL_NONE)
      ++a->missing_visible;
    else if (!bk_actor_forest_visibility(a->forest,
                 bk_actor_forest_node(a->forest, 0, a->visible[i]), 0, error))
      goto bad;
  }
  const uint32_t tracks[] = {1, 2};
  if (!bk_actor_pose_request_mode(a->primary.pose, 1, BK_CLIP_REQUEST_CONFIGURED, error) ||
      !bk_ending_camera_assets_attach(a->cameras, a->forest, tracks,
                                       &next_camera, &next_presets, error))
    goto bad;
  for (unsigned i = 0; i < BK_ENDING_NORMAL_NODES; ++i)
    if (!bk_model_find_frame_first(a->primary.model, a->primary.root,
                                    bk_ending_normal_node_name(i), &a->nodes[i], error))
      goto bad;
  if (group == 2 && !bk_model_find_frame_first(a->primary.model, a->primary.root,
                                               "OYU", &a->oyu, error))
    goto bad;
  if (a->nodes[0] == BK_MODEL_NONE) {
    fail(error, "missing required node0 target");
    goto bad;
  }
  if (!bk_ending_4d39e6_targets(a->targets,
          bk_actor_pose_frame(a->primary.pose, a->nodes[0]) + 12,
          bk_actor_pose_frame(a->primary.pose, a->anchor) + 12, error) ||
      !bk_ending_camera_assets_step(a->cameras, a->forest, tracks, &next_camera,
          BK_ENDING_CAMERA_FIXED, a->targets[0], NULL, 0, BK_FRAME_NONE, 0, error) ||
      !bk_model_find_frame_first(a->primary.model, a->primary.root, "A_kuch",
                                  &a->follow, error))
    goto bad;
  /*4D39E6 retains4DF411's FOV; the outer frame may overwrite it later.
   * The eye slot was already selected indirectly through4DFB96 above;
   * there is no later normal-loader final FOV1 assignment here. */
  if (a->follow == BK_MODEL_NONE) {
    fail(error, "missing required A_kuch follow target");
    goto bad;
  }
  if (a->background_owner && (!attach_actor(a, &a->background, 3, error) ||
      !bk_actor_pose_request_mode(a->background.pose, 0, BK_CLIP_REQUEST_CONFIGURED, error)))
    goto bad;
  a->background_state = a->background_owner != NULL;
  *random = rng;
  *camera = next_camera;
  *presets = next_presets;
  return a;
bad:
  bk_ending_auxiliary_assets_destroy(a);
  return NULL;
}
BkEndingAuxiliaryAssets *bk_ending_auxiliary_assets_create(
    BkResourceStore *store, unsigned group, unsigned variant,
    const uint32_t clocks[4], uint32_t *random, BkMenuCamera *camera,
    BkEndingCameraPresets *presets, char error[256]) {
  return create_assets(store, group, variant, clocks, random, camera, presets,
                       NULL, error);
}
BkEndingAuxiliaryAssets *bk_ending_auxiliary_assets_create_reloaded(
    BkResourceStore *store, unsigned group, unsigned variant,
    BkEndingBackgroundAssets *background, const uint32_t clocks[4],
    uint32_t *random, BkMenuCamera *camera, BkEndingCameraPresets *presets,
    char error[256]) {
  const char *expected = bk_ending_normal_background(group, variant);
  const char *actual = bk_ending_background_assets_name(background);
  if (!actual || !expected || (group != 1 && strcmp(actual, expected))) {
    fail(error, "stage reload needs the actual retained outer background");
    return NULL;
  }
  return create_assets(store, group, variant, clocks, random, camera, presets,
                       background, error);
}
BkEndingBackgroundAssets *bk_ending_auxiliary_assets_background(
    const BkEndingAuxiliaryAssets *a) {
  return a ? a->background_owner : NULL;
}
int bk_ending_auxiliary_assets_load_background(BkEndingAuxiliaryAssets *a,
                                                BkResourceStore *store,
                                                char error[256]) {
  (void)store;
  if (!a) return fail(error, "missing auxiliary owner");
  return a->background_owner
      ? 1
      : fail(error, "4D39E6 does not own an outer background");
}
const BkEnding4d39Config *
bk_ending_auxiliary_assets_config(const BkEndingAuxiliaryAssets *a) {
  return a ? &a->config : NULL;
}
BkActorForest *bk_ending_auxiliary_assets_forest(BkEndingAuxiliaryAssets *a) {
  return a ? a->forest : NULL;
}
BkActorPose *bk_ending_auxiliary_assets_pose(BkEndingAuxiliaryAssets *a,
                                             unsigned actor) {
  if (!a)
    return NULL;
  if (actor == 0)
    return a->primary.pose;
  if (actor < 3)
    return bk_ending_camera_assets_pose(a->cameras, actor - 1);
  return actor == 3 ? a->background.pose : NULL;
}
static const AuxiliaryActor *effect_actor(const BkEndingAuxiliaryAssets *a,
                                           unsigned actor) {
  if (!a || (actor != 0 && actor != 3)) return NULL;
  const AuxiliaryActor *p = actor == 0 ? &a->primary : &a->background;
  return p->pose ? p : NULL;
}
int bk_ending_auxiliary_assets_advance(BkEndingAuxiliaryAssets *a,
                                      unsigned actor, float seconds,
                                      char error[256]) {
  const AuxiliaryActor *p = effect_actor(a, actor);
  if (!p) return fail(error, "missing primary/background effect owner");
  BkPlaybackEffects effects;
  int submitted;
  if (!bk_actor_pose_advance_effects(p->pose, seconds, &effects, &submitted, error))
    return 0;
  if (!submitted) return 1;
  if (effects.pose.blend && p->morph &&
      !bk_morph_group_blend(p->morph, effects.pose.from, effects.pose.to,
                            effects.pose.weight, NULL, 0, error))
    return 0;
  if (p->animation && !bk_material_animation_sample(
          p->animation, effects.source, p->materials, error))
    return 0;
  return effects.pose.blend || !p->morph ||
         bk_morph_group_sample(p->morph, effects.source, NULL, 0, error);
}
int bk_ending_auxiliary_assets_advance_plain(BkEndingAuxiliaryAssets *a,
                                             float seconds,
                                             BkClipPlainMode mode,
                                             char error[256]) {
  const AuxiliaryActor *p = effect_actor(a, 0);
  if (!p) return fail(error, "missing primary plain effect owner");
  BkPlaybackEffects effects;
  int submitted;
  if (!bk_actor_pose_advance_plain(p->pose, seconds, mode, &effects,
                                   &submitted, error)) return 0;
  if (!submitted) return 1;
  if (p->animation && !bk_material_animation_sample(
          p->animation, effects.source, p->materials, error)) return 0;
  return !p->morph || bk_morph_group_sample(p->morph, effects.source,
                                             NULL, 0, error);
}
BkMaterialPose *bk_ending_auxiliary_assets_materials(BkEndingAuxiliaryAssets *a,
                                                     unsigned actor) {
  const AuxiliaryActor *p = effect_actor(a, actor);
  return p ? p->materials : NULL;
}
int bk_ending_auxiliary_assets_material_alpha(BkEndingAuxiliaryAssets *a,
                                              const char *name,
                                              uint32_t hidden, float alpha,
                                              char error[256]) {
  if (!a || !name) return fail(error, "invalid material lookup");
  if (!*name) return 1;
  const unsigned order[] = {3, 0}; /* retained outer background is oldest */
  for (unsigned n = 0; n < 2; ++n) {
    unsigned actor = order[n];
    const AuxiliaryActor *p = effect_actor(a, actor);
    if (!p) continue;
    for (uint32_t i = 0; i < p->model->material_count; ++i) {
      if (strcmp(p->model->materials[i].name, name)) continue;
      const BkModelMaterial *m = bk_material_pose_material(p->materials, i);
      if (!m || m->id != p->model->materials[i].id ||
          (!hidden && !isfinite(alpha)))
        return fail(error, "invalid material identity/alpha");
      BkMaterialValuesEdit edit = {.index = i, .id = m->id};
      memcpy(edit.values.diffuse, m->diffuse, sizeof(edit.values.diffuse));
      memcpy(edit.values.ambient, m->ambient, sizeof(edit.values.ambient));
      memcpy(edit.values.specular, m->specular, sizeof(edit.values.specular));
      memcpy(edit.values.emissive, m->emissive, sizeof(edit.values.emissive));
      edit.values.power = m->power;
      edit.values.diffuse[3] = hidden ? 0 : alpha;
      return bk_material_pose_values(p->materials, &edit, 1, error);
    }
  }
  return 1;
}
const BkMaterialAnimation *bk_ending_auxiliary_assets_material_animation(
    const BkEndingAuxiliaryAssets *a, unsigned actor) {
  const AuxiliaryActor *p = effect_actor(a, actor);
  return p ? p->animation : NULL;
}
const BkMorphGroup *bk_ending_auxiliary_assets_morph(
    const BkEndingAuxiliaryAssets *a, unsigned actor) {
  const AuxiliaryActor *p = effect_actor(a, actor);
  return p ? p->morph : NULL;
}
const BkMorphMesh *bk_ending_auxiliary_assets_mesh(
    const BkEndingAuxiliaryAssets *a, unsigned actor, uint32_t submesh) {
  const AuxiliaryActor *p = effect_actor(a, actor);
  if (!p) return NULL;
  const BkMorphMesh *mesh = bk_morph_group_mesh(p->morph, submesh);
  return mesh || actor != 0 ? mesh : bk_face_assets_mesh(a->face, submesh);
}
uint32_t bk_ending_auxiliary_assets_root(const BkEndingAuxiliaryAssets *a,
                                         unsigned actor) {
  if (!a || !a->forest || (actor != 0 && actor != 3))
    return BK_FRAME_NONE;
  const AuxiliaryActor *p = actor == 0 ? &a->primary : &a->background;
  return p->pose ? bk_actor_forest_node(a->forest, actor, p->root) : BK_FRAME_NONE;
}
const char *
bk_ending_auxiliary_assets_model_name(const BkEndingAuxiliaryAssets *a,
                                       unsigned actor) {
  if (!a || (actor != 0 && actor != 3))
    return NULL;
  const AuxiliaryActor *p = actor == 0 ? &a->primary : &a->background;
  return p->clips ? bk_clip_model_name(p->clips) : NULL;
}
BkFaceAssets *bk_ending_auxiliary_assets_face(BkEndingAuxiliaryAssets *a) {
  return a ? a->face : NULL;
}
BkFaceState *bk_ending_auxiliary_assets_face_state(BkEndingAuxiliaryAssets *a) {
  return a ? &a->face_state : NULL;
}
BkEyeAssets *bk_ending_auxiliary_assets_eyes(BkEndingAuxiliaryAssets *a) {
  return a ? a->eyes : NULL;
}
BkEndingCameraAssets *
bk_ending_auxiliary_assets_cameras(BkEndingAuxiliaryAssets *a) {
  return a ? a->cameras : NULL;
}
uint32_t bk_ending_auxiliary_assets_node(const BkEndingAuxiliaryAssets *a,
                                         unsigned i) {
  return a && i < BK_ENDING_NORMAL_NODES ? a->nodes[i] : BK_MODEL_NONE;
}
uint32_t bk_ending_auxiliary_assets_anchor(const BkEndingAuxiliaryAssets *a) {
  return a ? a->anchor : BK_MODEL_NONE;
}
uint32_t bk_ending_auxiliary_assets_follow(const BkEndingAuxiliaryAssets *a) {
  return a ? a->follow : BK_MODEL_NONE;
}
uint32_t bk_ending_auxiliary_assets_oyu(const BkEndingAuxiliaryAssets *a) {
  return a ? a->oyu : BK_MODEL_NONE;
}
uint32_t bk_ending_auxiliary_assets_visible_node(const BkEndingAuxiliaryAssets *a,
                                                  unsigned i) {
  return a && i < 3 ? a->visible[i] : BK_MODEL_NONE;
}
unsigned
bk_ending_auxiliary_assets_missing_visible(const BkEndingAuxiliaryAssets *a) {
  return a ? a->missing_visible : 3;
}
const float *bk_ending_auxiliary_assets_target(const BkEndingAuxiliaryAssets *a,
                                                unsigned i) {
  return a && i < 3 ? a->targets[i] : NULL;
}
