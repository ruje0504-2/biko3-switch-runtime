#ifndef BK_GAME_CLOCK_H
#define BK_GAME_CLOCK_H
#include "core/timer.h"
/* Original4adc16/4adc96: the1ms rearming gate samples every other frame at
 * ordinary frame rates. Unsampled frames REUSE the preceding game step.
 * This clock is deliberately separate from wall timers and audio cursors. */
typedef struct {
  BkTimer step_gate, fps_gate;
  float last_sample_ms, seconds;
  uint32_t frames, fps;
  double clamped_seconds;
} BkGameClock;
/* Zero-initialize once. Milliseconds share the process-relative wall epoch.
 * sample_ms is the second native GetTickCount read, used only when due. */
void bk_game_clock_poll(BkGameClock *, uint32_t now_ms, uint32_t sample_ms);
#endif
