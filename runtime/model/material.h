#ifndef BK_MODEL_MATERIAL_H
#define BK_MODEL_MATERIAL_H
#include "model/model.h"
#include "resource/store.h"
typedef enum {
  BK_MATERIAL_ALPHA,
  BK_MATERIAL_ADDITIVE,
  BK_MATERIAL_INVERSE_COLOR
} BkMaterialBlend;
typedef struct {
  float diffuse[4]; /* Effective D3D material value, before texture/lighting. */
  float encoded_alpha; /* After the original setter's range handling. */
  BkMaterialBlend blend;
  int visible, alpha_hint, specular_enabled;
} BkMaterialState;
/* Reproduces material setter/apply and zero-alpha mesh rejection. The original
 * alpha_hint is cached metadata, NOT evidence that GPU blending is disabled.
 * Depth, lighting, fog and draw ordering belong to the scene/render path. */
int bk_material_state(const BkModelMaterial *material, int texture_alpha,
                      BkMaterialState *out, char error[256]);
typedef struct {
  BkImage image;
  int alpha_hint; /* Original loader flags TGA by filename, not pixel scan. */
} BkModelTextureImage;
/* Explicit pack lookup, no search across unrelated archives or default image.
 * Caller owns the decoded image. Input model/store remain borrowed. */
int bk_model_texture_load(const BkModel *model, uint32_t texture_index,
                          BkResourceStore *store, const char *pack,
                          BkModelTextureImage *out, char error[256]);
#endif
