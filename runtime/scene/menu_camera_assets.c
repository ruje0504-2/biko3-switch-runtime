#include "scene/menu_camera_assets.h"
#include "model/x_pose.h"
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
struct BkMenuCameraAssets {
  BkModel *models[2];
  BkClipSet *clips[2];
  BkActorPose *poses[2];
  uint32_t roots[2], nodes[2];
};
static int fail(char error[256], const char *message) {
  snprintf(error, 256, "menu camera assets: %s", message);
  return 0;
}
void bk_menu_camera_assets_destroy(BkMenuCameraAssets *a) {
  if (!a)
    return;
  for (unsigned i = 0; i < 2; ++i) {
    bk_actor_pose_destroy(a->poses[i]);
    bk_clip_set_destroy(a->clips[i]);
    bk_model_destroy(a->models[i]);
  }
  free(a);
}
BkActorPose *bk_menu_camera_assets_pose(BkMenuCameraAssets *a, unsigned i) {
  return a && i < 2 ? a->poses[i] : NULL;
}
uint32_t bk_menu_camera_assets_root(const BkMenuCameraAssets *a, unsigned i) {
  return a && i < 2 ? a->roots[i] : BK_MODEL_NONE;
}
uint32_t bk_menu_camera_assets_node(const BkMenuCameraAssets *a, unsigned i) {
  return a && i < 2 ? a->nodes[i] : BK_MODEL_NONE;
}
BkMenuCameraAssets *bk_menu_camera_assets_create(BkResourceStore *store,
                                                 unsigned group, float seconds,
                                                 BkMenuCamera *state,
                                                 char error[256]) {
  if (!store || !state || group >= 5 || !isfinite(seconds) || seconds < 0 ||
      (double)seconds * 60 >= INT32_MAX) {
    fail(error, "invalid retail selection load");
    return NULL;
  }
  BkMenuCameraAssets *a = calloc(1, sizeof(*a));
  if (!a) {
    fail(error, "allocation failed");
    return NULL;
  }
  BkBlob raw = {0};
  for (unsigned i = 0; i < 2; ++i) {
    char name[32];
    if (i)
      snprintf(name, sizeof(name), "cam%02u_50.xan", group + 1);
    else
      strcpy(name, "cam00_00.xan");
    if (bk_resources_read(store, "bk3_04", name, &raw, error) != BK_RESOURCE_OK)
      goto bad;
    a->clips[i] = bk_clip_set_decode(raw.data, raw.size, error);
    bk_blob_free(&raw);
    if (!a->clips[i] ||
        bk_resources_read(store, "bk3_04", bk_clip_model_name(a->clips[i]),
                          &raw, error) != BK_RESOURCE_OK)
      goto bad;
    BkModelResult result =
        raw.size >= 16 && !memcmp(raw.data, "xof 0302txt 0032", 16)
            ? bk_model_x_pose_decode(raw.data, raw.size, &a->models[i], error)
            : bk_model_decode(raw.data, raw.size, &a->models[i], error);
    bk_blob_free(&raw);
    if (result != BK_MODEL_OK)
      goto bad;
    a->roots[i] = BK_MODEL_NONE;
    for (uint32_t n = 0; n < a->models[i]->frame_count; ++n)
      if (a->models[i]->frames[n].parent_index == BK_MODEL_NONE) {
        if (a->roots[i] != BK_MODEL_NONE) {
          fail(error, "multiple roots");
          goto bad;
        }
        a->roots[i] = n;
      }
    if (a->roots[i] == BK_MODEL_NONE ||
        !bk_model_find_frame(a->models[i], i ? "cam" : "Cam_AUTO", &a->nodes[i],
                             error))
      goto bad;
    a->poses[i] = bk_actor_pose_create(a->models[i], a->clips[i], a->roots[i],
                                       NULL, (float[3]){0}, 0, 0, 1, error);
    if (!a->poses[i] ||
        !bk_actor_pose_root_local(
            a->poses[i], a->models[i]->frames[a->roots[i]].local, error))
      goto bad;
  }
  if (!bk_actor_pose_advance(a->poses[1], -1, seconds, error))
    goto bad;
  /*4be4f8 only overwrites the first entry of each orbit preset in flow38.
   * Other flows' preset tables do not belong to this selection-only owner. */
  state->pose.position[0] = state->pose.position[2] = 0;
  state->pose.position[1] = 20;
  state->yaw = state->pitch = 0;
  state->radius = 22;
  state->height = 18;
  state->fov = 1;
  return a;
bad:
  bk_blob_free(&raw);
  bk_menu_camera_assets_destroy(a);
  return NULL;
}
int bk_menu_camera_assets_step(BkMenuCameraAssets *a, BkActorForest *forest,
                               const uint32_t indices[2], BkMenuCamera *state,
                               unsigned mode, const float motion[2],
                               unsigned buttons, uint32_t focus_node,
                               float seconds, char error[256]) {
  if (!a || !forest || !indices || !state || mode > 1 || !motion ||
      !isfinite(motion[0]) || !isfinite(motion[1]) || (buttons & ~3u) ||
      !isfinite(seconds) || seconds < 0 || (double)seconds * 60 >= INT32_MAX)
    return fail(error, "invalid camera step");
  const BkFrameTree *tree = bk_actor_forest_tree(forest);
  for (unsigned i = 0; i < 2; ++i) {
    uint32_t root = bk_actor_forest_node(forest, indices[i], a->roots[i]);
    if (root == BK_FRAME_NONE || bk_frame_tree_parent(tree, root) != 0 ||
        bk_actor_forest_world(forest, root) !=
            bk_actor_pose_frame(a->poses[i], a->roots[i]))
      return fail(error, "camera tracks not bound to supplied global forest");
  }
  const float *global = bk_actor_forest_world(forest, 0);
  for (unsigned i = 0; i < 16; ++i)
    if (global[i] != (float)(i % 5 == 0))
      return fail(error, "selection requires identity global anchor");
  if (focus_node != BK_FRAME_NONE && !bk_actor_forest_world(forest, focus_node))
    return fail(error, "missing focus binding");
  if (mode) {
    if (!bk_actor_pose_advance(a->poses[0], 0, seconds, error) ||
        !bk_actor_forest_refresh(forest, error))
      return 0;
    const float *track = bk_actor_pose_frame(a->poses[0], a->nodes[0]);
    const float *focus = focus_node == BK_FRAME_NONE
                             ? NULL
                             : bk_actor_forest_world(forest, focus_node) + 12;
    if (!bk_menu_camera_track(state, track + 12, focus, 0x38, seconds, error))
      return 0;
  } else if (!bk_menu_camera_orbit(state, motion, buttons, seconds, error))
    return 0;
  return bk_actor_forest_anchor(forest, 1, state->pose.world, 0, error);
}
