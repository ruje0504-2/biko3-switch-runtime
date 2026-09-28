#include "scene/curtain_render.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
struct BkCurtainRender {
  BkRenderer *renderer;
  BkTexture *texture;
  BkGpuMesh *mesh;
  unsigned width, height;
  int ready;
};
static int fail(char *e, const char *why) {
  snprintf(e, 256, "curtain render: %s", why);
  return 0;
}
void bk_curtain_render_destroy(BkCurtainRender *c) {
  if (c) {
    bk_mesh_destroy(c->renderer, c->mesh);
    bk_texture_destroy(c->renderer, c->texture);
    free(c);
  }
}
BkCurtainRender *bk_curtain_render_create(BkRenderer *r, BkResourceStore *store,
                                          char e[256]) {
  if (!r || !store) {
    fail(e, "missing services");
    return NULL;
  }
  BkCurtainRender *c = calloc(1, sizeof(*c));
  if (!c) {
    fail(e, "allocation failed");
    return NULL;
  }
  c->renderer = r;
  BkBlob raw = {0};
  BkImage image = {0};
  BkResourceResult result =
      bk_resources_read(store, "bk3_00", "ma_01.tga", &raw, e);
  if (result != BK_RESOURCE_OK) {
    if (result == BK_RESOURCE_MISSING)
      fail(e, "missing ma_01.tga");
    goto bad;
  }
  if (!bk_image_decode(raw.data, raw.size, &image, e))
    goto bad;
  c->texture = bk_texture_create(r, &image, e);
  bk_blob_free(&raw);
  bk_image_free(&image);
  if (!c->texture)
    goto bad;
  const BkVertex v[4] = {{0, 0, 0, 0, 0, 1, 1, 1, 1},
                         {1, 0, 0, 1, 0, 1, 1, 1, 1},
                         {1, 1, 0, 1, 1, 1, 1, 1, 1},
                         {0, 1, 0, 0, 1, 1, 1, 1, 1}};
  const uint16_t ix[] = {0, 1, 2, 3, 0, 2};
  c->mesh = bk_mesh_create(r, v, 4, ix, 6, e);
  if (!c->mesh)
    goto bad;
  return c;
bad:
  bk_blob_free(&raw);
  bk_image_free(&image);
  bk_curtain_render_destroy(c);
  return NULL;
}
int bk_curtain_render_prepare(BkCurtainRender *c, const BkCommonHudFrame *frame,
                              unsigned width, unsigned height, char e[256]) {
  if (!c || !frame || !width || !height || !isfinite(frame->curtain_alpha) ||
      frame->curtain_alpha < 0 || frame->curtain_alpha > 1)
    return fail(e, "invalid frame/extent");
  unsigned w, h;
  bk_renderer_extent(c->renderer, &w, &h);
  if (width > w || height > h)
    return fail(e, "viewport exceeds target");
  float scale = (float)((double)width / 1280), right = 1280 * scale,
        bottom = 960 * scale;
  float a = (uint32_t)((double)frame->curtain_alpha * 255) / 255.f;
  const BkVertex v[4] = {{0, 0, 0, 0, 0, 1, 1, 1, a},
                         {right, 0, 0, 1, 0, 1, 1, 1, a},
                         {right, bottom, 0, 1, 1, 1, 1, 1, a},
                         {0, bottom, 0, 0, 1, 1, 1, 1, a}};
  c->ready = 0;
  if (!bk_mesh_update(c->renderer, c->mesh, v, 4, e))
    return 0;
  c->width = width;
  c->height = height;
  c->ready = 1;
  return 1;
}
int bk_curtain_render_draw(BkCurtainRender *c, char e[256]) {
  if (!c || !c->ready)
    return fail(e, "missing prepared curtain");
  float matrix[16];
  memcpy(matrix, bk_identity, sizeof(matrix));
  matrix[0] = 2.f / c->width;
  matrix[5] = 2.f / c->height;
  matrix[12] = (float)(1.0 / c->width - 1);
  matrix[13] = (float)(1.0 / c->height - 1);
  return bk_renderer_draw_mesh(c->renderer, c->texture, c->mesh, matrix,
                               (BkDrawState){BK_BLEND_ALPHA, 0, BK_CULL_NONE},
                               e);
}
