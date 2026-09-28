#include "ui/text_layout.h"
#include <limits.h>
#include <stdio.h>
#include <string.h>
static int fail(char *error, const char *message) {
  snprintf(error, 256, "text layout: %s", message);
  return 0;
}
static int scan(const BkFont *font, const uint8_t *text, size_t n,
                const BkTextLayout *layout, BkTextGlyph *out, size_t capacity,
                size_t *count, char *error) {
  int64_t x = layout->x, y = layout->y, cell = bk_font_cell(font);
  int64_t right = (int64_t)layout->x + layout->width - cell;
  size_t used = 0;
  if (right < INT32_MIN || right > INT32_MAX)
    return fail(error, "wrap boundary overflow");
  for (size_t i = 0; i < n; i += 2) {
    if (text[i] == '\r') {
      y += layout->step_y;
      x = layout->x;
    } else if (text[i] != '\n') {
      uint16_t code =
          (uint16_t)((uint16_t)text[i] << 8 | (i + 1 < n ? text[i + 1] : 0));
      BkFontGlyph glyph;
      if (!bk_font_glyph(font, code, &glyph) || used == capacity)
        return fail(error, "missing glyph/capacity");
      int64_t gx = x + glyph.offset_x, gy = y + glyph.offset_y;
      if (gx < INT32_MIN || gx > INT32_MAX || gy < INT32_MIN || gy > INT32_MAX)
        return fail(error, "glyph position overflow");
      if (out)
        out[used] = (BkTextGlyph){code, (int32_t)gx, (int32_t)gy, glyph};
      ++used;
      x += layout->step_x ? layout->step_x : glyph.advance + 2;
      if (x < INT32_MIN || x > INT32_MAX)
        return fail(error, "advance overflow");
      if (x > right) {
        x = layout->x;
        y += layout->step_y ? layout->step_y : cell;
      }
    }
    if (y < INT32_MIN || y > INT32_MAX)
      return fail(error, "line position overflow");
  }
  *count = used;
  return 1;
}
int bk_text_layout(const BkFont *font, const uint8_t *text, size_t size,
                   const BkTextLayout *layout, BkTextGlyph *out,
                   size_t capacity, size_t *count, char error[256]) {
  if (!font || !text || !layout || !count || (capacity && !out))
    return fail(error, "missing input/output");
  const uint8_t *nul = memchr(text, 0, size);
  size_t n = nul ? (size_t)(nul - text) : size, used;
  if (!scan(font, text, n, layout, NULL, capacity, &used, error))
    return 0;
  /* The first pass has validated every coordinate and capacity before any
   * output write; callers must not alias text/layout with destination. */
  return scan(font, text, n, layout, out, capacity, count, error);
}
