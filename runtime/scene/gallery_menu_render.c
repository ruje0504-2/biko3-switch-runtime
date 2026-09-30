#include "scene/gallery_menu_render.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
struct BkGalleryMenuRender {
  BkRenderer *renderer;
  BkResourceStore *store;
  BkTexture *textures[51], *retired[49], *draw_textures[BK_GALLERY_DRAWS];
  BkGpuMesh *meshes[BK_GALLERY_DRAWS];
  BkGalleryMenuFrame frame;
  unsigned width, height;
  int ready;
};
static int fail(char e[256], const char *why) {
  snprintf(e, 256, "gallery render: %s", why);
  return 0;
}
void bk_gallery_menu_render_destroy(BkGalleryMenuRender *r) {
  if (!r)
    return;
  for (unsigned i = 0; i < BK_GALLERY_DRAWS; ++i)
    bk_mesh_destroy(r->renderer, r->meshes[i]);
  for (unsigned i = 0; i < 51; ++i)
    bk_texture_destroy(r->renderer, r->textures[i]);
  for (unsigned i = 0; i < 49; ++i)
    bk_texture_destroy(r->renderer, r->retired[i]);
  free(r);
}
BkGalleryMenuRender *bk_gallery_menu_render_create(BkRenderer *renderer,
                                               BkResourceStore *store,
                                               char e[256]) {
  if (!renderer || !store) {
    fail(e, "missing services");
    return NULL;
  }
  BkGalleryMenuRender *r = calloc(1, sizeof(*r));
  if (!r) {
    fail(e, "allocation failed");
    return NULL;
  }
  r->renderer = renderer;
  r->store = store;
  BkBlob raw = {0};
  BkImage image = {0};
  for (unsigned i = 49; i < 51; ++i) {
    const char *name = i == 49 ? "ma_00.tga" : "ma_01.tga";
    BkResourceResult read = bk_resources_read(store, "bk3_00", name, &raw, e);
    if (read != BK_RESOURCE_OK) {
      if (read == BK_RESOURCE_MISSING)
        snprintf(e, 256, "gallery render: missing %s", name);
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
  for (unsigned i = 0; i < BK_GALLERY_DRAWS; ++i)
    if (!(r->meshes[i] = bk_mesh_create(renderer, v, 4, ix, 6, e)))
      goto bad;
  return r;
bad:
  bk_image_free(&image);
  bk_blob_free(&raw);
  bk_gallery_menu_render_destroy(r);
  return NULL;
}
int bk_gallery_menu_render_prepare(BkGalleryMenuRender *r,
                                 const BkGalleryMenuFrame *f, unsigned width,
                                 unsigned height, char e[256]) {
  if (!r || !f || !width || !height || f->count > BK_GALLERY_DRAWS)
    return fail(e, "invalid frame/extent");
  r->ready = 0;
  unsigned tw, th;
  bk_renderer_extent(r->renderer, &tw, &th);
  if (width > tw || height > th)
    return fail(e, "viewport exceeds target");
  for (unsigned i = 0; i < f->count; ++i) {
    const BkGalleryMenuDraw *d = &f->draws[i];
    const float *q = d->corners;
    if (d->slot >= 51)
      return fail(e, "invalid image slot");
    BkTexture *texture = r->textures[d->slot];
    if (!texture && d->slot < 49)
      texture = r->retired[d->slot];
    if (!texture || !isfinite(d->alpha) ||
        d->alpha < 0 || d->alpha > 1 || !isfinite(q[0]) || !isfinite(q[1]) ||
        !isfinite(q[2]) || !isfinite(q[3]) || q[2] < q[0] || q[3] < q[1])
      return fail(e, "invalid draw/missing asset");
    r->draw_textures[i] = texture;
    float a = (uint32_t)((double)d->alpha * 255) / 255.f;
    const BkVertex v[4] = {{q[0], q[1], 0, 0, 0, 1, 1, 1, a},
                           {q[2], q[1], 0, 1, 0, 1, 1, 1, a},
                           {q[2], q[3], 0, 1, 1, 1, 1, 1, a},
                           {q[0], q[3], 0, 0, 1, 1, 1, 1, a}};
    if (!bk_mesh_update(r->renderer, r->meshes[i], v, 4, e))
      return 0;
  }
  r->frame = *f;
  r->width = width;
  r->height = height;
  r->ready = 1;
  return 1;
}
int bk_gallery_menu_render_draw(BkGalleryMenuRender *r, char e[256]) {
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
            r->renderer, r->draw_textures[i], r->meshes[i],
            matrix, (BkDrawState){BK_BLEND_ALPHA, 0, BK_CULL_NONE}, e))
      return 0;
  return 1;
}

void bk_gallery_menu_render_begin(BkGalleryMenuRender *r) {
  if (!r) return;
  r->ready = 0;
  for (unsigned i = 0; i < 49; ++i) {
    bk_texture_destroy(r->renderer, r->retired[i]);
    r->retired[i] = NULL;
  }
}
int bk_gallery_menu_render_image(BkGalleryMenuRender *r, unsigned slot,
                                  const char *name, char e[256]) {
  if (!r || slot >= 49)
    return fail(e, "invalid image owner/slot");
  if (!name) {
    if (r->retired[slot])
      return fail(e, "image retired twice in one tick");
    r->retired[slot] = r->textures[slot];
    r->textures[slot] = NULL;
    return 1;
  }
  if (r->textures[slot])
    return fail(e, "new image would overwrite live owner");
  BkBlob raw = {0};
  BkImage image = {0};
  BkResourceResult read = bk_resources_read(r->store, "bk3_00", name, &raw, e);
  if (read != BK_RESOURCE_OK) {
    if (read == BK_RESOURCE_MISSING)
      snprintf(e, 256, "gallery render: missing %s", name);
    return 0;
  }
  if (!bk_image_decode(raw.data, raw.size, &image, e)) {
    bk_blob_free(&raw);
    return 0;
  }
  BkTexture *texture = bk_texture_create_sampled(r->renderer, &image, BK_WRAP_REPEAT, e);
  bk_blob_free(&raw);
  bk_image_free(&image);
  if (!texture) return 0;
  r->textures[slot] = texture;
  return 1;
}
