#include "scene/flow_loading_render.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
struct BkFlowLoadingRender {
  BkRenderer *renderer;
  BkTexture *textures[5];
  BkGpuMesh *meshes[4];
  BkFlowLoadingFrame frame;
  unsigned width, height;
  int ready;
};
static int fail(char *error, const char *why) {
  snprintf(error, 256, "loading render: %s", why);
  return 0;
}
void bk_flow_loading_render_destroy(BkFlowLoadingRender *r) {
  if (r) {
    for (unsigned i = 0; i < 4; ++i)
      bk_mesh_destroy(r->renderer, r->meshes[i]);
    for (unsigned i = 0; i < 5; ++i)
      bk_texture_destroy(r->renderer, r->textures[i]);
    free(r);
  }
}
BkFlowLoadingRender *bk_flow_loading_render_create(BkRenderer *renderer,
                                                   BkResourceStore *store,
                                                   uint8_t special,
                                                   char error[256]) {
  if (!renderer || !store) {
    fail(error, "missing services");
    return NULL;
  }
  BkFlowLoadingRender *r = calloc(1, sizeof(*r));
  if (!r) {
    fail(error, "allocation failed");
    return NULL;
  }
  r->renderer = renderer;
  BkBlob raw = {0};
  BkImage image = {0};
  for (unsigned i = 0; i < 5; ++i) {
    if (i == BK_LOADING_SPECIAL && special != 1)
      continue;
    const char *name = bk_flow_loading_image((BkFlowLoadingAsset)i);
    BkResourceResult read =
        bk_resources_read(store, "bk3_00", name, &raw, error);
    if (read != BK_RESOURCE_OK) {
      if (read == BK_RESOURCE_MISSING)
        snprintf(error, 256, "loading render: missing %s", name);
      goto bad;
    }
    if (!bk_image_decode(raw.data, raw.size, &image, error))
      goto bad;
    r->textures[i] =
        bk_texture_create_sampled(renderer, &image, BK_WRAP_REPEAT, error);
    bk_image_free(&image);
    bk_blob_free(&raw);
    if (!r->textures[i])
      goto bad;
  }
  const BkVertex v[4] = {{0, 0, 0, 0, 0, 1, 1, 1, 1},
                         {1, 0, 0, 1, 0, 1, 1, 1, 1},
                         {1, 1, 0, 1, 1, 1, 1, 1, 1},
                         {0, 1, 0, 0, 1, 1, 1, 1, 1}};
  const uint16_t ix[] = {0, 1, 2, 3, 0, 2};
  for (unsigned i = 0; i < 4; ++i)
    if (!(r->meshes[i] = bk_mesh_create(renderer, v, 4, ix, 6, error)))
      goto bad;
  return r;
bad:
  bk_image_free(&image);
  bk_blob_free(&raw);
  bk_flow_loading_render_destroy(r);
  return NULL;
}
int bk_flow_loading_render_prepare(BkFlowLoadingRender *r,
                                   const BkFlowLoadingFrame *f, unsigned width,
                                   unsigned height, char error[256]) {
  if (!r || !f || !width || !height || f->count > 4)
    return fail(error, "invalid frame/extent");
  r->ready = 0;
  unsigned tw, th;
  bk_renderer_extent(r->renderer, &tw, &th);
  if (width > tw || height > th)
    return fail(error, "viewport exceeds target");
  for (unsigned i = 0; i < f->count; ++i) {
    const BkFlowLoadingDraw *d = &f->draws[i];
    float q[4];
    if (!bk_flow_loading_layout(q, d->asset, width, height) ||
        !r->textures[d->asset] || !isfinite(d->alpha) || d->alpha < 0 ||
        d->alpha > 1)
      return fail(error, "invalid draw/missing asset");
    float x = q[0], y = q[1], right = x + q[2], bottom = y + q[3];
    float a = (uint32_t)((double)d->alpha * 255) / 255.f;
    const BkVertex v[4] = {{x, y, 0, 0, 0, 1, 1, 1, a},
                           {right, y, 0, 1, 0, 1, 1, 1, a},
                           {right, bottom, 0, 1, 1, 1, 1, 1, a},
                           {x, bottom, 0, 0, 1, 1, 1, 1, a}};
    if (!bk_mesh_update(r->renderer, r->meshes[i], v, 4, error))
      return 0;
  }
  r->frame = *f;
  r->width = width;
  r->height = height;
  r->ready = 1;
  return 1;
}
int bk_flow_loading_render_draw(BkFlowLoadingRender *r, char error[256]) {
  if (!r || !r->ready)
    return fail(error, "missing prepared frame");
  float matrix[16];
  memcpy(matrix, bk_identity, sizeof(matrix));
  matrix[0] = 2.f / r->width;
  matrix[5] = 2.f / r->height;
  matrix[12] = (float)(1.0 / r->width - 1);
  matrix[13] = (float)(1.0 / r->height - 1);
  for (unsigned i = 0; i < r->frame.count; ++i)
    if (!bk_renderer_draw_mesh(
            r->renderer, r->textures[r->frame.draws[i].asset], r->meshes[i],
            matrix, (BkDrawState){BK_BLEND_ALPHA, 0, BK_CULL_NONE}, error))
      return 0;
  return 1;
}
