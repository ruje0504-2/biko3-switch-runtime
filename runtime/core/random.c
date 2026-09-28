#include "core/random.h"
uint16_t bk_random_next(uint32_t *state) {
  if (!state)
    return 0;
  *state = *state * UINT32_C(214013) + UINT32_C(2531011);
  return (uint16_t)((*state >> 16) & UINT32_C(0x7fff));
}
