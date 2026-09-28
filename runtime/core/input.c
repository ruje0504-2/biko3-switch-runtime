#include "core/input.h"
void bk_input_latch(BkInput *pending, const BkInput *sample) {
  uint32_t pressed = pending->pressed | sample->pressed;
  uint32_t released = pending->released | sample->released;
  *pending = *sample;
  pending->pressed = pressed;
  pending->released = released;
}
BkInput bk_input_consume(BkInput *pending) {
  BkInput result = *pending;
  pending->pressed = pending->released = 0;
  return result;
}
