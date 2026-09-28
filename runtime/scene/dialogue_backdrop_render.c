#include "scene/dialogue_backdrop_render.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
struct BkDialogueBackdropRender {
  BkRenderer *renderer;
  BkResourceStore *store;
  BkTexture *textures[2];
  BkGpuMesh *meshes[2];
  unsigned width, height;
  int replacement, ready;
};
static int fail(char e[256], const char *why) {
  snprintf(e, 256, "dialogue backdrop render: %s", why);
  return 0;
}
static BkTexture *load(BkDialogueBackdropRender *r, const char *name,
                       char e[256]) {
  BkBlob raw = {0};
  BkImage image = {0};
  BkTexture *texture = NULL;
  if (bk_resources_read(r->store, "bk3_00", name, &raw, e) != BK_RESOURCE_OK) {
    fail(e, "image resource unavailable");
    return NULL;
  }
  if (bk_image_decode(raw.data, raw.size, &image, e))
    texture = bk_texture_create_sampled(r->renderer, &image, BK_WRAP_REPEAT, e);
  bk_image_free(&image);
  bk_blob_free(&raw);
  return texture;
}
void bk_dialogue_backdrop_render_destroy(BkDialogueBackdropRender *r) {
  if (!r)
    return;
  for (unsigned i = 0; i < 2; ++i) {
    bk_mesh_destroy(r->renderer, r->meshes[i]);
    bk_texture_destroy(r->renderer, r->textures[i]);
  }
  free(r);
}
BkDialogueBackdropRender *
bk_dialogue_backdrop_render_create(BkRenderer *renderer, BkResourceStore *store,
                                   const char *name, char e[256]) {
  if (!renderer || !store || !name) {
    fail(e, "missing services/name");
    return NULL;
  }
  BkDialogueBackdropRender *r = calloc(1, sizeof(*r));
  if (!r) {
    fail(e, "allocation failed");
    return NULL;
  }
  r->renderer = renderer;
  r->store = store;
  r->textures[0] = load(r, name, e);
  r->textures[1] = load(r, "ma_02.tga", e);
  if (!r->textures[0] || !r->textures[1])
    goto bad;
  const BkVertex v[4] = {{0, 0, 0, 0, 0, 1, 1, 1, 1},
                         {1, 0, 0, 1, 0, 1, 1, 1, 1},
                         {1, 1, 0, 1, 1, 1, 1, 1, 1},
                         {0, 1, 0, 0, 1, 1, 1, 1, 1}};
  const uint16_t indices[] = {0, 1, 2, 3, 0, 2};
  for (unsigned i = 0; i < 2; ++i)
    if (!(r->meshes[i] = bk_mesh_create(renderer, v, 4, indices, 6, e)))
      goto bad;
  return r;
bad:
  bk_dialogue_backdrop_render_destroy(r);
  return NULL;
}
int bk_dialogue_backdrop_render_replace(BkDialogueBackdropRender *r,
                                        const char *name, char e[256]) {
  if (!r || !name)
    return fail(e, "missing owner/name");
  BkTexture *next = load(r, name, e);
  if (!next)
    return 0;
  bk_texture_destroy(r->renderer, r->textures[0]);
  r->textures[0] = next;
  r->replacement = 1;
  r->ready = 0;
  return 1;
}
int bk_dialogue_backdrop_render_prepare(BkDialogueBackdropRender *r,
                                        const BkDialogueBackdropFrame *f,
                                        unsigned width, unsigned height,
                                        char e[256]) {
  if (!r || !f || !width || !height || !isfinite(f->image_alpha) ||
      !isfinite(f->curtain_alpha) || f->image_alpha < 0 || f->image_alpha > 1 ||
      f->curtain_alpha < 0 || f->curtain_alpha > 1)
    return fail(e, "invalid frame/extent");
  unsigned tw, th;
  bk_renderer_extent(r->renderer, &tw, &th);
  if (width > tw || height > th)
    return fail(e, "viewport exceeds target");
  float q[2][2];
  if (!bk_dialogue_backdrop_extent(q[0], width, r->replacement) ||
      !bk_dialogue_backdrop_extent(q[1], width, 0))
    return fail(e, "invalid geometry");
  r->ready = 0;
  for (unsigned i = 0; i < 2; ++i) {
    float alpha = i ? f->curtain_alpha : f->image_alpha;
    float a = (uint32_t)((double)alpha * 255) / 255.f;
    const BkVertex v[4] = {{0, 0, 0, 0, 0, 1, 1, 1, a},
                           {q[i][0], 0, 0, 1, 0, 1, 1, 1, a},
                           {q[i][0], q[i][1], 0, 1, 1, 1, 1, 1, a},
                           {0, q[i][1], 0, 0, 1, 1, 1, 1, a}};
    if (!bk_mesh_update(r->renderer, r->meshes[i], v, 4, e))
      return 0;
  }
  r->width = width;
  r->height = height;
  r->ready = 1;
  return 1;
}
int bk_dialogue_backdrop_render_draw(BkDialogueBackdropRender *r, char e[256]) {
  if (!r || !r->ready)
    return fail(e, "missing prepared frame");
  float matrix[16];
  memcpy(matrix, bk_identity, sizeof(matrix));
  matrix[0] = 2.f / r->width;
  matrix[5] = 2.f / r->height;
  matrix[12] = (float)(1.0 / r->width - 1);
  matrix[13] = (float)(1.0 / r->height - 1);
  for (unsigned i = 0; i < 2; ++i)
    if (!bk_renderer_draw_mesh(
            r->renderer, r->textures[i], r->meshes[i], matrix,
            (BkDrawState){BK_BLEND_ALPHA, 0, BK_CULL_NONE}, e))
      return 0;
  return 1;
}
