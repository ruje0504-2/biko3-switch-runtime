#include "media/avi_clock.h"
#include <math.h>
#include <stdio.h>
static float seconds(int32_t ms) { return (float)((double)ms / 1000.0); }
void bk_avi_clock_init(BkAviClock *c, int32_t ms) {
  if (c)
    *c = (BkAviClock){seconds(ms), 0};
}
static int select_frame(BkAviClock *c, const BkAviInfo *info, int32_t ms,
                        int32_t restart, BkAviClockRead read, void *context,
                        uint32_t *frame, char error[256]) {
  if (!c || !info || !frame || !info->scale || info->scale > INT32_MAX ||
      !info->rate || info->frames < 2 || !isfinite(c->start_seconds)) {
    snprintf(error, 256, "AVI clock: invalid state/rate/frame count");
    return -1;
  }
  float elapsed = seconds(ms) - c->start_seconds;
  float rate = (float)((double)info->rate / (int32_t)info->scale);
  /* Both operands have been stored as float, but x87 multiplies before FIST.
   * Double holds their product exactly; avoid an extra float rounding. */
  double product = (double)elapsed * rate;
  /*534398 converts to signed64 and returns its low32 in EAX. A signed32
   * conversion would change large-rate/wrapped-clock frame requests. */
  uint32_t index = product >= 0x1p63 || product < -0x1p63
                       ? 0
                       : (uint32_t)(uint64_t)(int64_t)product;
  if (index == c->last_frame)
    return 0;
  if (index >= info->frames) {
    if (read && !read(context, &restart, error)) return -1;
    c->start_seconds = seconds(restart);
    index = 1;
  }
  c->last_frame = index;
  *frame = index;
  return 1;
}
int bk_avi_clock_select(BkAviClock *c, const BkAviInfo *info, int32_t ms,
                        int32_t restart, uint32_t *frame, char error[256]) {
  return select_frame(c, info, ms, restart, NULL, NULL, frame, error);
}
int bk_avi_clock_poll(BkAviClock *c, const BkAviInfo *info, BkAviClockRead read,
                       void *context, uint32_t *frame, char error[256]) {
  int32_t now;
  if (!read) { snprintf(error, 256, "AVI clock: missing live clock"); return -1; }
  if (!read(context, &now, error)) return -1;
  return select_frame(c, info, now, 0, read, context, frame, error);
}
