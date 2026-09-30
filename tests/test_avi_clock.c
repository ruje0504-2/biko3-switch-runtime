#ifdef NDEBUG
#undef NDEBUG
#endif
#include "media/avi_clock.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef struct { int calls, fail_at; int32_t values[2]; } Clock;
static int read_clock(void *p, int32_t *out, char e[256]) {
  Clock *c = p;
  ++c->calls;
  if (c->calls == c->fail_at) { snprintf(e, 256, "injected clock failure"); return 0; }
  assert(c->calls <= 2); *out = c->values[c->calls - 1]; return 1;
}
int main(void) {
  BkAviClock c;
  const BkAviInfo info = {256, 256, 90, 1, 30};
  uint32_t frame = 999;
  char error[256];
  bk_avi_clock_init(&c, 1000);
  assert(!bk_avi_clock_select(&c, &info, 1000, 1000, &frame, error) &&
         frame == 999);
  assert(!bk_avi_clock_select(&c, &info, 1033, 1033, &frame, error));
  assert(bk_avi_clock_select(&c, &info, 1034, 1034, &frame, error) == 1 &&
         frame == 1);
  assert(!bk_avi_clock_select(&c, &info, 1040, 1040, &frame, error));
  assert(bk_avi_clock_select(&c, &info, 4100, 4102, &frame, error) == 1 &&
         frame == 1);
  assert(c.start_seconds == (float)(4102.0 / 1000.0));
  assert(bk_avi_clock_select(&c, &info, 4102, 4102, &frame, error) == 1 &&
         frame == 0);
  assert(bk_avi_clock_select(&c, &info, 4101, 4101, &frame, error) ==
         0); /* trunc(-.03)=0 */
  assert(bk_avi_clock_select(&c, &info, 4000, 4000, &frame, error) == 1 &&
         frame == 1);
  BkAviClock before = c;
  BkAviInfo bad = info;
  bad.scale = 0;
  assert(bk_avi_clock_select(&c, &bad, 5000, 5000, &frame, error) == -1);
  assert(!memcmp(&before, &c, sizeof(c)));
  assert(bk_avi_clock_select(NULL, &info, 0, 0, &frame, error) == -1);
  bad = info;
  bad.rate = UINT32_MAX;
  bk_avi_clock_init(&c, 0);
  assert(!bk_avi_clock_select(&c, &bad, 1000, 1000, &frame, error));
  bk_avi_clock_init(&c, 1000);
  Clock live = {.values = {1000, 4102}};
  assert(!bk_avi_clock_poll(&c, &info, read_clock, &live, &frame, error));
  assert(live.calls == 1);
  live = (Clock){.values = {1034, 4102}};
  assert(bk_avi_clock_poll(&c, &info, read_clock, &live, &frame, error) == 1 && frame == 1);
  assert(live.calls == 1);
  before = c; uint32_t before_frame = frame;
  live = (Clock){.fail_at = 2, .values = {4100, 4102}};
  assert(bk_avi_clock_poll(&c, &info, read_clock, &live, &frame, error) == -1);
  assert(live.calls == 2 && !memcmp(&before, &c, sizeof c) && frame == before_frame);
  live = (Clock){.values = {4100, 4102}};
  assert(bk_avi_clock_poll(&c, &info, read_clock, &live, &frame, error) == 1 && frame == 1);
  assert(live.calls == 2 && c.start_seconds == (float)(4102.0 / 1000.0));
  puts("PASS AVI clock0 hold,30fps requests, loop1 then0, signed rewind and "
       "validation");
}
