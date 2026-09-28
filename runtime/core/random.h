#ifndef BK_CORE_RANDOM_H
#define BK_CORE_RANDOM_H
#include <stdint.h>
/* Original 0x534a34 shared 32-bit state; callers own/serialize the state. */
uint16_t bk_random_next(uint32_t *state);
#endif
