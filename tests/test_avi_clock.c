#ifdef NDEBUG
#undef NDEBUG
#endif
#include "media/avi_clock.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
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
  puts("PASS AVI clock0 hold,30fps requests, loop1 then0, signed rewind and "
       "validation");
}
