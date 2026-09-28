#include "scene/pause_render.h"
#include "scene/checkpoint_prompt.h"
#include "scene/retry.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
struct BkPauseRender {
  BkRenderer *renderer;
  BkTexture *textures[22];
  BkGpuMesh *meshes[BK_PAUSE_DRAWS];
  BkPauseFrame frame;
  unsigned width, height;
  int ready;
};
static int fail(char *error, const char *why) {
  snprintf(error, 256, "pause render: %s", why);
  return 0;
}
void bk_pause_render_destroy(BkPauseRender *r) {
  if (r) {
    for (unsigned i = 0; i < BK_PAUSE_DRAWS; ++i)
      bk_mesh_destroy(r->renderer, r->meshes[i]);
    for (unsigned i = 0; i < 22; ++i)
      bk_texture_destroy(r->renderer, r->textures[i]);
    free(r);
  }
}
static BkPauseRender *create(BkRenderer *renderer, BkResourceStore *store,
                             const BkImage *capture, int retry,
                             char error[256]) {
  if (!renderer || !store ||
      (!retry &&
       (!capture || !capture->rgba || !capture->width || !capture->height))) {
    fail(error, "missing services");
    return NULL;
  }
  BkPauseRender *r = calloc(1, sizeof(*r));
  if (!r) {
    fail(error, "allocation failed");
    return NULL;
  }
  r->renderer = renderer;
  BkBlob raw = {0};
  BkImage image = {0};
  for (unsigned i = 0; i < 22; ++i) {
    if (retry && i >= (retry == 2 ? 5u : 6u) && i < BK_PAUSE_CURSOR)
      continue;
    if (i == 19) {
      r->textures[i] =
          bk_texture_create_sampled(renderer, capture, BK_WRAP_REPEAT, error);
      if (!r->textures[i])
        goto bad;
      continue;
    }
    const char *name = retry == 2 ? bk_checkpoint_prompt_image(i)
                       : retry    ? bk_retry_image(i)
                                  : bk_pause_image(i);
    BkResourceResult read =
        bk_resources_read(store, "bk3_00", name, &raw, error);
    if (read != BK_RESOURCE_OK) {
      if (read == BK_RESOURCE_MISSING)
        snprintf(error, 256, "pause render: missing %s", name);
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
  for (unsigned i = 0; i < BK_PAUSE_DRAWS; ++i)
    if (!(r->meshes[i] = bk_mesh_create(renderer, v, 4, ix, 6, error)))
      goto bad;
  return r;
bad:
  bk_image_free(&image);
  bk_blob_free(&raw);
  bk_pause_render_destroy(r);
  return NULL;
}
BkPauseRender *bk_pause_render_create(BkRenderer *renderer,
                                      BkResourceStore *store,
                                      const BkImage *capture, char error[256]) {
  return create(renderer, store, capture, 0, error);
}
BkPauseRender *bk_pause_render_create_retry(BkRenderer *renderer,
                                            BkResourceStore *store,
                                            char error[256]) {
  return create(renderer, store, NULL, 1, error);
}
int bk_pause_render_prepare(BkPauseRender *r, const BkPauseFrame *f,
                            unsigned width, unsigned height, char error[256]) {
  if (!r || !f || !width || !height || f->count > BK_PAUSE_DRAWS)
    return fail(error, "invalid frame/extent");
  r->ready = 0;
  unsigned tw, th;
  bk_renderer_extent(r->renderer, &tw, &th);
  if (width > tw || height > th)
    return fail(error, "viewport exceeds target");
  for (unsigned i = 0; i < f->count; ++i) {
    const BkPauseDraw *d = &f->draws[i];
    const float *q = d->rect;
    if (d->slot >= 22 || !r->textures[d->slot] || !isfinite(d->alpha) ||
        d->alpha < 0 || d->alpha > 1 || !isfinite(q[0]) || !isfinite(q[1]) ||
        !isfinite(q[2]) || !isfinite(q[3]) || q[2] <= 0 || q[3] <= 0 ||
        !isfinite(q[0] + q[2]) || !isfinite(q[1] + q[3]))
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
int bk_pause_render_draw(BkPauseRender *r, char error[256]) {
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
            r->renderer, r->textures[r->frame.draws[i].slot], r->meshes[i],
            matrix, (BkDrawState){BK_BLEND_ALPHA, 0, BK_CULL_NONE}, error))
      return 0;
  return 1;
}

BkPauseRender *bk_pause_render_create_checkpoint(BkRenderer *renderer,
                                                 BkResourceStore *store,
                                                 char error[256]) {
  return create(renderer, store, NULL, 2, error);
}
