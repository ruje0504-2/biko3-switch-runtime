#include "model/material.h"
#include <math.h>
#include <string.h>

int bk_material_state(const BkModelMaterial *m, int texture_alpha,
                      BkMaterialState *out, char error[256]) {
  if (!out)
    return 0;
  memset(out, 0, sizeof(*out));
  if (!m || !isfinite(m->power)) {
    snprintf(error, 256, "invalid material state");
    return 0;
  }
  for (unsigned i = 0; i < 4; i++)
    if (!isfinite(m->diffuse[i]) || !isfinite(m->ambient[i]) ||
        !isfinite(m->specular[i]) || !isfinite(m->emissive[i])) {
      snprintf(error, 256, "nonfinite material color");
      return 0;
    }
  /* Exact float32 constants and strict comparisons from 0x43041a/0x43056d.
   * Preserve unusual boundary behavior instead of approximating with clamp. */
  const float low = 0.99999f, high = 1.00001f, negative = -0.00001f;
  float a = m->diffuse[3];
  if ((a > low && a < high) || (a > negative && a < 1.0f)) {
    /* Preserved by the original setter. */
  } else if (a > high) {
    if (a > 2.0f)
      a = 2.0f;
  } else if (a < negative) {
    if (a < -1.0f)
      a = -1.0f;
  } else {
    a = 0.0f;
  }
  out->encoded_alpha = a;
  out->visible = a != 0.0f; /* Original mesh early return at 0x444c08. */
  memcpy(out->diffuse, m->diffuse, sizeof(out->diffuse));
  out->diffuse[3] =
      a < 0.0f ? -a : a; /* Preserve the original negative zero. */
  if (out->diffuse[3] > 1.0f)
    out->diffuse[3] -= 1.0f;
  out->blend = BK_MATERIAL_ALPHA;
  if (a > low && a < high) {
    out->alpha_hint = !!texture_alpha;
  } else if (a > negative && a < 1.0f) {
    out->alpha_hint = 1;
  } else if (a > high) {
    out->alpha_hint = 1;
    out->blend = BK_MATERIAL_ADDITIVE;
  } else if (a < negative) {
    out->alpha_hint = 1;
    out->blend = BK_MATERIAL_INVERSE_COLOR;
  } else {
    out->diffuse[3] = 0;
    out->alpha_hint = !!texture_alpha;
  }
  out->specular_enabled = m->power >= 0.01f;
  return 1;
}
static int suffix(const char *name, const char *extension) {
  size_t n = strlen(name), length = strlen(extension);
  if (n < length)
    return 0;
  for (size_t i = 0; i < length; i++) {
    unsigned char c = (unsigned char)name[n - length + i];
    if (c >= 'A' && c <= 'Z')
      c += 'a' - 'A';
    if (c != (unsigned char)extension[i])
      return 0;
  }
  return 1;
}
int bk_model_texture_load(const BkModel *m, uint32_t index,
                          BkResourceStore *store, const char *pack,
                          BkModelTextureImage *out, char error[256]) {
  if (!out)
    return 0;
  memset(out, 0, sizeof(*out));
  if (!m || index >= m->texture_count || !store || !pack) {
    snprintf(error, 256, "invalid model texture request");
    return 0;
  }
  const char *name = m->textures[index].filename;
  int tga = suffix(name, ".tga"), bmp = suffix(name, ".bmp");
  if (!tga && !bmp) {
    snprintf(error, 256, "unsupported model texture: %.64s", name);
    return 0;
  }
  BkBlob blob = {0};
  if (bk_resources_read(store, pack, name, &blob, error) != BK_RESOURCE_OK)
    return 0;
  int result = bk_image_decode(blob.data, blob.size, &out->image, error);
  bk_blob_free(&blob);
  if (result)
    out->alpha_hint = tga;
  return result;
}
