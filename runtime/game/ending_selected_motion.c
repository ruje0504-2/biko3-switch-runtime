#include "game/ending_selected_motion.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static int fail(char e[256], const char *why) {
  if (e) snprintf(e, 256, "selected ending motion: %s", why);
  return 0;
}
static float difference(int32_t a, int32_t b) {
  uint32_t bits = (uint32_t)a - (uint32_t)b;
  int32_t value;
  memcpy(&value, &bits, sizeof(value));
  return (float)value;
}
static float distance(const int32_t p[2], const int32_t center[2]) {
  float x = difference(p[0], center[0]), y = difference(p[1], center[1]);
  float squared = (float)((double)x * x + (double)y * y);
  return (float)sqrt((double)squared);
}
int bk_ending_selected_motion_pointer(BkEndingSelectedMotionClip *c,
                                      const int32_t target[2],
                                      const int32_t menu[2],
                                      const int32_t pointer[2], char e[256]) {
  if (!c || !target || !menu || !pointer || !isfinite(c->start) ||
      !isfinite(c->end))
    return fail(e, "invalid pointer range/coordinates");
  float length = distance(target, menu), current = distance(pointer, menu);
  float source = c->start;
  if (length > current) {
    float range = (float)((double)c->end - c->start);
    float factor = (float)((double)range / length);
    float offset = (float)(((double)length - current) * factor);
    source = (float)((double)offset + 30.0);
  }
  if (!isfinite(source)) return fail(e, "pointer source overflow");
  c->source = source;
  return 1;
}
int bk_ending_selected_motion_drag(BkEndingSelectedMotionClip *c,
                                   int32_t plain_scheduled, int32_t reverse,
                                   const float motion[2], char e[256]) {
  if (!c || !motion || !isfinite(c->start) || !isfinite(c->end) ||
      !isfinite(c->source) || !isfinite(c->rate) || !isfinite(motion[0]) ||
      (plain_scheduled && !isfinite(motion[1])))
    return fail(e, "invalid drag state/input");
  float coefficient;
  switch (c->duration) {
  case 10: case 15: case 20: coefficient = .00005f; break;
  case 30: case 40: coefficient = .00008f; break;
  case 60: case 80: coefficient = .0002f; break;
  default: coefficient = .0001f; break;
  }
  coefficient = (float)((double)coefficient * .5);
  float span = (float)(((double)c->end - c->start) * coefficient);
  double amount = fabs((double)motion[0]);
  if (plain_scheduled) amount += fabs((double)motion[1]);
  amount = (amount * span) * ((double)c->rate * 60.0);
  float source = (float)((double)c->source +
                         (!plain_scheduled && reverse ? -amount : amount));
  if (!isfinite(source)) return fail(e, "drag source overflow");
  if (!plain_scheduled && reverse && c->start >= source) source = c->start;
  c->source = source;
  return 1;
}
