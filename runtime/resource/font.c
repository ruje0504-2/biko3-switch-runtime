#include "resource/font.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define TABLE_BYTES 0x60000u
#define BITMAP_START (TABLE_BYTES + 4u)
struct BkFont {
  uint8_t *data;
  uint32_t cell;
};
static uint16_t u16(const uint8_t *p) {
  return (uint16_t)(p[0] | (uint16_t)p[1] << 8);
}
static uint32_t u32(const uint8_t *p) {
  return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 |
         (uint32_t)p[3] << 24;
}
static BkFont *fail(char *error, const char *message) {
  snprintf(error, 256, "font: %s", message);
  return NULL;
}
BkFont *bk_font_decode(const void *data, size_t size, char error[256]) {
  if (!data || size < BITMAP_START)
    return fail(error, "truncated header/table");
  const uint8_t *raw = data;
  uint32_t cell = u32(raw);
  if (!cell || cell > 4096)
    return fail(error, "unsupported cell size");
  size_t payload = size - BITMAP_START;
  for (unsigned i = 0; i < 32768; ++i) {
    const uint8_t *r = raw + 4 + i * 12;
    uint32_t offset = u32(r), pitch = u16(r + 4), width = u16(r + 6),
             height = u16(r + 8);
    if (pitch > 512 || height > 4096 || width > 4096 || offset > payload ||
        (size_t)pitch * height > payload - offset)
      return fail(error, "invalid glyph dimensions/bitmap range");
    /* The unused last WORD is retained; the native renderer never reads it.
     * advance width is independent of storage pitch; do not equate them. */
  }
  BkFont *f = calloc(1, sizeof(*f));
  if (!f)
    return fail(error, "allocation failed");
  f->data = malloc(size);
  if (!f->data) {
    free(f);
    return fail(error, "allocation failed");
  }
  memcpy(f->data, raw, size);
  f->cell = cell;
  return f;
}
void bk_font_destroy(BkFont *f) {
  if (f) {
    free(f->data);
    free(f);
  }
}
uint32_t bk_font_cell(const BkFont *f) { return f ? f->cell : 0; }
static int in(uint16_t code, const uint16_t *set, size_t n) {
  for (size_t i = 0; i < n; ++i)
    if (code == set[i])
      return 1;
  return 0;
}
int bk_font_glyph(const BkFont *f, uint16_t code, BkFontGlyph *out) {
  if (!f || !out)
    return 0;
  BkFontGlyph g = {0};
  if (code >= 0x8000) {
    const uint8_t *r = f->data + 4 + (code - 0x8000u) * 12;
    g.row_bytes = u16(r + 4);
    g.width = g.row_bytes * 8;
    g.advance_width = u16(r + 6);
    g.height = u16(r + 8);
    if (g.width && g.height)
      g.bits = f->data + BITMAP_START + u32(r);
  }
  static const uint16_t centered[] = {
      0x8148, 0x8149, 0x8168, 0x8194, 0x8190, 0x8193, 0x8195, 0x8166,
      0x8169, 0x816a, 0x8160, 0x8181, 0x815b, 0x8162, 0x814f, 0x818f,
      0x817b, 0x817c, 0x8175, 0x8176, 0x8177, 0x8178, 0x816f, 0x8170,
      0x8179, 0x817a, 0x8146, 0x8147, 0x8145, 0x8163, 0x8164};
  static const uint16_t top[] = {0x814c, 0x814d, 0x8166, 0x8168,
                                 0x814f, 0x8175, 0x8177};
  static const uint16_t bottom[] = {
      0x8151, 0x8142, 0x8141, 0x8143, 0x8144, 0x8176, 0x8178, 0x829f, 0x82a1,
      0x82a3, 0x82a5, 0x82a7, 0x82c1, 0x82e1, 0x82e3, 0x82e5, 0x8340, 0x8342,
      0x8344, 0x8346, 0x8348, 0x8362, 0x8383, 0x8385, 0x8387};
  int alphanumeric =
      (code >= 0x824f && code <= 0x8258) || (code >= 0x8281 && code <= 0x829a);
  if (alphanumeric || in(code, centered, sizeof(centered) / sizeof(*centered)))
    g.offset_x = ((int32_t)f->cell - (int32_t)g.advance_width) / 2;
  int32_t remaining = (int32_t)f->cell - (int32_t)g.height;
  g.offset_y =
      in(code, top, sizeof(top) / sizeof(*top)) ? 0
      : alphanumeric || in(code, bottom, sizeof(bottom) / sizeof(*bottom))
          ? remaining
          : remaining / 2;
  g.advance = code == 0x8140    ? (int32_t)f->cell / 2
              : g.advance_width ? (int32_t)g.advance_width
                                : (int32_t)f->cell;
  *out = g;
  return 1;
}
