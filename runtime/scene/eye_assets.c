#include "scene/eye_assets.h"
#include <stdlib.h>
#include <string.h>
struct BkEyeAssets {
  BkEyeBinding binding;
  BkImage alternate;
  uint32_t available, selected, gaze_variant;
  int alternate_alpha;
};
void bk_eye_assets_destroy(BkEyeAssets *e) {
  if (e) {
    bk_image_free(&e->alternate);
    free(e);
  }
}
BkEyeAssets *bk_eye_assets_create(BkResourceStore *resources, const char *pack,
                                  const BkModel *model, const char *filename,
                                  const BkFaceConfig *config, char error[256]) {
  if (!resources || !pack || !model || !filename || !config) {
    snprintf(error, 256, "eye assets: missing resource/model/config");
    return NULL;
  }
  if (!memchr(config->eye_materials[0], 0, 256) ||
      !memchr(config->eye_materials[1], 0, 256) ||
      !memchr(config->eye_textures[1], 0, 256) ||
      !memchr(config->slots[0][0].target, 0, 256)) {
    snprintf(error, 256, "eye assets: unterminated configuration name");
    return NULL;
  }
  BkEyeAssets *e = calloc(1, sizeof(*e));
  if (!e) {
    snprintf(error, 256, "eye assets: allocation failed");
    return NULL;
  }
  e->binding = (BkEyeBinding){
      {BK_MODEL_NONE, BK_MODEL_NONE}, BK_MODEL_NONE, config->texture_mode};
  char ignored[256];
  for (unsigned i = 0; i < 2; i++)
    if (config->eye_materials[i][0])
      bk_model_find_frame(model, config->eye_materials[i],
                          &e->binding.frames[i], ignored);
  if (!config->slots[0][0].target[0] ||
      !bk_model_find_submesh(model, filename, config->slots[0][0].target,
                             &e->binding.target_submesh, ignored))
    return e;
  const BkModelSubmesh *target = &model->submeshes[e->binding.target_submesh];
  if (target->texture_count && target->texture_indices[0] != BK_MODEL_NONE)
    e->available |= 1;
  const char *name = config->eye_textures[1];
  if (name[0]) {
    BkBlob blob = {0};
    BkResourceResult result =
        bk_resources_read(resources, pack, name, &blob, error);
    /* Original loader leaves a missing optional replacement slot empty.
     * A corrupt/read-failed resource is still a diagnostic error. */
    if (result == BK_RESOURCE_MISSING)
      return e;
    if (result != BK_RESOURCE_OK)
      goto bad;
    int decoded = bk_image_decode(blob.data, blob.size, &e->alternate, error);
    bk_blob_free(&blob);
    if (!decoded)
      goto bad;
    size_t n = strlen(name);
    e->alternate_alpha = n >= 4 && name[n - 4] == '.' &&
                         (name[n - 3] == 't' || name[n - 3] == 'T') &&
                         (name[n - 2] == 'g' || name[n - 2] == 'G') &&
                         (name[n - 1] == 'a' || name[n - 1] == 'A');
    e->available |= 2;
  }
  return e;
bad:
  bk_eye_assets_destroy(e);
  return NULL;
}
const BkEyeBinding *bk_eye_assets_binding(const BkEyeAssets *e) {
  return e ? &e->binding : NULL;
}
int bk_eye_assets_select(BkEyeAssets *e, uint32_t slot, char error[256]) {
  if (!e || slot >= 5) {
    snprintf(error, 256, "eye assets: invalid texture slot");
    return 0;
  }
  if (e->binding.target_submesh != BK_MODEL_NONE &&
      (e->available & (1u << slot)))
    e->selected = slot;
  return 1;
}
uint32_t bk_eye_assets_selected(const BkEyeAssets *e) {
  return e ? e->selected : 0;
}
const BkImage *bk_eye_assets_image(const BkEyeAssets *e, uint32_t slot,
                                   int *alpha) {
  if (!e || slot != 1 || !(e->available & 2))
    return NULL;
  if (alpha)
    *alpha = e->alternate_alpha;
  return &e->alternate;
}
int bk_eye_assets_gaze(BkEyeAssets *e, BkActorPose *actor, int8_t command,
                       const float target[16], float pitch, float yaw,
                       char error[256]) {
  if (!e || !actor) {
    snprintf(error, 256, "eye assets: missing gaze instance");
    return 0;
  }
  if (command < 0 || command > 2)
    return 1;
  uint32_t variant = command == 1;
  if (command == 2)
    pitch = yaw = 0;
  if (!bk_actor_pose_eyes(actor, e->binding.frames, target,
                          e->binding.texture_mode, variant, pitch, yaw, error))
    return 0;
  e->gaze_variant = variant;
  return 1;
}
uint32_t bk_eye_assets_gaze_variant(const BkEyeAssets *e) {
  return e ? e->gaze_variant : 0;
}
