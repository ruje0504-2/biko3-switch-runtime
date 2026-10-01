#include "ui/text_canvas.h"
#include <math.h>
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
  uint32_t width, height, ink_height;
  int32_t source_y;
  float zoom;
};
static int fail(char *error, const char *message) {
  snprintf(error, 256, "text canvas: %s", message);
  return 0;
}
BkTextCanvas *bk_text_canvas_create(const BkFont *font, uint32_t width,
                                    uint32_t height, char error[256]) {
  return bk_text_canvas_create_scaled(font, width, height, 1, error);
}
BkTextCanvas *bk_text_canvas_create_scaled(const BkFont *font, uint32_t width,
                                           uint32_t height, float zoom,
                                           char error[256]) {
  if (!font || !width || !height || width > 640 || height > 480 ||
      !isfinite(zoom) || zoom < 1 || zoom > 2 || width < zoom || height < zoom) {
    fail(error, "invalid font/window");
    return NULL;
  }
  BkTextCanvas *c = calloc(1, sizeof(*c));
  if (!c) {
    fail(error, "allocation failed");
    return NULL;
  }
  c->font = font;
  c->width = width;
  c->height = height;
  c->zoom = zoom;
  c->viewport = (BkImage){(uint32_t)(width / (double)zoom) * 2,
                          (uint32_t)(height / (double)zoom) * 2, NULL};
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
static int raster(BkTextCanvas *c, const BkTextStyle *style,
                   const uint8_t *text, size_t length, BkImage *out,
                   uint32_t *ink_height, char error[256]) {
  if (style->step_x < INT32_MIN / 2 || style->step_x > INT32_MAX / 2 ||
      style->step_y < INT32_MIN / 2 || style->step_y > INT32_MAX / 2)
    return fail(error, "doubled spacing overflow");
  BkTextLayout layout = {0, 0, (int32_t)c->viewport.width, style->step_x * 2,
                         style->step_y * 2};
  return bk_text_image_with_height(c->font, text, length, &layout, 1280, 960,
                                    out, ink_height, error);
}
int bk_text_canvas_prepare(BkTextCanvas *c, const BkTextStyle *style,
                           const uint8_t *text, size_t size, float seconds,
                           uint32_t target_width, BkTextFlow *flow,
                           BkTextDraw *out, int *upload, char error[256]) {
  if (!c || !style || !text || !flow || !out || !upload ||
      style->width != (int32_t)c->width || style->height != (int32_t)c->height)
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
  BkImage full = {0};
  uint32_t ink_height = c->ink_height;
  uint32_t visible_height = c->viewport.height;
  int raster_changed = !c->raster_ready || length != c->raster_size ||
                       memcmp(text, c->raster, length);
  if (c->zoom != 1) {
    if (style->step_y <= 0 || style->step_y > INT32_MAX / 2)
      return fail(error, "translated dialogue needs positive line spacing");
    /* Magnification can leave a fraction of a row at the panel bottom.
     * Keep it blank instead of revealing the next line's upper pixels. */
    visible_height -= visible_height % ((uint32_t)style->step_y * 2);
  }
  if (c->zoom != 1 && next.enabled) {
    if (raster_changed && !raster(c, style, text, length, &full, &ink_height, error))
      return 0;
    uint32_t step = (uint32_t)style->step_y * 2;
    uint32_t excess = ink_height > visible_height ? ink_height - visible_height : 0;
    next.target = (float)((excess + step - 1) / step * step);
    /* A jumps to the last real line without changing the message bytes.
     * Refresh that crop immediately, instead of waiting for a new page. */
    if (!next.started && c->ready && next.scroll != c->source_y)
      changed = 1;
  }
  if (!bk_text_flow_step(&next, seconds, changed, &update)) {
    bk_image_free(&full);
    return fail(error, "invalid flow/time");
  }
  if (update.update) {
    if (update.source_y < 0 ||
        (uint32_t)update.source_y > 960 - c->viewport.height) {
      bk_image_free(&full);
      return fail(error, "source crop outside full canvas");
    }
    if (!full.rgba && raster_changed &&
        !raster(c, style, text, length, &full, NULL, error))
      return 0;
    if (full.rgba) {
      bk_image_free(&c->full);
      c->full = full;
      memcpy(c->raster, text, length);
      c->raster[length] = 0;
      c->raster_size = length;
      c->raster_ready = 1;
      c->ink_height = ink_height;
    }
    for (size_t y = 0; y < c->viewport.height; ++y)
      for (size_t x = 0; x < c->viewport.width; ++x) {
        uint8_t mask = y < visible_height ?
            c->full.rgba[((y + (uint32_t)update.source_y) * 1280 + x) * 4 + 3] : 0;
        uint8_t *dst = c->viewport.rgba + (y * c->viewport.width + x) * 4;
        dst[0] = dst[1] = dst[2] = mask;
      }
    c->source_y = update.source_y;
  } else {
    bk_image_free(&full);
  }
  if (c->zoom != 1) {
    for (unsigned i = 0; i < draw.count; ++i) {
      draw.passes[i].width = (float)(c->viewport.width * (double)c->zoom * target_width / 1280);
      draw.passes[i].height = (float)(c->viewport.height * (double)c->zoom * target_width / 1280);
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
