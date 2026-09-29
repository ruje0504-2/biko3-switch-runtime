#include "scene/ending_normal_assets.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct {
  BkModel *model;
  BkClipSet *clips;
  BkActorPose *pose;
  uint32_t root;
} Actor;
struct BkEndingNormalAssets {
  BkEndingNormalConfig config;
  const char *background_name;
  unsigned background_state; /*0=not registered,1=ready,2=failed prefix */
  Actor actors[3];           /* primary, auxiliary, optional background */
  BkEndingBackgroundAssets *background; /* actors[2] is a borrowed view */
  BkActorForest *forest;
  BkEndingCameraAssets *cameras;
  BkBomAssets *bom;
  BkFaceAssets *face;
  BkEyeAssets *eyes;
  BkFaceState face_state;
  uint32_t nodes[BK_ENDING_NORMAL_NODES], oyu;
  float targets[3][3];
};
uint32_t bk_ending_normal_assets_root(const BkEndingNormalAssets *a,
                                       unsigned actor) {
  unsigned index = actor == 4 ? 2 : actor;
  if (!a || !a->forest || index > 2 || (actor != 0 && actor != 1 && actor != 4) ||
      !a->actors[index].pose)
    return BK_FRAME_NONE;
  return bk_actor_forest_node(a->forest, actor, a->actors[index].root);
}
static int fail(char e[256], const char *why) {
  snprintf(e, 256, "normal ending assets: %s", why);
  return 0;
}
static void destroy_actor(Actor *a) {
  bk_actor_pose_destroy(a->pose);
  bk_clip_set_destroy(a->clips);
  bk_model_destroy(a->model);
}
void bk_ending_normal_assets_destroy(BkEndingNormalAssets *a) {
  if (!a)
    return;
  bk_bom_assets_destroy(a->bom);
  bk_actor_forest_destroy(a->forest);
  bk_ending_camera_assets_destroy(a->cameras);
  bk_eye_assets_destroy(a->eyes);
  bk_face_assets_destroy(a->face);
  for (unsigned i = 0; i < 2; i++)
    destroy_actor(&a->actors[i]);
  bk_ending_background_assets_destroy(a->background);
  free(a);
}
static int load_actor(Actor *a, BkResourceStore *s, const char *pack,
                      const char *name, char e[256]) {
  BkBlob raw = {0};
  int ok = 0;
  if (bk_resources_read(s, pack, name, &raw, e) != BK_RESOURCE_OK)
    goto done;
  a->clips = bk_clip_set_decode(raw.data, raw.size, e);
  bk_blob_free(&raw);
  if (!a->clips || bk_resources_read(s, pack, bk_clip_model_name(a->clips),
                                     &raw, e) != BK_RESOURCE_OK)
    goto done;
  if (bk_model_decode(raw.data, raw.size, &a->model, e) != BK_MODEL_OK)
    goto done;
  /* Additional primary/background effect types need actual owners. */
  if (bk_model_chunk(a->model, "LIGA") || bk_model_chunk(a->model, "TEXA")) {
    fail(e, "unsupported animated resource effect");
    goto done;
  }
  a->root = BK_MODEL_NONE;
  for (uint32_t i = 0; i < a->model->frame_count; i++)
    if (a->model->frames[i].parent_index == BK_MODEL_NONE) {
      if (a->root != BK_MODEL_NONE) {
        fail(e, "multiple actor roots");
        goto done;
      }
      a->root = i;
    }
  if (a->root == BK_MODEL_NONE) {
    fail(e, "missing actor root");
    goto done;
  }
  a->pose = bk_actor_pose_create_loaded(a->model, a->clips, a->root,
                                        (float[3]){0}, 0, e);
  if (!a->pose ||
      !bk_actor_pose_root_local(a->pose, a->model->frames[a->root].local, e))
    goto done;
  ok = 1;
done:
  bk_blob_free(&raw);
  return ok;
}
static void snapshot(Actor *a, BkNodeReference *node) {
  memcpy(node->local, bk_actor_pose_local(a->pose, a->root), 64);
  memcpy(node->world, bk_actor_pose_frame(a->pose, a->root), 64);
  memcpy(node->parent_world, bk_actor_pose_parent_world(a->pose, a->root), 64);
}
static int attach_actor(BkEndingNormalAssets *a, Actor *actor, unsigned index,
                        char e[256]) {
  if (!bk_actor_forest_attach(
          a->forest, 0, bk_actor_forest_node(a->forest, index, actor->root), e))
    return 0;
  /*401074 ->40150c(0,0,0), after the model's global attachment. */
  BkNodeReference root;
  snapshot(actor, &root);
  return bk_node_reference_orientation(
             &root, bk_actor_forest_world(a->forest, 0), (float[3]){0, 0, 1},
             (float[3]){0, 1, 0}, e) &&
         bk_actor_pose_root_local(actor->pose, root.local, e);
}
static BkEndingNormalAssets *
create_assets(BkResourceStore *store, unsigned group,
                               unsigned variant, const uint32_t clocks[4],
                               uint32_t *random, BkMenuCamera *camera,
                               BkEndingCameraPresets *presets,
                               BkEndingBackgroundAssets *retained, char e[256]) {
  BkEndingNormalConfig config;
  if (!store || !clocks || !random || !camera || !presets ||
      !bk_ending_normal_config(&config, group, variant)) {
    fail(e, "invalid construction input");
    return NULL;
  }
  BkEndingNormalAssets *a = calloc(1, sizeof(*a));
  if (!a) {
    fail(e, "allocation failed");
    return NULL;
  }
  a->config = config;
  a->background_name = bk_ending_normal_background(group, variant);
  a->oyu = BK_MODEL_NONE;
  BkBlob raw = {0};
  BkBomConfig bom;
  BkMenuCamera next_camera = *camera;
  BkEndingCameraPresets next_presets;
  uint32_t rng = *random;
  if (!load_actor(&a->actors[0], store, "bk3_08", config.primary, e) ||
      !load_actor(&a->actors[1], store, "bk3_08", config.auxiliary, e))
    goto bad;
  a->face = bk_face_assets_create_ending(
      store, "fambom", config.face, "bk3_08", a->actors[0].model,
      bk_clip_model_name(a->actors[0].clips), e);
  if (!a->face)
    goto bad;
  a->eyes = bk_eye_assets_create(store, "bk3_08", a->actors[0].model,
                                 bk_clip_model_name(a->actors[0].clips),
                                 bk_face_assets_config(a->face), e);
  if (!a->eyes)
    goto bad;
  /*4cc582 selects camera variant0 for every4cf318 call. 721e04 is the
   * independent action-table variant, including gallery normal entries. */
  a->cameras =
      bk_ending_camera_assets_create(store, group, 0, config.camera_yaw, e);
  if (!a->cameras)
    goto bad;
  if (group == 1 || retained) {
    a->background = group == 1
        ? bk_ending_background_assets_create(store, "m02_92.xan", e)
        : bk_ending_background_assets_retain(retained);
    const BkEndingBackgroundData *background =
        bk_ending_background_assets_data(a->background);
    if (!background) {
      if (group != 1) fail(e, "cannot retain outer background");
      goto bad;
    }
    a->actors[2] = (Actor){background->model, background->clips,
                           background->pose, background->root};
  }
  if (bk_resources_read(store, "fambom", config.bom, &raw, e) !=
          BK_RESOURCE_OK ||
      !bk_bom_decode(raw.data, raw.size, &bom, e))
    goto bad;
  bk_blob_free(&raw);
  BkActorPose *poses[] = {a->actors[0].pose, a->actors[1].pose,
                          bk_ending_camera_assets_pose(a->cameras, 0),
                          bk_ending_camera_assets_pose(a->cameras, 1),
                          a->actors[2].pose};
  a->forest = bk_actor_forest_create(poses, a->background ? 5 : 4, e);
  if (!a->forest ||
      (retained && group != 1 && !bk_actor_forest_restore_global(
          a->forest, 4, a->actors[2].root, e)) ||
      !attach_actor(a, &a->actors[0], 0, e) ||
      !bk_face_assets_initialize(a->face, &a->face_state, clocks, &rng, e) ||
      !attach_actor(a, &a->actors[1], 1, e) ||
      !bk_eye_assets_select(a->eyes, 1, e))
    goto bad;
  BkNodeReference root;
  snapshot(&a->actors[0], &root);
  if (!bk_node_reference_position(&root, bk_actor_forest_world(a->forest, 0),
                                  config.position, e))
    goto bad;
  float rotated[16];
  if (!bk_ending_camera_root_rotation(rotated, root.local, config.yaw) ||
      !bk_actor_pose_root_local(a->actors[0].pose, rotated, e))
    goto bad;
  BkBomActor inputs[2];
  for (unsigned i = 0; i < 2; i++)
    inputs[i] =
        (BkBomActor){a->actors[i].pose, bk_clip_model_name(a->actors[i].clips),
                     i, a->actors[i].root};
  a->bom = bk_bom_assets_create(store, "bk3_08", &bom, inputs, a->forest, e);
  if (!a->bom)
    goto bad;
  for (unsigned i = 0; i < 2; i++)
    if (!bk_actor_pose_request_mode(a->actors[i].pose, 1,
                                    BK_CLIP_REQUEST_CONFIGURED, e))
      goto bad;
  const uint32_t tracks[] = {2, 3};
  if (!bk_ending_camera_assets_attach(a->cameras, a->forest, tracks,
                                      &next_camera, &next_presets, e))
    goto bad;
  for (unsigned i = 0; i < BK_ENDING_NORMAL_NODES; i++)
    if (!bk_model_find_frame_first(a->actors[0].model, a->actors[0].root,
                                   bk_ending_normal_node_name(i), &a->nodes[i],
                                   e))
      goto bad;
  if (group == 2 &&
      !bk_model_find_frame_first(a->actors[0].model, a->actors[0].root, "OYU",
                                 &a->oyu, e))
    goto bad;
  const unsigned targets[] = {5, 13, 0};
  for (unsigned i = 0; i < 3; i++) {
    if (a->nodes[targets[i]] == BK_MODEL_NONE) {
      fail(e, "missing required camera target");
      goto bad;
    }
    memcpy(a->targets[i],
           bk_actor_pose_frame(a->actors[0].pose, a->nodes[targets[i]]) + 12,
           12);
  }
  if (!bk_ending_camera_assets_step(a->cameras, a->forest, tracks, &next_camera,
                                    BK_ENDING_CAMERA_FIXED, a->targets[0], NULL,
                                    0, BK_FRAME_NONE, 0, e))
    goto bad;
  next_camera.fov = 1;
  if (group == 1 && (!attach_actor(a, &a->actors[2], 4, e) ||
                     !bk_actor_pose_request_mode(
                         a->actors[2].pose, 0, BK_CLIP_REQUEST_CONFIGURED, e)))
    goto bad;
  a->background_state = a->background != NULL;
  *random = rng;
  *camera = next_camera;
  *presets = next_presets;
  return a;
bad:
  bk_blob_free(&raw);
  bk_ending_normal_assets_destroy(a);
  return NULL;
}
BkEndingNormalAssets *bk_ending_normal_assets_create(
    BkResourceStore *store, unsigned group, unsigned variant,
    const uint32_t clocks[4], uint32_t *random, BkMenuCamera *camera,
    BkEndingCameraPresets *presets, char e[256]) {
  return create_assets(store, group, variant, clocks, random, camera, presets,
                       NULL, e);
}
BkEndingNormalAssets *bk_ending_normal_assets_create_reloaded(
    BkResourceStore *store, unsigned group, unsigned variant,
    BkEndingBackgroundAssets *background, const uint32_t clocks[4],
    uint32_t *random, BkMenuCamera *camera, BkEndingCameraPresets *presets,
    char e[256]) {
  const char *expected = bk_ending_normal_background(group, variant);
  const char *actual = bk_ending_background_assets_name(background);
  if (!actual || !expected || (group != 1 && strcmp(actual, expected))) {
    fail(e, "stage reload needs the actual retained outer background");
    return NULL;
  }
  return create_assets(store, group, variant, clocks, random, camera, presets,
                       background, e);
}
BkEndingBackgroundAssets *bk_ending_normal_assets_background(
    const BkEndingNormalAssets *a) {
  return a ? a->background : NULL;
}
int bk_ending_normal_assets_load_background(BkEndingNormalAssets *a,
                                            BkResourceStore *store,
                                            char e[256]) {
  if (!a || !store || a->background_state == 2)
    return fail(e, "invalid or failed background owner");
  if (a->background_state == 1)
    return 1;
  BkEndingBackgroundAssets *background =
      bk_ending_background_assets_create(store, a->background_name, e);
  const BkEndingBackgroundData *data = bk_ending_background_assets_data(background);
  uint32_t index;
  if (!data || !bk_actor_forest_append(a->forest, data->pose, &index, e)) {
    bk_ending_background_assets_destroy(background);
    return 0;
  }
  /* Forest now borrows this actor even if a later stateful step fails. */
  a->background = background;
  a->actors[2] = (Actor){data->model, data->clips, data->pose, data->root};
  a->background_state = 2;
  if (index != 4)
    return fail(e, "background registry changed outside owner");
  if (!attach_actor(a, &a->actors[2], index, e) ||
      !bk_actor_pose_request_mode(a->actors[2].pose, 0,
                                  BK_CLIP_REQUEST_CONFIGURED, e))
    return 0;
  a->background_state = 1;
  return 1;
}
const BkEndingNormalConfig *
bk_ending_normal_assets_config(const BkEndingNormalAssets *a) {
  return a ? &a->config : NULL;
}
BkActorForest *bk_ending_normal_assets_forest(BkEndingNormalAssets *a) {
  return a ? a->forest : NULL;
}
BkActorPose *bk_ending_normal_assets_pose(BkEndingNormalAssets *a, unsigned i) {
  return !a       ? NULL
         : i < 2  ? a->actors[i].pose
         : i < 4  ? bk_ending_camera_assets_pose(a->cameras, i - 2)
         : i == 4 ? a->actors[2].pose
                  : NULL;
}
const char *bk_ending_normal_assets_model_name(const BkEndingNormalAssets *a,
                                               unsigned i) {
  if (!a || (i > 1 && i != 4))
    return NULL;
  const Actor *actor = &a->actors[i == 4 ? 2 : i];
  return actor->clips ? bk_clip_model_name(actor->clips) : NULL;
}
BkBomAssets *bk_ending_normal_assets_bom(BkEndingNormalAssets *a) {
  return a ? a->bom : NULL;
}
BkFaceAssets *bk_ending_normal_assets_face(BkEndingNormalAssets *a) {
  return a ? a->face : NULL;
}
BkFaceState *bk_ending_normal_assets_face_state(BkEndingNormalAssets *a) {
  return a ? &a->face_state : NULL;
}
BkEyeAssets *bk_ending_normal_assets_eyes(BkEndingNormalAssets *a) {
  return a ? a->eyes : NULL;
}
BkEndingCameraAssets *bk_ending_normal_assets_cameras(BkEndingNormalAssets *a) {
  return a ? a->cameras : NULL;
}
uint32_t bk_ending_normal_assets_node(const BkEndingNormalAssets *a,
                                      unsigned i) {
  return a && i < BK_ENDING_NORMAL_NODES ? a->nodes[i] : BK_MODEL_NONE;
}
uint32_t bk_ending_normal_assets_oyu(const BkEndingNormalAssets *a) {
  return a ? a->oyu : BK_MODEL_NONE;
}
const float *bk_ending_normal_assets_target(const BkEndingNormalAssets *a,
                                            unsigned i) {
  return a && i < 3 ? a->targets[i] : NULL;
}
