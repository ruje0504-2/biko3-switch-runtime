#ifndef BK_CLOCK_H
#define BK_CLOCK_H
#include <stdint.h>
typedef struct {
  double step, accumulator, last_time, dropped_seconds;
  uint64_t ticks;
  unsigned max_steps;
  int initialized;
} BkClock;
typedef struct {
  unsigned steps;
  double step_seconds, alpha;
} BkClockFrame;
/* Monotonic seconds in, bounded fixed steps out. No OS or renderer dependency.
 */
void bk_clock_init(BkClock *clock);
BkClockFrame bk_clock_advance(BkClock *clock, double now);
/* Generic wall-time presentation clock (not original game733700):
 * one update per presentation using wall elapsed,
 * never a fixed-step catch-up loop that presents again for each old tick.
 * Only stalls above250ms are clamped (and counted), avoiding unbounded jumps
 * after loading/suspend. Do not mix with advance without reinitializing. */
BkClockFrame bk_clock_present(BkClock *clock, double now);
#endif
