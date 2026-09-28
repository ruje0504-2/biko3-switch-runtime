#include "resource/font.h"
#include "ui/text_canvas.h"
#include "ui/text_flow.h"
#include "ui/text_image.h"
#include <math.h>
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void u32(uint8_t *p, uint32_t n) {
  for (unsigned i = 0; i < 4; ++i)
    p[i] = (uint8_t)(n >> (i * 8));
}
int main(void) {
  char error[256];
  const size_t size = 0x60004 + 16;
  uint8_t *raw = calloc(1, size);
  assert(raw);
  raw[0] = 32;
  uint8_t *record = raw + 4 + (0x8281 - 0x8000) * 12;
  record[4] = 1;
  record[6] = 7;
  record[8] = 8;
  raw[0x60004] = 0xa5;
  BkFont *font = bk_font_decode(raw, size, error);
  assert(font && bk_font_cell(font) == 32);
  BkFontGlyph glyph;
  assert(bk_font_glyph(font, 0x8281, &glyph));
  assert(glyph.width == 8 && glyph.height == 8 && glyph.advance_width == 7);
  assert(glyph.offset_x == 12 && glyph.offset_y == 24 && glyph.advance == 7);
  assert(glyph.bits[0] == 0xa5);
  assert(bk_font_glyph(font, 0x8140, &glyph));
  assert(!glyph.bits && glyph.advance == 16);
  assert(bk_font_glyph(font, 0x41, &glyph) && !glyph.bits &&
         glyph.advance == 32);
  const uint8_t text[] = {0x82, 0x81, 13, 10, 0x82, 0x81, 10, 0x82, 0x81, 0};
  BkTextLayout layout = {0, 0, 64, 0, 0};
  BkTextGlyph out[8], before[8];
  memset(out, 0xa5, sizeof(out));
  size_t count = 99;
  assert(
      bk_text_layout(font, text, sizeof(text), &layout, out, 8, &count, error));
  assert(count == 3 && out[0].code == 0x8281 && out[0].x == 12 &&
         out[0].y == 24);
  assert(out[1].x == 12 &&
         out[1].y == 24); /* Explicit CR doesn't use cell fallback. */
  assert(out[2].code == 0x8100 && out[2].x == 9 &&
         out[2].y == 16); /* LF consumes2. */
  memcpy(before, out, sizeof(out));
  count = 91;
  assert(!bk_text_layout(font, text, sizeof(text), &layout, out, 1, &count,
                         error));
  assert(count == 91 && !memcmp(before, out, sizeof(out)));
  layout.x = INT32_MAX;
  assert(!bk_text_layout(font, text, sizeof(text), &layout, out, 8, &count,
                         error));
  assert(count == 91 && !memcmp(before, out, sizeof(out)));
  u32(record, UINT32_MAX);
  assert(!bk_font_decode(raw, size, error));
  u32(record, 12);
  assert(!bk_font_decode(raw, size, error));
  u32(record, 0);
  record[5] = 0xff;
  assert(!bk_font_decode(raw, size, error));
  record[5] = 0;
  assert(!bk_font_decode(raw, 0x60003, error));
  assert(!bk_font_decode(NULL, size, error));
  raw[0] = 0;
  assert(!bk_font_decode(raw, size, error));
  assert(!bk_font_glyph(NULL, 0, &glyph));
  BkImage image = {0};
  layout = (BkTextLayout){0, 0, 64, 32, 32};
  const uint8_t one[] = {0x82, 0x81};
  assert(bk_text_image(font, one, 2, &layout, 64, 64, &image, error));
  assert(image.rgba[(24 * 64 + 12) * 4 + 3] == 255);
  assert(image.rgba[(24 * 64 + 13) * 4 + 3] == 0);
  BkImage saved = image;
  assert(!bk_text_image(font, one, 2, &layout, 4097, 64, &image, error));
  assert(!memcmp(&saved, &image, sizeof(image)));
  bk_image_free(&image);
  BkTextCanvas *canvas = bk_text_canvas_create(font, 32, 32, error);
  assert(canvas);
  BkTextStyle style = {0, 0, 32, 32, 16, 16, 1, 1, {1, 1, 1}, 1};
  BkTextFlow canvas_flow = {0, 0, 0, 1, 0};
  BkTextDraw draw;
  int upload = 0;
  assert(bk_text_canvas_prepare(canvas, &style, one, 2, .1f, 640, &canvas_flow,
                                &draw, &upload, error));
  assert(upload && !canvas_flow.enabled);
  size_t canvas_bytes = 64 * 64 * 4;
  uint8_t *saved_canvas = malloc(canvas_bytes);
  assert(saved_canvas);
  memcpy(saved_canvas, bk_text_canvas_image(canvas)->rgba, canvas_bytes);
  canvas_flow = (BkTextFlow){51, 900, 1000, 1, 1};
  BkTextFlow saved_flow = canvas_flow;
  BkTextDraw saved_draw = draw;
  upload = 77;
  assert(!bk_text_canvas_prepare(canvas, &style, one, 2, 1, 640, &canvas_flow,
                                 &draw, &upload, error));
  assert(upload == 77 &&
         !memcmp(&canvas_flow, &saved_flow, sizeof(saved_flow)) &&
         !memcmp(&draw, &saved_draw, sizeof(draw)));
  assert(
      !memcmp(saved_canvas, bk_text_canvas_image(canvas)->rgba, canvas_bytes));
  style.width = 31;
  assert(!bk_text_canvas_prepare(canvas, &style, one, 2, 0, 640, &canvas_flow,
                                 &draw, &upload, error));
  free(saved_canvas);
  bk_text_canvas_destroy(canvas);
  bk_font_destroy(font);
  free(raw);
  BkTextFlow flow = {49.5f, 0, 10, 1, 1};
  BkTextUpdate update;
  assert(bk_text_flow_step(&flow, .1f, 1, &update));
  assert(flow.delay == 50 && !update.update);
  assert(bk_text_flow_step(&flow, 1, 0, &update));
  assert(flow.delay == 55 && flow.scroll == 10 && flow.enabled == 1 &&
         update.source_y == 10);
  assert(bk_text_flow_step(&flow, .1f, 0, &update));
  assert(flow.enabled == 0 && flow.scroll == 10);
  BkTextFlow prior = flow;
  update = (BkTextUpdate){77, 91};
  assert(!bk_text_flow_step(&flow, NAN, 1, &update));
  assert(!memcmp(&flow, &prior, sizeof(flow)) && update.update == 77 &&
         update.source_y == 91);
  flow = (BkTextFlow){0, 2147483648.f, 2147483648.f, 1, 0};
  prior = flow;
  assert(!bk_text_flow_step(&flow, 0, 1, &update));
  assert(!memcmp(&flow, &prior, sizeof(flow)));
  puts("PASS font bounds/metrics, byte-pair layout, explicit CR/LF, odd byte "
       "and atomic overflow/capacity");
}
