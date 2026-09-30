#ifndef BK_MEDIA_AVI_CLOCK_H
#define BK_MEDIA_AVI_CLOCK_H
#include "media/avi.h"
typedef struct {
  float start_seconds;
  uint32_t last_frame;
} BkAviClock;
typedef int (*BkAviClockRead)(void *, int32_t *milliseconds, char error[256]);
/* Original521d78/521ed2 clock(): signed process-elapsed milliseconds, NOT
 * game_seconds or the unsigned GetTickCount timer. Keep float rounding and
 * the separate clock read when restarting a loop. This actual AVI subset
 * starts at0 and has at least2 frames. No decoder/GPU side effects here. */
void bk_avi_clock_init(BkAviClock *, int32_t clock_ms);
/* -1 invalid arguments,0 unchanged,1 request *frame. The request is latched
 * BEFORE decoding/uploading: a failed frame is not retried at the same index.
 * Native conversion keeps the low32 bits of a truncated signed64 value.
 * Out-of-range indices loop to1 and reset start_seconds; the next call can
 * request0. Output frame is untouched on0/-1. */
int bk_avi_clock_select(BkAviClock *, const BkAviInfo *, int32_t clock_ms,
                        int32_t restart_clock_ms, uint32_t *frame,
                        char error[256]);
/* Live clock service: one read per update, a second only on loop restart.
 * A failed read leaves the clock and frame untouched. */
int bk_avi_clock_poll(BkAviClock *, const BkAviInfo *, BkAviClockRead, void *,
                       uint32_t *frame, char error[256]);
#endif
