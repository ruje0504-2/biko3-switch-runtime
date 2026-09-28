#include "world/menu_camera.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
int main(void) {
  char e[256];
  BkMenuCamera s = {.yaw = 0, .pitch = 0, .radius = 20, .height = 18};
  for (unsigned i = 0; i < 16; ++i)
    s.pose.world[i] = i % 5 == 0;
  const float motion[2] = {2, -3};
  assert(bk_menu_camera_orbit(&s, motion, 0, 0, e) && s.yaw == 360);
  assert(bk_menu_camera_orbit(&s, motion, 0, 0, e) && s.yaw == 0);
  assert(bk_menu_camera_orbit(&s, motion, 3, .5f, e));
  assert(s.yaw == 4 && s.pitch == -6 && s.radius == 20 && s.height == 18);
  assert(bk_menu_camera_orbit(&s, motion, 2, .5f, e));
  assert(s.radius == 21 && s.height == 19.5f);
  BkMenuCamera saved = s;
  assert(!bk_menu_camera_orbit(&s, motion, 4, 1, e) &&
         !memcmp(&s, &saved, sizeof(s)));
  const float track[3] = {10, 25, -30}, focus[3] = {0, 18, 0};
  for (unsigned i = 0; i < 1000; ++i) {
    assert(bk_menu_camera_track(&s, track, focus, 0x38, .016f, e));
    assert(s.fov == .4f && !memcmp(s.pose.world, s.matrix, sizeof(s.matrix)));
    assert(
        !memcmp(s.pose.world + 12, s.pose.position, sizeof(s.pose.position)));
  }
  assert(bk_menu_camera_track(&s, track, NULL, 0x10, 0, e) && s.fov == .2f);
  assert(!memcmp(s.focus, focus, sizeof(focus)));
  saved = s;
  assert(!bk_menu_camera_track(&s, track, track, 0x38, 1, e));
  assert(!memcmp(&s, &saved, sizeof(s))); /* Degenerate aim keeps all fields. */
  assert(!bk_menu_camera_track(&s, track, NULL, 0x38, INFINITY, e));
  assert(!memcmp(&s, &saved, sizeof(s)));
  puts("menu camera mode/geometry/atomic failure tests passed");
  return 0;
}
