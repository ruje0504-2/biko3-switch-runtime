#include "scene/selection_ui_render.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
struct BkSelectionUiRender {
  BkRenderer *renderer;
  BkTexture *textures[50];
  BkGpuMesh *meshes[BK_SELECTION_DRAWS];
  BkSelectionFrame frame;
  unsigned width, height;
  int ready;
};
static int fail(char e[256], const char *why) {
  snprintf(e, 256, "selection render: %s", why);
  return 0;
}
void bk_selection_ui_render_destroy(BkSelectionUiRender *r) {
  if (!r)
    return;
  for (unsigned i = 0; i < BK_SELECTION_DRAWS; ++i)
    bk_mesh_destroy(r->renderer, r->meshes[i]);
  for (unsigned i = 0; i < 50; ++i)
    bk_texture_destroy(r->renderer, r->textures[i]);
  free(r);
}
BkSelectionUiRender *bk_selection_ui_render_create(BkRenderer *renderer,
                                                   BkResourceStore *store,
                                                   uint8_t special,
                                                   char e[256]) {
  if (!renderer || !store) {
    fail(e, "missing services");
    return NULL;
  }
  BkSelectionUiRender *r = calloc(1, sizeof(*r));
  if (!r) {
    fail(e, "allocation failed");
    return NULL;
  }
  r->renderer = renderer;
  BkBlob raw = {0};
  BkImage image = {0};
  for (unsigned i = 0; i < 50; ++i) {
    const char *name = bk_selection_image(i, special);
    if (!name)
      continue;
    BkResourceResult read = bk_resources_read(store, "bk3_00", name, &raw, e);
    if (read != BK_RESOURCE_OK) {
      if (read == BK_RESOURCE_MISSING)
        snprintf(e, 256, "selection render: missing %s", name);
      goto bad;
    }
    if (!bk_image_decode(raw.data, raw.size, &image, e))
      goto bad;
    r->textures[i] =
        bk_texture_create_sampled(renderer, &image, BK_WRAP_REPEAT, e);
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
  for (unsigned i = 0; i < BK_SELECTION_DRAWS; ++i)
    if (!(r->meshes[i] = bk_mesh_create(renderer, v, 4, ix, 6, e)))
      goto bad;
  return r;
bad:
  bk_image_free(&image);
  bk_blob_free(&raw);
  bk_selection_ui_render_destroy(r);
  return NULL;
}
int bk_selection_ui_render_prepare(BkSelectionUiRender *r,
                                   const BkSelectionFrame *f, unsigned width,
                                   unsigned height, char e[256]) {
  if (!r || !f || !width || !height || f->count > BK_SELECTION_DRAWS)
    return fail(e, "invalid frame/extent");
  r->ready = 0;
  unsigned tw, th;
  bk_renderer_extent(r->renderer, &tw, &th);
  if (width > tw || height > th)
    return fail(e, "viewport exceeds target");
  for (unsigned i = 0; i < f->count; ++i) {
    const BkSelectionDraw *d = &f->draws[i];
    const float *q = d->corners;
    const float *uv = d->uv;
    for (unsigned j = 0; j < 4; ++j)
      if (!isfinite(uv[j]))
        return fail(e, "invalid UV");
    if (d->slot >= 50 || !r->textures[d->slot] || !isfinite(d->alpha) ||
        d->alpha < 0 || d->alpha > 1 || !isfinite(q[0]) || !isfinite(q[1]) ||
        !isfinite(q[2]) || !isfinite(q[3]) || q[2] < q[0] || q[3] < q[1])
      return fail(e, "invalid draw/missing asset");
    float a = (uint32_t)((double)d->alpha * 255) / 255.f;
    const BkVertex v[4] = {{q[0], q[1], 0, uv[0], uv[1], 1, 1, 1, a},
                           {q[2], q[1], 0, uv[2], uv[1], 1, 1, 1, a},
                           {q[2], q[3], 0, uv[2], uv[3], 1, 1, 1, a},
                           {q[0], q[3], 0, uv[0], uv[3], 1, 1, 1, a}};
    if (!bk_mesh_update(r->renderer, r->meshes[i], v, 4, e))
      return 0;
  }
  r->frame = *f;
  r->width = width;
  r->height = height;
  r->ready = 1;
  return 1;
}
int bk_selection_ui_render_draw(BkSelectionUiRender *r, char e[256]) {
  if (!r || !r->ready)
    return fail(e, "missing prepared frame");
  float matrix[16];
  memcpy(matrix, bk_identity, sizeof(matrix));
  matrix[0] = 2.f / r->width;
  matrix[5] = 2.f / r->height;
  matrix[12] = (float)(1.0 / r->width - 1);
  matrix[13] = (float)(1.0 / r->height - 1);
  for (unsigned i = 0; i < r->frame.count; ++i)
    if (!bk_renderer_draw_mesh(
            r->renderer, r->textures[r->frame.draws[i].slot], r->meshes[i],
            matrix, (BkDrawState){BK_BLEND_ALPHA, 0, BK_CULL_NONE}, e))
      return 0;
  return 1;
}
