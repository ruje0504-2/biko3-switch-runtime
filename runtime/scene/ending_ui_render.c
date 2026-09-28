#include "scene/ending_ui_render.h"
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
struct BkEndingUiRender {
  BkRenderer *renderer;
  BkTexture *textures[BK_ENDING_UI_TOTAL_SPRITES];
  BkGpuMesh *meshes[BK_ENDING_UI_DRAWS];
  BkEndingUiFrame frame;
  unsigned width, height;
  int ready;
  unsigned references;
};
static int fail(char e[256], const char *why) {
  snprintf(e, 256, "ending UI render: %s", why);
  return 0;
}
void bk_ending_ui_render_destroy(BkEndingUiRender *r) {
  if (!r)
    return;
  if (--r->references)
    return;
  for (unsigned i = 0; i < BK_ENDING_UI_DRAWS; ++i)
    bk_mesh_destroy(r->renderer, r->meshes[i]);
  for (unsigned i = 0; i < BK_ENDING_UI_TOTAL_SPRITES; ++i)
    bk_texture_destroy(r->renderer, r->textures[i]);
  free(r);
}
BkEndingUiRender *bk_ending_ui_render_retain(BkEndingUiRender *r) {
  if (!r || r->references == UINT_MAX)
    return NULL;
  r->references++;
  return r;
}
static BkEndingUiRender *create(BkRenderer *renderer, BkResourceStore *store,
                                const BkEndingStageUi *stage, char e[256]) {
  if (!renderer || !store) {
    fail(e, "missing services");
    return NULL;
  }
  BkEndingUiRender *r = calloc(1, sizeof(*r));
  if (!r) {
    fail(e, "allocation failed");
    return NULL;
  }
  r->renderer = renderer;
  r->references = 1;
  BkBlob raw = {0};
  BkImage image = {0};
  for (unsigned i = 0; i < BK_ENDING_UI_TOTAL_SPRITES; ++i) {
    const char *name =
        stage ? bk_ending_stage_ui_image(stage, i) : bk_ending_ui_image(i);
    if (!name)
      continue;
    BkResourceResult read = bk_resources_read(store, "bk3_00", name, &raw, e);
    if (read != BK_RESOURCE_OK) {
      if (read == BK_RESOURCE_MISSING)
        snprintf(e, 256, "ending UI render: missing %s", name);
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
  for (unsigned i = 0; i < BK_ENDING_UI_DRAWS; ++i)
    if (!(r->meshes[i] = bk_mesh_create(renderer, v, 4, ix, 6, e)))
      goto bad;
  return r;
bad:
  bk_image_free(&image);
  bk_blob_free(&raw);
  bk_ending_ui_render_destroy(r);
  return NULL;
}
BkEndingUiRender *bk_ending_ui_render_create(BkRenderer *r, BkResourceStore *s,
                                             char e[256]) {
  return create(r, s, NULL, e);
}
BkEndingUiRender *bk_ending_ui_render_create_stage(BkRenderer *r,
                                                   BkResourceStore *s,
                                                   const BkEndingStageUi *stage,
                                                   char e[256]) {
  if (!stage || !stage->loaded || (stage->loaded & ~0x1fffu)) {
    fail(e, "invalid stage image set");
    return NULL;
  }
  for (unsigned i = 0; i < 13; i++)
    if ((stage->loaded & (1u << i)) && !stage->images[i]) {
      fail(e, "missing stage image identity");
      return NULL;
    }
  return create(r, s, stage, e);
}
int bk_ending_ui_render_prepare(BkEndingUiRender *r, const BkEndingUiFrame *f,
                                unsigned width, unsigned height, char e[256]) {
  if (r)
    r->ready = 0;
  if (!r || !f || !width || !height || f->count > BK_ENDING_UI_DRAWS)
    return fail(e, "invalid frame/extent");
  unsigned tw, th;
  bk_renderer_extent(r->renderer, &tw, &th);
  if (width > tw || height > th)
    return fail(e, "viewport exceeds target");
  for (unsigned i = 0; i < f->count; ++i) {
    const BkEndingUiDraw *d = &f->draws[i];
    const float *q = d->xy;
    const float *uv = d->uv;
    for (unsigned j = 0; j < 4; ++j)
      if (!isfinite(uv[j]))
        return fail(e, "invalid UV");
    if (d->slot >= BK_ENDING_UI_TOTAL_SPRITES || !r->textures[d->slot] ||
        !isfinite(d->alpha) || d->alpha < 0 || d->alpha > 1)
      return fail(e, "invalid draw/missing asset");
    for (unsigned j = 0; j < 8; ++j)
      if (!isfinite(q[j]))
        return fail(e, "invalid quad");
    float alpha = (uint32_t)((double)d->alpha * 255) / 255.f;
    float red = ((d->rgb >> 16) & 255) / 255.f;
    float green = ((d->rgb >> 8) & 255) / 255.f;
    float blue = (d->rgb & 255) / 255.f;
    const BkVertex v[4] = {
        {q[0], q[1], 0, uv[0], uv[1], red, green, blue, alpha},
        {q[2], q[3], 0, uv[2], uv[1], red, green, blue, alpha},
        {q[4], q[5], 0, uv[2], uv[3], red, green, blue, alpha},
        {q[6], q[7], 0, uv[0], uv[3], red, green, blue, alpha}};
    if (!bk_mesh_update(r->renderer, r->meshes[i], v, 4, e))
      return 0;
  }
  r->frame = *f;
  r->width = width;
  r->height = height;
  r->ready = 1;
  return 1;
}
int bk_ending_ui_render_draw_range(BkEndingUiRender *r, unsigned first,
                                   unsigned count, char e[256]) {
  if (!r || !r->ready)
    return fail(e, "missing prepared frame");
  if (first > r->frame.count || count > r->frame.count - first)
    return fail(e, "invalid draw range");
  float matrix[16];
  memcpy(matrix, bk_identity, sizeof(matrix));
  matrix[0] = 2.f / r->width;
  matrix[5] = 2.f / r->height;
  matrix[12] = (float)(1.0 / r->width - 1);
  matrix[13] = (float)(1.0 / r->height - 1);
  for (unsigned i = first; i < first + count; ++i)
    if (!bk_renderer_draw_mesh(
            r->renderer, r->textures[r->frame.draws[i].slot], r->meshes[i],
            matrix, (BkDrawState){BK_BLEND_ALPHA, 0, BK_CULL_NONE}, e))
      return 0;
  return 1;
}

int bk_ending_ui_render_draw(BkEndingUiRender *r, char e[256]) {
  return bk_ending_ui_render_draw_range(r, 0, r ? r->frame.count : 0, e);
}
