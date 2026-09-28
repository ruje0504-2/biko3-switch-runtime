#ifndef BK_WORLD_TIMELINE_EVENT_H
#define BK_WORLD_TIMELINE_EVENT_H
#include <stdint.h>
/* 4afe00: shared latch indexed by SOURCE tick, not clip or actor. Mode0
 * fires once when source>=tick, resets below it; mode1 reverses the test.
 * Other mode bytes do nothing. Caller owns/aliases the exact latch byte. */
int bk_timeline_event(uint8_t *latch, float source, int32_t tick, int8_t mode,
                      int *fired);
#endif
