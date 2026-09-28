#include "resource/bitmap.h"
#include <stdlib.h>
static void u16(uint8_t *p, uint16_t v) {
  p[0] = (uint8_t)v;
  p[1] = (uint8_t)(v >> 8);
}
static void u32(uint8_t *p, uint32_t v) {
  for (unsigned i = 0; i < 4; ++i)
    p[i] = (uint8_t)(v >> (i * 8));
}
int bk_bitmap_encode(const BkImage *im, BkBlob *out, char e[256]) {
  if (!im || !out || out->data || out->size || !im->rgba || !im->width ||
      !im->height || im->width > 16384 || im->height > 16384) {
    snprintf(e, 256, "bitmap: invalid image/output");
    return 0;
  }
  size_t stride = ((size_t)im->width * 3 + 3) & ~(size_t)3;
  size_t pixels = stride * im->height, bytes = 54 + pixels;
  uint8_t *p = calloc(1, bytes);
  if (!p) {
    snprintf(e, 256, "bitmap: allocation failed");
    return 0;
  }
  p[0] = 'B';
  p[1] = 'M';
  u32(p + 2, (uint32_t)bytes);
  u32(p + 10, 54);
  u32(p + 14, 40);
  u32(p + 18, im->width);
  u32(p + 22, im->height);
  u16(p + 26, 1);
  u16(p + 28, 24);
  u32(p + 34, (uint32_t)pixels);
  u32(p + 38, 0xb12);
  u32(p + 42, 0xb12);
  for (unsigned y = 0; y < im->height; ++y) {
    const uint8_t *src = im->rgba + (size_t)y * im->width * 4;
    uint8_t *dest = p + 54 + (size_t)(im->height - y - 1) * stride;
    for (unsigned x = 0; x < im->width; ++x) {
      dest[x * 3] = src[x * 4 + 2];
      dest[x * 3 + 1] = src[x * 4 + 1];
      dest[x * 3 + 2] = src[x * 4];
    }
  }
  *out = (BkBlob){p, bytes};
  return 1;
}
