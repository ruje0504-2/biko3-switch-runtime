#include "scene/volume_menu_render.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
struct BkVolumeMenuRender {
  BkRenderer *renderer;
  BkTexture *textures[BK_VOLUME_SPRITES];
  BkGpuMesh *meshes[BK_VOLUME_DRAWS];
  BkVolumeMenuFrame frame;
  unsigned width, height;
  int ready;
};
static int fail(char e[256], const char *why) {
  snprintf(e, 256, "volume render: %s", why);
  return 0;
}
void bk_volume_menu_render_destroy(BkVolumeMenuRender *r) {
  if (!r)
    return;
  for (unsigned i = 0; i < BK_VOLUME_DRAWS; ++i)
    bk_mesh_destroy(r->renderer, r->meshes[i]);
  for (unsigned i = 0; i < BK_VOLUME_SPRITES; ++i) {
    unsigned j = 0;
    while (j < i && r->textures[j] != r->textures[i]) ++j;
    if (j == i) bk_texture_destroy(r->renderer, r->textures[i]);
  }
  free(r);
}
BkVolumeMenuRender *bk_volume_menu_render_create(BkRenderer *renderer,
                                               BkResourceStore *store,
                                               char e[256]) {
  if (!renderer || !store) {
    fail(e, "missing services");
    return NULL;
  }
  BkVolumeMenuRender *r = calloc(1, sizeof(*r));
  if (!r) {
    fail(e, "allocation failed");
    return NULL;
  }
  r->renderer = renderer;
  BkBlob raw = {0};
  BkImage image = {0};
  for (unsigned i = 0; i < BK_VOLUME_SPRITES; ++i) {
    const char *name = bk_volume_menu_image(i);
    unsigned prior = 0;
    while (prior < i && strcmp(name, bk_volume_menu_image(prior))) ++prior;
    if (prior < i) {
      r->textures[i] = r->textures[prior];
      continue;
    }
    BkResourceResult read = bk_resources_read(store, "bk3_00", name, &raw, e);
    if (read != BK_RESOURCE_OK) {
      if (read == BK_RESOURCE_MISSING)
        snprintf(e, 256, "volume render: missing %s", name);
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
  for (unsigned i = 0; i < BK_VOLUME_DRAWS; ++i)
    if (!(r->meshes[i] = bk_mesh_create(renderer, v, 4, ix, 6, e)))
      goto bad;
  return r;
bad:
  bk_image_free(&image);
  bk_blob_free(&raw);
  bk_volume_menu_render_destroy(r);
  return NULL;
}
int bk_volume_menu_render_prepare(BkVolumeMenuRender *r,
                                 const BkVolumeMenuFrame *f, unsigned width,
                                 unsigned height, char e[256]) {
  if (!r || !f || !width || !height || f->count > BK_VOLUME_DRAWS)
    return fail(e, "invalid frame/extent");
  r->ready = 0;
  unsigned tw, th;
  bk_renderer_extent(r->renderer, &tw, &th);
  if (width > tw || height > th)
    return fail(e, "viewport exceeds target");
  for (unsigned i = 0; i < f->count; ++i) {
    const BkVolumeMenuDraw *d = &f->draws[i];
    const float *q = d->corners;
    if (d->slot >= BK_VOLUME_SPRITES || !r->textures[d->slot] || !isfinite(q[0]) || !isfinite(q[1]) ||
        !isfinite(q[2]) || !isfinite(q[3]) || q[2] < q[0] || q[3] < q[1])
      return fail(e, "invalid draw/missing asset");
    const float a = 1;
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
int bk_volume_menu_render_draw(BkVolumeMenuRender *r, char e[256]) {
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
