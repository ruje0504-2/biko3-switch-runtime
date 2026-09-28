#include "media/avi_surface.h"
#include <stdio.h>
int bk_avi_surface_rgba(const uint16_t *source, uint32_t width, uint32_t height,
                        int green565, uint8_t *rgba, size_t bytes,
                        char error[256]) {
  if (!source || !rgba || !width || !height || width > 2048 || height > 2048 ||
      width % 4 || height % 4 || (green565 != 0 && green565 != 1) ||
      bytes != (size_t)width * height * 4) {
    snprintf(error, 256, "AVI surface: invalid image/size/format");
    return 0;
  }
  size_t count = (size_t)width * height;
  for (size_t i = 0; i < count; i++) {
    size_t dib = i + 2;
    uint16_t rgb =
        dib < count ? source[(height - 1 - dib / width) * width + dib % width]
                    : 0;
    unsigned r = (rgb >> 10) & 31, g = (rgb >> 5) & 31, b = rgb & 31;
    rgba[4 * i] = (uint8_t)((r * 255 + 15) / 31);
    rgba[4 * i + 1] =
        (uint8_t)(green565 ? (g * 2 * 255 + 31) / 63 : (g * 255 + 15) / 31);
    rgba[4 * i + 2] = (uint8_t)((b * 255 + 15) / 31);
    rgba[4 * i + 3] = 255;
  }
  return 1;
}
