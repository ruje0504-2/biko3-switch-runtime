#include "scene/ending_background_assets.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct BkEndingBackgroundAssets {
  BkEndingBackgroundData data;
  uint32_t references;
  char name[64];
};
static int fail(char error[256], const char *why) {
  snprintf(error, 256, "ending background assets: %s", why);
  return 0;
}
BkEndingBackgroundAssets *bk_ending_background_assets_retain(
    BkEndingBackgroundAssets *a) {
  if (!a || !a->references || a->references == UINT32_MAX)
    return NULL;
  ++a->references;
  return a;
}
void bk_ending_background_assets_destroy(BkEndingBackgroundAssets *a) {
  if (!a || --a->references)
    return;
  BkEndingBackgroundData *p = &a->data;
  bk_morph_group_destroy(p->morph);
  bk_material_animation_destroy(p->animation);
  bk_material_pose_destroy(p->materials);
  bk_actor_pose_destroy(p->pose);
  bk_clip_set_destroy(p->clips);
  bk_model_destroy(p->model);
  free(a);
}
BkEndingBackgroundAssets *bk_ending_background_assets_create(
    BkResourceStore *store, const char *name, char error[256]) {
  if (!store || !name || !*name || strlen(name) >= 64) {
    fail(error, "invalid resource name");
    return NULL;
  }
  BkEndingBackgroundAssets *a = calloc(1, sizeof(*a));
  if (!a) {
    fail(error, "allocation failed");
    return NULL;
  }
  a->references = 1;
  memcpy(a->name, name, strlen(name) + 1);
  BkEndingBackgroundData *p = &a->data;
  BkBlob blob = {0};
  if (bk_resources_read(store, "bk3_03", name, &blob, error) != BK_RESOURCE_OK)
    goto bad;
  p->clips = bk_clip_set_decode(blob.data, blob.size, error);
  bk_blob_free(&blob);
  if (!p->clips ||
      bk_resources_read(store, "bk3_03", bk_clip_model_name(p->clips), &blob,
                          error) != BK_RESOURCE_OK ||
      bk_model_decode(blob.data, blob.size, &p->model, error) != BK_MODEL_OK)
    goto bad;
  bk_blob_free(&blob);
  if (bk_model_chunk(p->model, "LIGA") || bk_model_chunk(p->model, "TEXA")) {
    fail(error, "animated resource effect needs its own owner");
    goto bad;
  }
  p->materials = bk_material_pose_create(p->model, error);
  if (!p->materials ||
      (bk_model_chunk(p->model, "MATA") &&
       !(p->animation = bk_material_animation_create(p->model, error))) ||
      (bk_model_chunk(p->model, "MORP") &&
       !(p->morph = bk_morph_group_create(p->model, error))))
    goto bad;
  p->root = BK_MODEL_NONE;
  for (uint32_t i = 0; i < p->model->frame_count; ++i)
    if (p->model->frames[i].parent_index == BK_MODEL_NONE) {
      if (p->root != BK_MODEL_NONE) {
        fail(error, "multiple actor roots");
        goto bad;
      }
      p->root = i;
    }
  if (p->root == BK_MODEL_NONE) {
    fail(error, "missing actor root");
    goto bad;
  }
  p->pose = bk_actor_pose_create_loaded(p->model, p->clips, p->root,
                                         (float[3]){0}, 0, error);
  if (!p->pose || !bk_actor_pose_root_local(p->pose,
                      p->model->frames[p->root].local, error))
    goto bad;
  return a;
bad:
  bk_blob_free(&blob);
  bk_ending_background_assets_destroy(a);
  return NULL;
}
const BkEndingBackgroundData *bk_ending_background_assets_data(
    const BkEndingBackgroundAssets *a) {
  return a ? &a->data : NULL;
}
const char *bk_ending_background_assets_name(const BkEndingBackgroundAssets *a) {
  return a ? a->name : NULL;
}
int bk_ending_background_assets_advance(BkEndingBackgroundAssets *a,
                                        float seconds, char error[256]) {
  if (!a)
    return fail(error, "missing background owner");
  BkEndingBackgroundData *p = &a->data;
  BkPlaybackEffects effects;
  int submitted;
  if (!bk_actor_pose_advance_effects(p->pose, seconds, &effects, &submitted, error))
    return 0;
  if (!submitted)
    return 1;
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
