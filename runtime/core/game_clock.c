#include "core/game_clock.h"
void bk_game_clock_poll(BkGameClock *c, uint32_t now, uint32_t sample) {
  c->step_gate.duration = 1;
  if (bk_timer_poll(&c->step_gate, now)) {
    /* FILD(sample) remains unrounded across FST(last_sample) and FSUB.
     * Only the stored preceding sample is float, as in the original. */
    float seconds = (float)(((double)sample - c->last_sample_ms) / 1000.0);
    c->last_sample_ms = (float)sample;
    if ((double)seconds >= .22) {
      c->clamped_seconds += (double)seconds - .22f;
      seconds = .22f;
    }
    c->seconds = seconds;
  }
  c->fps_gate.duration = 1000;
  if (bk_timer_poll(&c->fps_gate, now)) {
    c->fps = c->frames;
    c->frames = 0;
  }
  ++c->frames;
}
