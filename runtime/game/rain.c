#include "game/rain.h"
#include "core/random.h"
#include <limits.h>
#include <math.h>
#include <string.h>
int bk_rain_initialize(BkRainState *s, unsigned group, unsigned area,
                       int enabled) {
  if (!s || group >= 5 || area >= 9 || (enabled != 0 && enabled != 1))
    return 0;
  BkRainState next = {0};
  if (enabled && group == 4 && area <= 6) {
    next.present = 0xffff;
    memset(next.phase, 1, sizeof(next.phase));
  }
  *s = next;
  return 1;
}
int bk_rain_draw(BkRainState *s, uint32_t *random, int enabled, float scale,
                 BkRainDraw *out) {
  if (!s || !random || !out || s->present > 0xffff ||
      (enabled != 0 && enabled != 1))
    return 0;
  BkRainDraw draw = {0};
  if (!enabled || !s->present) {
    *out = draw;
    return 1;
  }
  double y_limit = 1280.0 * scale, x_limit = 960.0 * scale;
  if (!isfinite(scale) || x_limit < 1 || y_limit >= INT32_MAX)
    return 0;
  BkRainState next = *s;
  uint32_t seed = *random;
  for (unsigned i = 0; i < BK_RAIN_SPRITES; ++i) {
    if (!(s->present & (1u << i)))
      continue;
    if (s->phase[i] < 1 || s->phase[i] > 3)
      return 0;
    next.y[i] = (float)(bk_random_next(&seed) % (uint32_t)y_limit);
    next.x[i] =
        (float)((int32_t)(bk_random_next(&seed) % (uint32_t)x_limit) - 100);
    if (next.phase[i] < 3)
      ++next.phase[i];
    draw.sprites[draw.count++] = (BkRainSprite){i, next.x[i], next.y[i]};
  }
  *s = next;
  *random = seed;
  *out = draw;
  return 1;
}
