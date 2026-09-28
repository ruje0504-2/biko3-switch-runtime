#include "core/timer.h"
int bk_timer_poll(BkTimer *timer, uint32_t now) {
  if (!timer)
    return 0;
  if (!timer->armed) {
    timer->deadline = now + timer->duration;
    timer->armed = 1;
  }
  if ((now ^ UINT32_C(0x80000000)) < (timer->deadline ^ UINT32_C(0x80000000)))
    return 0;
  timer->deadline = 0;
  timer->armed = 0;
  return 1;
}
