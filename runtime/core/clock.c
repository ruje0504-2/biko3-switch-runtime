#include "core/clock.h"
#include <math.h>
#include <string.h>
BkClockFrame bk_clock_present(BkClock *clock, double now) {
  BkClockFrame frame = {0};
  if (!isfinite(now))
    return frame;
  if (!clock->initialized) {
    clock->initialized = 1;
    clock->last_time = now;
    return frame;
  }
  double elapsed = now - clock->last_time;
  if (elapsed <= 0)
    return frame;
  clock->last_time = now;
  if (elapsed > .25) {
    clock->dropped_seconds += elapsed - .25;
    elapsed = .25;
  }
  clock->ticks++;
  return (BkClockFrame){1, elapsed, 0};
}
void bk_clock_init(BkClock *clock) {
  memset(clock, 0, sizeof(*clock));
  clock->step = 1.0 / 60.0;
  clock->max_steps = 8;
}
BkClockFrame bk_clock_advance(BkClock *clock, double now) {
  BkClockFrame frame = {0, clock->step, 0};
  if (!isfinite(now))
    return frame;
  if (!clock->initialized) {
    clock->initialized = 1;
    clock->last_time = now;
    return frame;
  }
  double elapsed = now - clock->last_time;
  if (elapsed < 0)
    return frame;
  clock->last_time = now;
  clock->accumulator += elapsed;
  double budget = clock->step * clock->max_steps;
  if (clock->accumulator > budget) {
    clock->dropped_seconds += clock->accumulator - budget;
    clock->accumulator = budget;
  }
  while (frame.steps < clock->max_steps &&
         clock->accumulator + 1e-12 >= clock->step) {
    clock->accumulator -= clock->step;
    frame.steps++;
    clock->ticks++;
  }
  if (clock->accumulator < 0)
    clock->accumulator = 0;
  frame.alpha = clock->accumulator / clock->step;
  return frame;
}
