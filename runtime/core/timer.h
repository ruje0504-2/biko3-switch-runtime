#ifndef BK_CORE_TIMER_H
#define BK_CORE_TIMER_H
#include <stdint.h>
/* Original 0x4adbb9 one-shot/rearming timer. Caller supplies low 32 bits of
 * monotonic milliseconds. Keep its signed deadline comparison, including
 * wrap behavior; do not silently replace with a different timer policy. */
typedef struct {
  uint32_t duration, deadline;
  uint8_t armed;
} BkTimer;
int bk_timer_poll(BkTimer *timer, uint32_t now);
#endif
