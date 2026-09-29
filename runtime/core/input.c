#include "core/input.h"
#include <math.h>
#include <stdio.h>
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
static float pointer_clamp(double value, unsigned extent) {
  return (float)fmin(fmax(value, 0.0), (double)extent - 1.0);
}
int bk_virtual_pointer_step(BkVirtualPointer *p, const BkViewport *viewport,
                             const BkInput *input, double seconds,
                             char error[256]) {
  if (!p || !viewport || !input || !viewport->width || !viewport->height ||
      !isfinite(seconds) || seconds < 0 || seconds > 1 ||
      !isfinite(p->position[0]) || !isfinite(p->position[1]) ||
      !isfinite(input->move_x) || !isfinite(input->move_y) ||
      !isfinite(input->pointer_motion_x) ||
      !isfinite(input->pointer_motion_y) ||
      (input->pointer_active &&
       (!isfinite(input->pointer_x) || !isfinite(input->pointer_y)))) {
    if (error)
      snprintf(error, 256, "pointer: invalid viewport/input/time");
    return 0;
  }
  BkVirtualPointer next = *p;
  double x = p->position[0], y = p->position[1];
  if (input->pointer_active) {
    x = (double)input->pointer_x - viewport->x;
    y = (double)input->pointer_y - viewport->y;
  } else {
    double sx = input->move_x, sy = -input->move_y;
    double length = hypot(sx, sy);
    if (length <= .18) {
      sx = sy = 0;
    } else {
      double speed = (fmin(length, 1.0) - .18) / .82;
      sx = sx / length * speed;
      sy = sy / length * speed;
    }
    sx += !!(input->held & BK_BUTTON_RIGHT) -
          !!(input->held & BK_BUTTON_LEFT);
    sy += !!(input->held & BK_BUTTON_DOWN) -
          !!(input->held & BK_BUTTON_UP);
    length = hypot(sx, sy);
    if (length > 1) {
      sx /= length;
      sy /= length;
    }
    double speed = .5 * viewport->width * seconds;
    if (input->held & BK_BUTTON_SLOW)
      speed *= .25;
    x += input->pointer_motion_x + sx * speed;
    y += input->pointer_motion_y + sy * speed;
  }
  next.position[0] = pointer_clamp(x, viewport->width);
  next.position[1] = pointer_clamp(y, viewport->height);
  for (unsigned i = 0; i < 2; ++i)
    next.motion[i] = input->pointer_active && !p->absolute_active
                         ? 0 : next.position[i] - p->position[i];
  next.absolute_active = input->pointer_active != 0;
  *p = next;
  return 1;
}
