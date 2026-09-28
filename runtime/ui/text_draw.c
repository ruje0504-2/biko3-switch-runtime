#include "ui/text_draw.h"
#include <math.h>
static int coordinate(int32_t value, float scale, int32_t *out) {
  double v = (double)value * scale;
  if (v < -2147483648. || v >= 2147483648.)
    return 0;
  *out = (int32_t)v;
  return 1;
}
static uint32_t rgb(const float color[3]) {
  uint32_t bits = 0xff000000u;
  for (unsigned i = 0; i < 3; ++i)
    bits |= (uint32_t)((double)color[i] * 255) << (16 - 8 * i);
  return bits;
}
int bk_text_draw(const BkTextStyle *s, uint32_t target_width, BkTextDraw *out) {
  if (!s || !out || !target_width || target_width > 16384 || s->width <= 0 ||
      s->height <= 0 || !isfinite(s->opacity) || s->opacity < 0 ||
      s->opacity > 1 || !isfinite(s->shadow_distance))
    return 0;
  for (unsigned i = 0; i < 3; ++i)
    if (!isfinite(s->color[i]) || s->color[i] < 0 || s->color[i] > 1)
      return 0;
  float scale = (float)((double)target_width / 640);
  int32_t x, y, width, height;
  if (!coordinate(s->x, scale, &x) || !coordinate(s->y, scale, &y) ||
      !coordinate(s->width, scale, &width) ||
      !coordinate(s->height, scale, &height))
    return 0;
  float offset = (float)((double)scale * s->shadow_distance);
  if (!isfinite(offset))
    return 0;
  float white[3] = {s->opacity, s->opacity, s->opacity}, inverse[3];
  for (unsigned i = 0; i < 3; ++i)
    inverse[i] = (float)((1.0 - (double)s->color[i]) * s->opacity);
  BkTextDraw d = {0};
  BkTextPass base = {BK_TEXT_DARKEN, (float)x,      (float)y,
                     (float)width,   (float)height, rgb(white)};
  if (s->shadow) {
    BkTextPass shadow = base;
    shadow.x = (float)((double)x + offset);
    shadow.y = (float)((double)y + offset);
    if (!isfinite(shadow.x) || !isfinite(shadow.y))
      return 0;
    d.passes[d.count++] = shadow;
  }
  d.passes[d.count++] = base;
  base.blend = BK_TEXT_ADD;
  d.passes[d.count++] = base;
  base.blend = BK_TEXT_DARKEN;
  base.argb = rgb(inverse);
  d.passes[d.count++] = base;
  *out = d;
  return 1;
}
