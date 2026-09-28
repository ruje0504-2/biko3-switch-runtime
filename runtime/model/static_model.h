#ifndef BK_STATIC_MODEL_H
#define BK_STATIC_MODEL_H
#include "model/material.h"
typedef struct {
  BkMaterialState material;
  uint32_t priority, sort_bias;
  int sorted, depth_write;
} BkStaticPart;
typedef struct {
  uint32_t frame, submesh;
} BkStaticInstance;
typedef struct {
  const BkModel *model; /* Borrowed, immutable, outlives this adapter. */
  BkStaticPart *parts;
  float *world;
  BkStaticInstance *instances;
  uint32_t instance_count, opaque_count;
} BkStaticModel;
/* texture_alpha contains model->texture_count flags from loaded textures.
 * Base pose/layout only. Use bk_model_triangles for each GPU upload: strip
 * and fan streams are not represented by the raw submesh indices.
 * Unsupported topology/state fails explicitly. */
BkStaticModel *bk_static_model_create(const BkModel *model,
                                      const int *texture_alpha,
                                      char error[256]);
void bk_static_model_destroy(BkStaticModel *model);
/* Caller supplies instance_count output elements. Uses the full mesh queue
 * flush policy (including ordinary key exchange and transparent-only bypass).
 * This CPU-only entry uses texture-index+1 as stable identities. */
int bk_static_model_order(const BkStaticModel *model, const float view[16],
                          uint32_t *order, float *distances, char error[256]);
/* Renderer supplies texture_count stable GPU resource identities; NULL uses
 * texture-index+1. Distances are returned in original instance order. */
int bk_static_model_order_textures(const BkStaticModel *, const float view[16],
                                   const uint32_t *texture_keys,
                                   uint32_t *order, float *distances,
                                   char error[256]);
/* Isolated transparent passes, irrespective of full flush dispatch. */
void bk_static_sort_keys(uint32_t *order, const float *distances,
                         const uint32_t *priorities, uint32_t count);
#endif
