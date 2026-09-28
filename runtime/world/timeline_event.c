#include "world/timeline_event.h"
#include <math.h>
int bk_timeline_event(uint8_t *latch, float source, int32_t tick, int8_t mode,
                      int *fired) {
  if (!latch || !fired || !isfinite(source))
    return 0;
  int fire = 0;
  if (mode == 0 || mode == 1) {
    int reached = mode == 0 ? (double)tick <= source : (double)tick >= source;
    if (!reached)
      *latch = 0;
    else if (!*latch) {
      *latch = 1;
      fire = 1;
    }
  }
  *fired = fire;
  return 1;
}
