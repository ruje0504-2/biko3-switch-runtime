#include "game/ending_tertiary_motion.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
static int fail(char e[256], const char *why) {
  if (e) snprintf(e, 256, "tertiary ending motion: %s", why);
  return 0;
}
BkEndingTertiaryMotionState bk_ending_tertiary_motion_initial(void) {
  return (BkEndingTertiaryMotionState){{1, 1}};
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
int bk_ending_tertiary_motion_pointer(BkEndingTertiaryMotionClip *c,
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
    source = (float)((double)offset + c->start);
  }
  if (!isfinite(source)) return fail(e, "pointer source overflow");
  c->source = source;
  return 1;
}
int bk_ending_tertiary_motion_drag(BkEndingTertiaryMotionState *s,
                                   unsigned group, unsigned mode, float seconds,
                                   const int32_t motion[2],
                                   BkEndingTertiaryMotionClip *c, char e[256]) {
  if (!s || !c || !motion || group >= 5 || mode >= 2 || !isfinite(seconds) ||
      seconds < 0 || !isfinite(c->start) || !isfinite(c->end) ||
      !isfinite(c->source) || !isfinite(c->rate))
    return fail(e, "invalid drag state/input");
  static const int32_t speed[5] = {500, 50, 50, 200, 200};
  double bound = (double)speed[group] * seconds;
  float delta[2] = {(float)motion[0], (float)motion[1]};
  for (unsigned i = 0; i < 2; ++i) {
    if (bound < delta[i]) delta[i] = (float)bound;
    else if (-bound > delta[i]) delta[i] = (float)-bound;
  }
  float factor = (float)((((double)c->end - c->start) / 40.0) *
                         c->duration * (double).0002f);
  double amount = ((fabs((double)delta[0]) + fabs((double)delta[1])) * factor) *
                  ((double)seconds * c->rate * 60.0);
  float source = (float)((double)c->source +
                          (s->direction[mode] ? amount : -amount));
  if (!isfinite(source)) return fail(e, "drag source overflow");
  int32_t direction = s->direction[mode];
  if (c->start >= source) {
    source = c->start;
    direction = 1;
  }
  if (c->end <= source) direction = 0;
  c->source = source;
  c->elapsed = 0;
  s->direction[mode] = direction;
  return 1;
}
