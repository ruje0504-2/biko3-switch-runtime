#include "core/input.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

int main(void) {
  const unsigned sizes[][2] = {{64, 48}, {640, 480}, {1280, 720},
                                {1920, 1080}, {640, 960}};
  char error[256];
  unsigned checks = 0;
  for (unsigned n = 0; n < sizeof(sizes) / sizeof(*sizes); ++n) {
    BkViewport v;
    assert(bk_camera_fit(&v, sizes[n][0], sizes[n][1], 4, 3));
    BkVirtualPointer p = {.position = {v.width * .5f, v.height * .5f}};
    BkVirtualPointer held = p;
    BkInput neutral = {.move_x = .1f, .move_y = .1f,
                        .look_x = 1, .look_y = -1};
    assert(bk_virtual_pointer_step(&p, &v, &neutral, 1.0 / 60, error));
    assert(!memcmp(&p, &held, sizeof(p)));
    BkInput right = {.move_x = 1};
    float results[3];
    for (unsigned rate = 30, i = 0; i < 3; rate *= 2, ++i) {
      p = held;
      for (unsigned step = 0; step < rate / 2; ++step)
        assert(bk_virtual_pointer_step(&p, &v, &right, 1.0 / rate, error));
      results[i] = p.position[0];
      assert(fabsf(results[i] - v.width * .75f) < .004f);
      ++checks;
    }
    p = held;
    assert(bk_virtual_pointer_step(&p, &v, &(BkInput){.move_y = 1}, .25, error));
    assert(p.position[1] < held.position[1] && p.motion[1] < 0);
    p = held;
    assert(bk_virtual_pointer_step(&p, &v,
             &(BkInput){.move_x = 1, .held = BK_BUTTON_SLOW}, .25, error));
    assert(fabsf(p.motion[0] - v.width / 32.f) < .001f);
    p = held;
    assert(bk_virtual_pointer_step(&p, &v,
             &(BkInput){.held = BK_BUTTON_DOWN | BK_BUTTON_RIGHT}, .1, error));
    assert(p.motion[0] > 0 && p.motion[1] > 0);
    assert(fabs(hypot(p.motion[0], p.motion[1]) - .05 * v.width) < .001);
    BkInput touch = {.pointer_active = 1,
                      .pointer_x = v.x + v.width * .2f,
                      .pointer_y = v.y + v.height * .3f, .move_x = 1};
    assert(bk_virtual_pointer_step(&p, &v, &touch, .1, error));
    assert(fabsf(p.position[0] - v.width * .2f) < .001f);
    assert(fabsf(p.position[1] - v.height * .3f) < .001f);
    assert(p.motion[0] == 0 && p.motion[1] == 0);
    touch.pointer_x += 3;
    touch.pointer_y += 2;
    assert(bk_virtual_pointer_step(&p, &v, &touch, .1, error));
    assert(fabsf(p.motion[0] - 3) < .001f && fabsf(p.motion[1] - 2) < .001f);
    touch.pointer_x = -100;
    touch.pointer_y = -100;
    assert(bk_virtual_pointer_step(&p, &v, &touch, .1, error));
    assert(p.position[0] == 0 && p.position[1] == 0);
    assert(bk_virtual_pointer_step(&p, &v,
             &(BkInput){.pointer_motion_x = 10000, .pointer_motion_y = 10000},
             .1, error));
    assert(p.position[0] == v.width - 1 && p.position[1] == v.height - 1);
    assert(bk_virtual_pointer_step(&p, &v, &right, .1, error));
    assert(p.motion[0] == 0 && p.motion[1] == 0);
    held = p;
    assert(!bk_virtual_pointer_step(&p, &v, &(BkInput){.move_x = NAN}, .1, error));
    assert(!bk_virtual_pointer_step(&p, &v, &right, -1, error));
    assert(!bk_virtual_pointer_step(&p, &v, &right, INFINITY, error));
    assert(!bk_virtual_pointer_step(&p, &(BkViewport){0}, &right, .1, error));
    assert(!memcmp(&p, &held, sizeof(p)));
    checks += 16;
  }
  printf("PASS virtual pointer: %u checks, five viewports, 30/60/120Hz, "
         "deadzone, d-pad, slow mode, touch offset/drag, bounds, rollback\n", checks);
  return 0;
}
