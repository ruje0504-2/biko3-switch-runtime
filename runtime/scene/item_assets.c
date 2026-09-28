#include "scene/item_assets.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct {
  BkModel *model;
  BkClipSet *clips;
  BkActorPose *pose;
  uint32_t root;
} Object;
struct BkItemAssets {
  Object objects[BK_ITEM_LIMIT];
  BkItemState states[BK_ITEM_LIMIT];
  uint32_t count, group;
};
static int fail(char *error, const char *message) {
  snprintf(error, 256, "item assets: %s", message);
  return 0;
}
void bk_item_assets_destroy(BkItemAssets *a) {
  if (!a)
    return;
  for (uint32_t i = 0; i < BK_ITEM_LIMIT; ++i) {
    bk_actor_pose_destroy(a->objects[i].pose);
    bk_clip_set_destroy(a->objects[i].clips);
    bk_model_destroy(a->objects[i].model);
  }
  free(a);
}
BkItemAssets *bk_item_assets_create(BkResourceStore *store, uint32_t group,
                                    uint32_t area, const uint8_t collected[5],
                                    const BkItemState retained[BK_ITEM_LIMIT],
                                    char error[256]) {
  const BkItemConfig *config;
  uint32_t count;
  if (!store || !collected || !retained ||
      !bk_item_config(&config, &count, group, area)) {
    fail(error, "invalid profile/state/resources");
    return NULL;
  }
  BkItemAssets *a = calloc(1, sizeof(*a));
  if (!a) {
    fail(error, "allocation failed");
    return NULL;
  }
  BkBlob blob = {0};
  a->group = group;
  memcpy(a->states, retained, sizeof(a->states));
  if (!bk_item_initialize(a->states, &a->count, group, area, collected)) {
    fail(error, "invalid retained state");
    goto bad;
  }
  for (uint32_t i = 0; i < count; ++i) {
    Object *o = &a->objects[i];
    if (bk_resources_read(store, "bk3_16", config[i].clip, &blob, error) !=
        BK_RESOURCE_OK)
      goto bad;
    o->clips = bk_clip_set_decode(blob.data, blob.size, error);
    bk_blob_free(&blob);
    if (!o->clips ||
        bk_resources_read(store, "bk3_16", bk_clip_model_name(o->clips), &blob,
                          error) != BK_RESOURCE_OK)
      goto bad;
    int ok =
        bk_model_decode(blob.data, blob.size, &o->model, error) == BK_MODEL_OK;
    bk_blob_free(&blob);
    if (!ok)
      goto bad;
    o->root = BK_MODEL_NONE;
    for (uint32_t f = 0; f < o->model->frame_count; ++f)
      if (o->model->frames[f].parent_index == BK_MODEL_NONE) {
        if (o->root != BK_MODEL_NONE) {
          fail(error, "multiple model roots");
          goto bad;
        }
        o->root = f;
      }
    o->pose = bk_actor_pose_create_loaded(o->model, o->clips, o->root,
                                          a->states[i].position,
                                          a->states[i].yaw, error);
    if (!o->pose)
      goto bad;
  }
  return a;
bad:
  bk_blob_free(&blob);
  bk_item_assets_destroy(a);
  return NULL;
}
uint32_t bk_item_assets_count(const BkItemAssets *a) {
  return a ? a->count : 0;
}
const BkItemState *bk_item_assets_state(const BkItemAssets *a, uint32_t i) {
  return a && i < a->count ? &a->states[i] : NULL;
}
const BkActorPose *bk_item_assets_pose(const BkItemAssets *a, uint32_t i) {
  return a && i < a->count ? a->objects[i].pose : NULL;
}
BkActorPose *bk_item_assets_bind_pose(BkItemAssets *a, uint32_t i) {
  return a && i < a->count ? a->objects[i].pose : NULL;
}
int bk_item_assets_hidden(BkItemAssets *a, uint32_t i, uint8_t hidden) {
  if (!a || i >= a->count)
    return 0;
  a->states[i].hidden = hidden;
  return 1;
}
int bk_item_assets_snapshot(const BkItemAssets *a,
                            BkItemState out[BK_ITEM_LIMIT]) {
  if (!a || !out)
    return 0;
  memcpy(out, a->states, sizeof(a->states));
  return 1;
}
int bk_item_assets_step(BkItemAssets *a, float seconds, char error[256]) {
  if (!a || !isfinite(seconds) || seconds < 0 ||
      (double)seconds * 60 >= INT32_MAX)
    return fail(error, "invalid timestep or owner");
  for (uint32_t i = 0; i < a->count; ++i) {
    Object *o = &a->objects[i];
    BkActorVisibilityEdit edit = {o->root, a->states[i].hidden};
    if (!bk_actor_pose_visibility(o->pose, &edit, 1, error) ||
        !bk_actor_pose_advance(o->pose, 0, seconds, error))
      return 0;
  }
  return 1;
}
int bk_item_assets_pickup(BkItemAssets *a, BkItemPickupState *state,
                          const float current[3], const float previous[3],
                          const BkItemPickupOps *ops, BkItemPickups *out,
                          char error[256]) {
  if (!a)
    return fail(error, "missing pickup owner");
  return bk_item_pickup_run(a->states, (1u << a->count) - 1, state, a->group,
                            current, previous, ops, out, error);
}
