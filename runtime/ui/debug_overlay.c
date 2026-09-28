#include "ui/debug_overlay.h"
#include <stdlib.h>

/* Small original bitmap alphabet for a permanent development-status overlay. */
static const unsigned char alphabet[36][7] = {
    {14, 17, 17, 31, 17, 17, 17}, {30, 17, 17, 30, 17, 17, 30},
    {14, 17, 16, 16, 16, 17, 14}, {30, 17, 17, 17, 17, 17, 30},
    {31, 16, 16, 30, 16, 16, 31}, {31, 16, 16, 30, 16, 16, 16},
    {14, 17, 16, 23, 17, 17, 15}, {17, 17, 17, 31, 17, 17, 17},
    {14, 4, 4, 4, 4, 4, 14},      {7, 2, 2, 2, 18, 18, 12},
    {17, 18, 20, 24, 20, 18, 17}, {16, 16, 16, 16, 16, 16, 31},
    {17, 27, 21, 21, 17, 17, 17}, {17, 25, 21, 19, 17, 17, 17},
    {14, 17, 17, 17, 17, 17, 14}, {30, 17, 17, 30, 16, 16, 16},
    {14, 17, 17, 17, 21, 18, 13}, {30, 17, 17, 30, 20, 18, 17},
    {15, 16, 16, 14, 1, 1, 30},   {31, 4, 4, 4, 4, 4, 4},
    {17, 17, 17, 17, 17, 17, 14}, {17, 17, 17, 17, 17, 10, 4},
    {17, 17, 17, 21, 21, 27, 17}, {17, 17, 10, 4, 10, 17, 17},
    {17, 17, 10, 4, 4, 4, 4},     {31, 1, 2, 4, 8, 16, 31},
    {14, 17, 19, 21, 25, 17, 14}, {4, 12, 4, 4, 4, 4, 14},
    {14, 17, 1, 2, 4, 8, 31},     {30, 1, 1, 14, 1, 1, 30},
    {2, 6, 10, 18, 31, 2, 2},     {31, 16, 16, 30, 1, 1, 30},
    {14, 16, 16, 30, 17, 17, 14}, {31, 1, 2, 4, 8, 8, 8},
    {14, 17, 17, 14, 17, 17, 14}, {14, 17, 17, 15, 1, 1, 14}};
int bk_debug_font_atlas(BkImage *image, char error[256]) {
  *image = (BkImage){256, 8, calloc(256 * 8, 4)};
  if (!image->rgba) {
    snprintf(error, 256, "debug font allocation failed");
    return 0;
  }
  for (unsigned g = 0; g < 40; ++g)
    for (unsigned y = 0; y < 7; ++y)
      for (unsigned x = 0; x < 5; ++x) {
        int set = g < 36    ? !!(alphabet[g][y] & (16 >> x))
                  : g == 36 ? y == 3
                  : g == 37 ? x == 0 || ((y == 0 || y == 6) && x < 4)
                  : g == 38 ? x == 4 || ((y == 0 || y == 6) && x > 0)
                            : x == 2 && y == 6;
        if (set)
          for (unsigned c = 0; c < 4; ++c)
            image->rgba[(y * 256 + g * 6 + x) * 4 + c] = 255;
      }
  return 1;
}
static void line(BkImage *im, unsigned x, unsigned y, const char *text) {
  for (; *text; text++, x += 12) {
    int index = *text >= 'A' && *text <= 'Z'   ? *text - 'A'
                : *text >= '0' && *text <= '9' ? *text - '0' + 26
                                               : -1;
    for (unsigned r = 0; r < 7; r++)
      for (unsigned c = 0; c < 5; c++) {
        int set = index >= 0 ? !!(alphabet[index][r] & (16 >> c))
                             : (*text == '-' && r == 3);
        if (!set)
          continue;
        for (unsigned sy = 0; sy < 2; sy++)
          for (unsigned sx = 0; sx < 2; sx++) {
            unsigned px = x + c * 2 + sx, py = y + r * 2 + sy;
            if (px < im->width && py < im->height) {
              uint8_t *p = im->rgba + ((size_t)py * im->width + px) * 4;
              p[0] = 255;
              p[1] = 230;
              p[2] = 160;
              p[3] = 255;
            }
          }
      }
  }
}
int bk_debug_banner_create(BkImage *banner, char error[256]) {
  return bk_debug_banner_lines(banner, "BIKO3 - NVK DEVELOPMENT PREVIEW",
                               "A - START FIRST MISSION / PORT IN DEVELOPMENT",
                               error);
}
int bk_debug_banner_lines(BkImage *banner, const char *first,
                          const char *second, char error[256]) {
  banner->width = 1280;
  banner->height = 64;
  banner->rgba = calloc(1280 * 64, 4);
  if (!banner->rgba) {
    snprintf(error, 256, "banner allocation failed");
    return 0;
  }
  for (unsigned i = 0; i < 1280 * 64; i++) {
    banner->rgba[i * 4] = 16;
    banner->rgba[i * 4 + 1] = 20;
    banner->rgba[i * 4 + 2] = 28;
    banner->rgba[i * 4 + 3] = 245;
  }
  line(banner, 24, 10, first);
  line(banner, 24, 36, second);
  return 1;
}
