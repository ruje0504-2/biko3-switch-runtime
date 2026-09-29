#include "scene/ending_secondary_assets.h"
#include "game/ending_normal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef BkEndingBackgroundData SecondaryActor;
struct BkEndingSecondaryAssets {
  BkEndingSecondaryConfig config;
  SecondaryActor primary, background;
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
  snprintf(error, 256, "secondary ending assets: %s", why);
  return 0;
}
static void destroy_actor(SecondaryActor *actor) {
  bk_morph_group_destroy(actor->morph);
  bk_material_animation_destroy(actor->animation);
  bk_material_pose_destroy(actor->materials);
  bk_actor_pose_destroy(actor->pose);
  bk_clip_set_destroy(actor->clips);
  bk_model_destroy(actor->model);
}
void bk_ending_secondary_assets_destroy(BkEndingSecondaryAssets *a) {
  if (!a) return;
  bk_actor_forest_destroy(a->forest);
  bk_ending_camera_assets_destroy(a->cameras);
  bk_eye_assets_destroy(a->eyes);
  bk_face_assets_destroy(a->face);
  destroy_actor(&a->primary);
  bk_ending_background_assets_destroy(a->background_owner);
  free(a);
}
static int load_actor(SecondaryActor *actor, BkResourceStore *store,
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
  if (bk_model_chunk(actor->model, "LIGA") || bk_model_chunk(actor->model, "TEXA")) {
    fail(error, "animated resource effect needs its own owner");
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
static void snapshot(const SecondaryActor *actor, BkNodeReference *node) {
  memcpy(node->local, bk_actor_pose_local(actor->pose, actor->root), 64);
  memcpy(node->world, bk_actor_pose_frame(actor->pose, actor->root), 64);
  memcpy(node->parent_world, bk_actor_pose_parent_world(actor->pose, actor->root), 64);
}
static int attach_actor(BkEndingSecondaryAssets *a, SecondaryActor *actor,
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
static BkEndingSecondaryAssets *create_assets(
    BkResourceStore *store, unsigned group, unsigned background_variant,
    const uint32_t clocks[4], uint32_t *random, BkMenuCamera *camera,
    BkEndingCameraPresets *presets, BkEndingBackgroundAssets *retained,
    char error[256]) {
  BkEndingSecondaryConfig config;
  if (!store || !clocks || !random || !camera || !presets ||
      background_variant > 1 || !bk_ending_secondary_config(&config, group)) {
    fail(error, "invalid construction input");
    return NULL;
  }
  BkEndingSecondaryAssets *a = calloc(1, sizeof(*a));
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
  if (!load_actor(&a->primary, store, "bk3_09", config.primary, error))
    goto bad;
  const char *model_name = bk_clip_model_name(a->primary.clips);
  a->face = bk_face_assets_create_ending(store, "fambom", config.face,
                                        "bk3_09", a->primary.model, model_name, error);
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
  a->eyes = bk_eye_assets_create(store, "bk3_09", a->primary.model, model_name,
                                 bk_face_assets_config(a->face), error);
  a->cameras = bk_ending_camera_assets_create(store, group, 1, config.camera_yaw, error);
  if (!a->eyes || !a->cameras)
    goto bad;
  if (group == 1 || retained) {
    a->background_owner = group == 1
        ? bk_ending_background_assets_create(store, "m02_90.xan", error)
        : bk_ending_background_assets_retain(retained);
    const BkEndingBackgroundData *background =
        bk_ending_background_assets_data(a->background_owner);
    if (!background) {
      if (group != 1) fail(error, "cannot retain outer background");
      goto bad;
    }
    a->background = *background;
  }
  BkActorPose *poses[] = {a->primary.pose,
      bk_ending_camera_assets_pose(a->cameras, 0),
      bk_ending_camera_assets_pose(a->cameras, 1), a->background.pose};
  a->forest = bk_actor_forest_create(poses, a->background_owner ? 4 : 3, error);
  if (!a->forest ||
      (retained && group != 1 && !bk_actor_forest_restore_global(
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
    fail(error, "missing required A_okosi target");
    goto bad;
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
  if (!bk_ending_secondary_targets(a->targets,
          bk_actor_pose_frame(a->primary.pose, a->nodes[0]) + 12,
          bk_actor_pose_frame(a->primary.pose, a->anchor) + 12, error) ||
      !bk_ending_camera_assets_step(a->cameras, a->forest, tracks, &next_camera,
          BK_ENDING_CAMERA_FIXED, a->targets[0], NULL, 0, BK_FRAME_NONE, 0, error) ||
      !bk_model_find_frame_first(a->primary.model, a->primary.root, "A_kuch",
                                  &a->follow, error))
    goto bad;
  /*4D00FA retains4DF411's FOV; the outer frame may overwrite it later.
   * The eye slot was already selected indirectly through4DFB96 above;
   * there is no later normal-loader final FOV1 assignment here. */
  if (a->follow == BK_MODEL_NONE) {
    fail(error, "missing required A_kuch follow target");
    goto bad;
  }
  if (group == 1 && (!attach_actor(a, &a->background, 3, error) ||
      !bk_actor_pose_request_mode(a->background.pose, 0, BK_CLIP_REQUEST_CONFIGURED, error)))
    goto bad;
  a->background_state = a->background_owner != NULL;
  *random = rng;
  *camera = next_camera;
  *presets = next_presets;
  return a;
bad:
  bk_ending_secondary_assets_destroy(a);
  return NULL;
}
BkEndingSecondaryAssets *bk_ending_secondary_assets_create(
    BkResourceStore *store, unsigned group, unsigned variant,
    const uint32_t clocks[4], uint32_t *random, BkMenuCamera *camera,
    BkEndingCameraPresets *presets, char error[256]) {
  return create_assets(store, group, variant, clocks, random, camera, presets,
                       NULL, error);
}
BkEndingSecondaryAssets *bk_ending_secondary_assets_create_reloaded(
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
BkEndingBackgroundAssets *bk_ending_secondary_assets_background(
    const BkEndingSecondaryAssets *a) {
  return a ? a->background_owner : NULL;
}
int bk_ending_secondary_assets_load_background(BkEndingSecondaryAssets *a,
                                                BkResourceStore *store,
                                                char error[256]) {
  if (!a || !store || a->background_state == 2)
    return fail(error, "invalid or failed background owner");
  if (a->background_state == 1) return 1;
  BkEndingBackgroundAssets *background =
      bk_ending_background_assets_create(store, a->background_name, error);
  const BkEndingBackgroundData *next = bk_ending_background_assets_data(background);
  uint32_t index;
  if (!next || !bk_actor_forest_append(a->forest, next->pose, &index, error)) {
    bk_ending_background_assets_destroy(background);
    return 0;
  }
  a->background_owner = background;
  a->background = *next;
  a->background_state = 2;
  if (index != 3) return fail(error, "background registry changed outside owner");
  if (!attach_actor(a, &a->background, index, error) ||
      !bk_actor_pose_request_mode(a->background.pose, 0, BK_CLIP_REQUEST_CONFIGURED, error))
    return 0;
  a->background_state = 1;
  return 1;
}
const BkEndingSecondaryConfig *
bk_ending_secondary_assets_config(const BkEndingSecondaryAssets *a) {
  return a ? &a->config : NULL;
}
BkActorForest *bk_ending_secondary_assets_forest(BkEndingSecondaryAssets *a) {
  return a ? a->forest : NULL;
}
BkActorPose *bk_ending_secondary_assets_pose(BkEndingSecondaryAssets *a,
                                             unsigned actor) {
  if (!a)
    return NULL;
  if (actor == 0)
    return a->primary.pose;
  if (actor < 3)
    return bk_ending_camera_assets_pose(a->cameras, actor - 1);
  return actor == 3 ? a->background.pose : NULL;
}
static const SecondaryActor *effect_actor(const BkEndingSecondaryAssets *a,
                                           unsigned actor) {
  if (!a || (actor != 0 && actor != 3)) return NULL;
  const SecondaryActor *p = actor == 0 ? &a->primary : &a->background;
  return p->pose ? p : NULL;
}
int bk_ending_secondary_assets_advance(BkEndingSecondaryAssets *a,
                                      unsigned actor, float seconds,
                                      char error[256]) {
  const SecondaryActor *p = effect_actor(a, actor);
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
BkMaterialPose *bk_ending_secondary_assets_materials(BkEndingSecondaryAssets *a,
                                                     unsigned actor) {
  const SecondaryActor *p = effect_actor(a, actor);
  return p ? p->materials : NULL;
}
const BkMaterialAnimation *bk_ending_secondary_assets_material_animation(
    const BkEndingSecondaryAssets *a, unsigned actor) {
  const SecondaryActor *p = effect_actor(a, actor);
  return p ? p->animation : NULL;
}
const BkMorphGroup *bk_ending_secondary_assets_morph(
    const BkEndingSecondaryAssets *a, unsigned actor) {
  const SecondaryActor *p = effect_actor(a, actor);
  return p ? p->morph : NULL;
}
const BkMorphMesh *bk_ending_secondary_assets_mesh(
    const BkEndingSecondaryAssets *a, unsigned actor, uint32_t submesh) {
  const SecondaryActor *p = effect_actor(a, actor);
  if (!p) return NULL;
  const BkMorphMesh *mesh = bk_morph_group_mesh(p->morph, submesh);
  return mesh || actor != 0 ? mesh : bk_face_assets_mesh(a->face, submesh);
}
uint32_t bk_ending_secondary_assets_root(const BkEndingSecondaryAssets *a,
                                         unsigned actor) {
  if (!a || !a->forest || (actor != 0 && actor != 3))
    return BK_FRAME_NONE;
  const SecondaryActor *p = actor == 0 ? &a->primary : &a->background;
  return p->pose ? bk_actor_forest_node(a->forest, actor, p->root) : BK_FRAME_NONE;
}
const char *
bk_ending_secondary_assets_model_name(const BkEndingSecondaryAssets *a,
                                       unsigned actor) {
  if (!a || (actor != 0 && actor != 3))
    return NULL;
  const SecondaryActor *p = actor == 0 ? &a->primary : &a->background;
  return p->clips ? bk_clip_model_name(p->clips) : NULL;
}
BkFaceAssets *bk_ending_secondary_assets_face(BkEndingSecondaryAssets *a) {
  return a ? a->face : NULL;
}
BkFaceState *bk_ending_secondary_assets_face_state(BkEndingSecondaryAssets *a) {
  return a ? &a->face_state : NULL;
}
BkEyeAssets *bk_ending_secondary_assets_eyes(BkEndingSecondaryAssets *a) {
  return a ? a->eyes : NULL;
}
BkEndingCameraAssets *
bk_ending_secondary_assets_cameras(BkEndingSecondaryAssets *a) {
  return a ? a->cameras : NULL;
}
uint32_t bk_ending_secondary_assets_node(const BkEndingSecondaryAssets *a,
                                         unsigned i) {
  return a && i < BK_ENDING_NORMAL_NODES ? a->nodes[i] : BK_MODEL_NONE;
}
uint32_t bk_ending_secondary_assets_anchor(const BkEndingSecondaryAssets *a) {
  return a ? a->anchor : BK_MODEL_NONE;
}
uint32_t bk_ending_secondary_assets_follow(const BkEndingSecondaryAssets *a) {
  return a ? a->follow : BK_MODEL_NONE;
}
uint32_t bk_ending_secondary_assets_oyu(const BkEndingSecondaryAssets *a) {
  return a ? a->oyu : BK_MODEL_NONE;
}
uint32_t bk_ending_secondary_assets_visible_node(const BkEndingSecondaryAssets *a,
                                                  unsigned i) {
  return a && i < 3 ? a->visible[i] : BK_MODEL_NONE;
}
unsigned
bk_ending_secondary_assets_missing_visible(const BkEndingSecondaryAssets *a) {
  return a ? a->missing_visible : 3;
}
const float *bk_ending_secondary_assets_target(const BkEndingSecondaryAssets *a,
                                                unsigned i) {
  return a && i < 3 ? a->targets[i] : NULL;
}
