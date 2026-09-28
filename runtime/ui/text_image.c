#include "ui/text_image.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int bk_text_image(const BkFont *font, const uint8_t *text, size_t size,
                  const BkTextLayout *layout, uint32_t width, uint32_t height,
                  BkImage *out, char error[256]) {
  if (!font || !text || !layout || !out || size > 10240 || !width || !height ||
      width > 4096 || height > 4096) {
    snprintf(error, 256, "text image: invalid input/canvas");
    return 0;
  }
  size_t capacity = size / 2 + 1, count;
  BkTextGlyph *glyphs = malloc(capacity * sizeof(*glyphs));
  BkImage image = {width, height, NULL};
  if (!glyphs) {
    snprintf(error, 256, "text image: allocation failed");
    return 0;
  }
  if (!bk_text_layout(font, text, size, layout, glyphs, capacity, &count,
                      error))
    goto bad;
  image.rgba = malloc((size_t)width * height * 4);
  if (!image.rgba) {
    snprintf(error, 256, "text image: allocation failed");
    goto bad;
  }
  memset(image.rgba, 255, (size_t)width * height * 4);
  for (size_t p = 0; p < (size_t)width * height; ++p)
    image.rgba[p * 4 + 3] = 0;
  for (size_t i = 0; i < count; ++i) {
    const BkTextGlyph *g = &glyphs[i];
    if (!g->glyph.bits)
      continue;
    int64_t left = g->x, top = g->y, right = left + g->glyph.width,
            bottom = top + g->glyph.height;
    int64_t x0 = left < 0 ? 0 : left, y0 = top < 0 ? 0 : top,
            x1 = right > width ? width : right,
            y1 = bottom > height ? height : bottom;
    for (int64_t y = y0; y < y1; ++y)
      for (int64_t x = x0; x < x1; ++x) {
        size_t sx = (size_t)(x - left), sy = (size_t)(y - top);
        uint8_t byte = g->glyph.bits[sy * g->glyph.row_bytes + sx / 8];
        image.rgba[((size_t)y * width + (size_t)x) * 4 + 3] =
            byte & (0x80u >> (sx % 8)) ? 255 : 0;
      }
  }
  free(glyphs);
  *out = image;
  return 1;
bad:
  free(glyphs);
  bk_image_free(&image);
  return 0;
}
