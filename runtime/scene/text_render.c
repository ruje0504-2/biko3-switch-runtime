#include "scene/text_render.h"
#include <stdlib.h>
#include <string.h>
struct BkTextRender {
  BkRenderer *renderer;
  BkFont *font;
  BkTextCanvas *canvas;
  BkTexture *texture;
  BkGpuMesh *meshes[4];
  BkTextDraw draw;
  unsigned width, height;
  int ready;
};
static int fail(char *error, const char *message) {
  snprintf(error, 256, "text render: %s", message);
  return 0;
}
void bk_text_render_destroy(BkTextRender *t) {
  if (!t)
    return;
  for (unsigned i = 0; i < 4; ++i)
    bk_mesh_destroy(t->renderer, t->meshes[i]);
  bk_texture_destroy(t->renderer, t->texture);
  bk_text_canvas_destroy(t->canvas);
  bk_font_destroy(t->font);
  free(t);
}
BkTextRender *bk_text_render_create(BkRenderer *renderer,
                                    BkResourceStore *store, const char *font,
                                    uint32_t width, uint32_t height,
                                    char error[256]) {
  return bk_text_render_create_scaled(renderer, store, font, width, height, 1,
                                       error);
}
BkTextRender *bk_text_render_create_scaled(BkRenderer *renderer,
                                           BkResourceStore *store,
                                           const char *font, uint32_t width,
                                           uint32_t height, float zoom,
                                           char error[256]) {
  if (!renderer || !store || !font) {
    fail(error, "missing services/font");
    return NULL;
  }
  BkTextRender *t = calloc(1, sizeof(*t));
  if (!t) {
    fail(error, "allocation failed");
    return NULL;
  }
  t->renderer = renderer;
  BkBlob blob = {0};
  BkResourceResult result =
      bk_resources_read(store, "fonts", font, &blob, error);
  if (result != BK_RESOURCE_OK) {
    if (result == BK_RESOURCE_MISSING)
      fail(error, "missing FTT resource");
    goto bad;
  }
  t->font = bk_font_decode(blob.data, blob.size, error);
  bk_blob_free(&blob);
  if (!t->font ||
      !(t->canvas = bk_text_canvas_create_scaled(t->font, width, height, zoom,
                                                 error)))
    goto bad;
  const BkVertex vertices[4] = {{0, 0, 0, 0, 0, 1, 1, 1, 1},
                                {1, 0, 0, 1, 0, 1, 1, 1, 1},
                                {1, 1, 0, 1, 1, 1, 1, 1, 1},
                                {0, 1, 0, 0, 1, 1, 1, 1, 1}};
  const uint16_t indices[] = {0, 1, 2, 0, 2, 3};
  for (unsigned i = 0; i < 4; ++i) {
    t->meshes[i] = bk_mesh_create(renderer, vertices, 4, indices, 6, error);
    if (!t->meshes[i])
      goto bad;
  }
  return t;
bad:
  bk_blob_free(&blob);
  bk_text_render_destroy(t);
  return NULL;
}
int bk_text_render_prepare(BkTextRender *t, const BkTextStyle *style,
                           const uint8_t *text, size_t size, float seconds,
                           unsigned width, unsigned height, BkTextFlow *flow,
                           char error[256]) {
  if (!t)
    return fail(error, "missing instance");
  unsigned target_width, target_height;
  bk_renderer_extent(t->renderer, &target_width, &target_height);
  if (!width || !height || width > target_width || height > target_height)
    return fail(error, "invalid viewport extent");
  BkTextDraw draw;
  int upload;
  if (!bk_text_canvas_prepare(t->canvas, style, text, size, seconds, width,
                              flow, &draw, &upload, error))
    return 0;
  if (!t->texture) {
    BkTexture *texture =
        bk_texture_create(t->renderer, bk_text_canvas_image(t->canvas), error);
    if (!texture)
      return 0;
    t->texture = texture;
  } else if (upload && !bk_texture_update(t->renderer, t->texture,
                                          bk_text_canvas_image(t->canvas), error))
    return 0;
  for (unsigned i = 0; i < draw.count; ++i) {
    const BkTextPass *p = &draw.passes[i];
    float r = ((p->argb >> 16) & 255) / 255.f,
          g = ((p->argb >> 8) & 255) / 255.f, b = (p->argb & 255) / 255.f;
    BkVertex v[4] = {{p->x, p->y, 0, 0, 0, r, g, b, 1},
                     {p->x + p->width, p->y, 0, 1, 0, r, g, b, 1},
                     {p->x + p->width, p->y + p->height, 0, 1, 1, r, g, b, 1},
                     {p->x, p->y + p->height, 0, 0, 1, r, g, b, 1}};
    if (!bk_mesh_update(t->renderer, t->meshes[i], v, 4, error))
      return 0;
  }
  t->draw = draw;
  t->width = width;
  t->height = height;
  t->ready = 1;
  return 1;
}
int bk_text_render_draw(BkTextRender *t, char error[256]) {
  if (!t || !t->ready)
    return fail(error, "missing prepared draw");
  float matrix[16];
  memcpy(matrix, bk_identity, sizeof(matrix));
  matrix[0] = 2.f / t->width;
  matrix[5] = 2.f / t->height;
  /* Original transformed vertices address integer pixel centers. */
  matrix[12] = (float)(1.0 / t->width - 1);
  matrix[13] = (float)(1.0 / t->height - 1);
  for (unsigned i = 0; i < t->draw.count; ++i) {
    BkBlend blend = t->draw.passes[i].blend == BK_TEXT_ADD
                        ? BK_BLEND_ADDITIVE
                        : BK_BLEND_INVERSE_COLOR;
    if (!bk_renderer_draw_mesh(t->renderer, t->texture, t->meshes[i], matrix,
                               (BkDrawState){blend, 0, BK_CULL_NONE}, error))
      return 0;
  }
  return 1;
}
const BkTextDraw *bk_text_render_snapshot(const BkTextRender *t) {
  return t && t->ready ? &t->draw : NULL;
}
const BkImage *bk_text_render_image(const BkTextRender *t) {
  return t ? bk_text_canvas_image(t->canvas) : NULL;
}
