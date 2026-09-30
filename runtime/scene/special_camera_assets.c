#include "scene/special_camera_assets.h"
#include "game/ending_camera_config.h"
#include "model/x_pose.h"
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
struct BkSpecialCameraAssets {
  BkModel *models[2];
  BkClipSet *clips[2];
  BkActorPose *poses[2];
  uint32_t roots[2], nodes[2];
  BkEndingCameraPresets presets;
  int attached;
};
static int fail(char e[256], const char *why) {
  if (e) snprintf(e, 256, "special camera assets: %s", why);
  return 0;
}
void bk_special_camera_assets_destroy(BkSpecialCameraAssets *a) {
  if (!a) return;
  for (unsigned i = 0; i < 2; ++i) {
    bk_actor_pose_destroy(a->poses[i]);
    bk_clip_set_destroy(a->clips[i]);
    bk_model_destroy(a->models[i]);
  }
  free(a);
}
BkActorPose *bk_special_camera_assets_pose(BkSpecialCameraAssets *a, unsigned i) {
  return a && i < 2 ? a->poses[i] : NULL;
}
uint32_t bk_special_camera_assets_root(const BkSpecialCameraAssets *a, unsigned i) {
  return a && i < 2 ? a->roots[i] : BK_MODEL_NONE;
}
uint32_t bk_special_camera_assets_node(const BkSpecialCameraAssets *a, unsigned i) {
  return a && i < 2 ? a->nodes[i] : BK_MODEL_NONE;
}
BkSpecialCameraAssets *bk_special_camera_assets_create(BkResourceStore *store,
    unsigned group, char e[256]) {
  BkEndingCameraPresets presets;
  if (!store || !bk_special_event_camera_config(&presets, group)) {
    fail(e, "invalid store/group"); return NULL;
  }
  BkSpecialCameraAssets *a = calloc(1, sizeof(*a));
  if (!a) { fail(e, "allocation failed"); return NULL; }
  a->presets = presets;
  BkBlob raw = {0};
  for (unsigned i = 0; i < 2; ++i) {
    char name[32];
    if (i) snprintf(name, sizeof name, "cam%02u_50.xan", group + 1);
    else strcpy(name, "cam00_00.xan");
    if (bk_resources_read(store, "bk3_04", name, &raw, e) != BK_RESOURCE_OK)
      goto bad;
    a->clips[i] = bk_clip_set_decode(raw.data, raw.size, e);
    bk_blob_free(&raw);
    if (!a->clips[i] || bk_resources_read(store, "bk3_04",
        bk_clip_model_name(a->clips[i]), &raw, e) != BK_RESOURCE_OK) goto bad;
    BkModelResult result = raw.size >= 16 && !memcmp(raw.data, "xof 0302txt 0032", 16)
        ? bk_model_x_pose_decode(raw.data, raw.size, &a->models[i], e)
        : bk_model_decode(raw.data, raw.size, &a->models[i], e);
    bk_blob_free(&raw);
    if (result != BK_MODEL_OK) goto bad;
    a->roots[i] = BK_MODEL_NONE;
    for (uint32_t n = 0; n < a->models[i]->frame_count; ++n)
      if (a->models[i]->frames[n].parent_index == BK_MODEL_NONE) {
        if (a->roots[i] != BK_MODEL_NONE) { fail(e, "multiple roots"); goto bad; }
        a->roots[i] = n;
      }
    if (a->roots[i] == BK_MODEL_NONE || !bk_model_find_frame(a->models[i],
        i ? "cam" : "Cam_AUTO", &a->nodes[i], e)) goto bad;
    a->poses[i] = bk_actor_pose_create_loaded(a->models[i], a->clips[i],
        a->roots[i], (float[3]){0}, 0, e);
    if (!a->poses[i] || !bk_actor_pose_root_local(a->poses[i],
        a->models[i]->frames[a->roots[i]].local, e)) goto bad;
  }
  return a;
bad:
  bk_blob_free(&raw); bk_special_camera_assets_destroy(a); return NULL;
}
static int bound(BkSpecialCameraAssets *a, BkActorForest *f,
                 const uint32_t indices[2], int detached, char e[256]) {
  if (!a || !f || !indices) return fail(e, "missing forest binding");
  const float *global = bk_actor_forest_world(f, 0);
  if (!global) return fail(e, "missing global anchor");
  for (unsigned i = 0; i < 16; ++i)
    if (global[i] != (float)(i % 5 == 0)) return fail(e, "nonidentity global");
  for (unsigned i = 0; i < 2; ++i) {
    uint32_t root = bk_actor_forest_node(f, indices[i], a->roots[i]);
    if (root == BK_FRAME_NONE || bk_actor_forest_world(f, root) !=
        bk_actor_pose_frame(a->poses[i], a->roots[i]) ||
        bk_frame_tree_parent(bk_actor_forest_tree(f), root) !=
        (detached ? BK_FRAME_NONE : 0)) return fail(e, "wrong track binding");
  }
  return 1;
}
int bk_special_camera_assets_attach(BkSpecialCameraAssets *a, BkActorForest *f,
    const uint32_t indices[2], float seconds, BkMenuCamera *s,
    BkEndingCameraPresets *presets, char e[256]) {
  if (!a || a->attached || !s || !presets || !isfinite(seconds) || seconds < 0 ||
      (double)seconds * 60 >= INT32_MAX || !bound(a, f, indices, 1, e))
    return fail(e, "invalid attachment");
  s->fov = 1;
  s->pose.position[0] = s->pose.position[2] = 0; s->pose.position[1] = 20;
  s->yaw = s->pitch = s->radius = s->height = 0;
  /*Mark before the first mutation: a partially executed loader is terminal.*/
  a->attached = 1;
  for (unsigned i = 0; i < 2; ++i)
    if (!bk_actor_forest_attach(f, 0, bk_actor_forest_node(f, indices[i], a->roots[i]), e)) return 0;
  for (unsigned i = 0; i < 2; ++i)
    if (!bk_actor_pose_select(a->poses[i], 0, 1, e)) return 0;
  if (!bk_actor_pose_advance(a->poses[1], -1, seconds, e)) return 0;
  *presets = a->presets;
  s->yaw = presets->active[0][0]; s->pitch = presets->active[1][0];
  s->radius = presets->active[2][0]; s->height = presets->active[3][0];
  a->attached = 2;
  return 1;
}
int bk_special_camera_assets_step(BkSpecialCameraAssets *a, BkActorForest *f,
    const uint32_t indices[2], BkMenuCamera *s, BkEndingCameraTransitions *clocks,
    const BkEndingCameraPresets *presets, BkSpecialEventCamera kind, int32_t clip,
    const float offset[3], const float motion[2], unsigned buttons,
    uint32_t focus, float seconds, uint8_t *done, char e[256]) {
  if (!a || a->attached != 2 || !s || !clocks || !presets || !done || !offset ||
      !motion || !isfinite(seconds) || seconds < 0 ||
      (double)seconds * 60 >= INT32_MAX || !bound(a, f, indices, 0, e))
    return fail(e, "invalid controller binding/time");
  if (kind == BK_SPECIAL_CAMERA_OPEN || kind == BK_SPECIAL_CAMERA_TRACK) {
    if ((focus != BK_FRAME_NONE && !bk_actor_forest_world(f, focus)) ||
        (kind == BK_SPECIAL_CAMERA_OPEN && (focus == BK_FRAME_NONE || clip < 0)))
      return fail(e, "missing opening/focus binding");
    unsigned track = kind == BK_SPECIAL_CAMERA_OPEN;
    if (track) { clocks->zoom_fov = 1; s->fov = 1; }
    if (!bk_actor_pose_request(a->poses[track], track ? (unsigned)clip : 0, e) ||
        !bk_actor_pose_advance(a->poses[track], -1, seconds, e) ||
        !bk_actor_forest_refresh(f, e)) return 0;
    const float *p = bk_actor_pose_frame(a->poses[track], a->nodes[track]) + 12;
    const float *target = focus == BK_FRAME_NONE ? NULL : bk_actor_forest_world(f, focus) + 12;
    if (track) {
      if (!bk_menu_camera_opening(s, p, target, e)) return 0;
    } else if (!bk_menu_camera_track(s, p, target, 0x48, seconds, e)) return 0;
  } else if (kind == BK_SPECIAL_CAMERA_ORBIT) {
    if (!bk_ending_camera_orbit(s, motion, buttons, offset, 0x48, seconds, e)) return 0;
  } else if (kind == BK_SPECIAL_CAMERA_TRANSITION) {
    int complete;
    if (clip < 0 || !bk_ending_camera_preset_zoom(s, clocks, presets,
        (unsigned)clip, offset, 0x48, seconds, &complete, e)) return 0;
    *done = (uint8_t)complete;
  } else return fail(e, "unknown camera operation");
  return bk_actor_forest_anchor(f, 1, s->pose.world, 0, e);
}
int bk_special_camera_assets_place(BkSpecialCameraAssets *a, BkActorForest *f,
    const uint32_t indices[2], unsigned track, const float v[3], float radians,
    char e[256]) {
  if (!a || a->attached != 2 || track >= 2 || !v || !bound(a, f, indices, 0, e))
    return fail(e, "invalid placement");
  BkNodeReference node;
  BkActorPose *pose = a->poses[track]; uint32_t root = a->roots[track];
  memcpy(node.local, bk_actor_pose_local(pose, root), 64);
  memcpy(node.world, bk_actor_pose_frame(pose, root), 64);
  memcpy(node.parent_world, bk_actor_pose_parent_world(pose, root), 64);
  const float *global = bk_actor_forest_world(f, 0);
  if (track ? !bk_node_reference_rotation(&node, global, v, radians, e)
            : !bk_node_reference_position(&node, global, v, e)) return 0;
  return bk_actor_pose_root_local(pose, node.local, e);
}
