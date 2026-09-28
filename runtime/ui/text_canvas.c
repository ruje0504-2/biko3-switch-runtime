#include "ui/text_canvas.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
struct BkTextCanvas {
  const BkFont *font;
  BkImage full, viewport;
  uint8_t last[10240], raster[10240];
  size_t last_size, raster_size;
  int ready, raster_ready;
  int32_t x, y;
};
static int fail(char *error, const char *message) {
  snprintf(error, 256, "text canvas: %s", message);
  return 0;
}
BkTextCanvas *bk_text_canvas_create(const BkFont *font, uint32_t width,
                                    uint32_t height, char error[256]) {
  if (!font || !width || !height || width > 640 || height > 480) {
    fail(error, "invalid font/window");
    return NULL;
  }
  BkTextCanvas *c = calloc(1, sizeof(*c));
  if (!c) {
    fail(error, "allocation failed");
    return NULL;
  }
  c->font = font;
  c->viewport = (BkImage){width * 2, height * 2, NULL};
  size_t pixels = (size_t)c->viewport.width * c->viewport.height;
  c->viewport.rgba = calloc(pixels, 4);
  if (!c->viewport.rgba) {
    bk_text_canvas_destroy(c);
    fail(error, "allocation failed");
    return NULL;
  }
  for (size_t p = 0; p < pixels; ++p)
    c->viewport.rgba[p * 4 + 3] = 255;
  return c;
}
void bk_text_canvas_destroy(BkTextCanvas *c) {
  if (c) {
    bk_image_free(&c->full);
    bk_image_free(&c->viewport);
    free(c);
  }
}
int bk_text_canvas_prepare(BkTextCanvas *c, const BkTextStyle *style,
                           const uint8_t *text, size_t size, float seconds,
                           uint32_t target_width, BkTextFlow *flow,
                           BkTextDraw *out, int *upload, char error[256]) {
  if (!c || !style || !text || !flow || !out || !upload ||
      style->width != (int32_t)(c->viewport.width / 2) ||
      style->height != (int32_t)(c->viewport.height / 2))
    return fail(error, "missing input or changed window dimensions");
  const uint8_t *nul = memchr(text, 0, size);
  size_t length = nul ? (size_t)(nul - text) : size;
  if (length >= sizeof(c->last))
    return fail(error, "text exceeds native cache capacity");
  BkTextDraw draw;
  if (!bk_text_draw(style, target_width, &draw))
    return fail(error, "invalid style/target");
  BkTextFlow next = *flow;
  BkTextUpdate update;
  int changed = length != c->last_size || memcmp(text, c->last, length);
  if (!bk_text_flow_step(&next, seconds, changed, &update))
    return fail(error, "invalid flow/time");
  BkImage full = {0};
  if (update.update) {
    if (update.source_y < 0 ||
        (uint32_t)update.source_y > 960 - c->viewport.height)
      return fail(error, "source crop outside full canvas");
    if (!c->raster_ready || length != c->raster_size ||
        memcmp(text, c->raster, length)) {
      if (style->step_x < INT32_MIN / 2 || style->step_x > INT32_MAX / 2 ||
          style->step_y < INT32_MIN / 2 || style->step_y > INT32_MAX / 2)
        return fail(error, "doubled spacing overflow");
      BkTextLayout layout = {0, 0, style->width * 2, style->step_x * 2,
                             style->step_y * 2};
      if (!bk_text_image(c->font, text, length, &layout, 1280, 960, &full,
                         error))
        return 0;
    }
    if (full.rgba) {
      bk_image_free(&c->full);
      c->full = full;
      memcpy(c->raster, text, length);
      c->raster[length] = 0;
      c->raster_size = length;
      c->raster_ready = 1;
    }
    for (size_t y = 0; y < c->viewport.height; ++y)
      for (size_t x = 0; x < c->viewport.width; ++x) {
        uint8_t mask =
            c->full.rgba[((y + (uint32_t)update.source_y) * 1280 + x) * 4 + 3];
        uint8_t *dst = c->viewport.rgba + (y * c->viewport.width + x) * 4;
        dst[0] = dst[1] = dst[2] = mask;
      }
  }
  *upload = update.update || !c->ready || c->x != style->x || c->y != style->y;
  memcpy(c->last, text, length);
  c->last[length] = 0;
  c->last_size = length;
  c->x = style->x;
  c->y = style->y;
  c->ready = 1;
  *flow = next;
  *out = draw;
  return 1;
}
const BkImage *bk_text_canvas_image(const BkTextCanvas *c) {
  return c ? &c->viewport : NULL;
}
