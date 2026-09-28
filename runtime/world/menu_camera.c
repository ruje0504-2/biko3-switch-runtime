#include "world/menu_camera.h"
#include "world/orbit_internal.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
static int fail(char e[256]) {
  snprintf(e, 256, "menu camera: invalid input/arithmetic/aim geometry");
  return 0;
}
static float clamp(float x, float lo, float hi) {
  return x >= hi ? hi : x <= lo ? lo : x;
}
static int finite_values(const float *v, unsigned count) {
  for (unsigned i = 0; i < count; ++i)
    if (!isfinite(v[i]))
      return 0;
  return 1;
}
int bk_menu_camera_dialogue(BkMenuCamera *s) {
  if (!s)
    return 0;
  s->yaw = s->pitch = 0;
  s->pose.position[0] = s->pose.position[2] = 0;
  s->pose.position[1] = 19;
  const float world[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 19, 0, 1};
  memcpy(s->pose.world, world, sizeof(world));
  s->fov = .2f;
  return 1;
}
int bk_menu_camera_orbit(BkMenuCamera *s, const float motion[2],
                         unsigned buttons, float seconds, char e[256]) {
  if (!s || !motion || !finite_values(motion, 2) || !isfinite(seconds) ||
      seconds < 0 || (buttons & ~3u) || !isfinite(s->yaw) ||
      !isfinite(s->pitch) || !isfinite(s->radius) || !isfinite(s->height))
    return fail(e);
  BkMenuCamera n = *s;
  if (buttons & 1) {
    n.yaw = (float)((double)n.yaw + (double)seconds * 4 * motion[0]);
    n.pitch = (float)((double)n.pitch + (double)seconds * 4 * motion[1]);
  } else if (buttons & 2) {
    n.radius = (float)((double)n.radius + (double)motion[0] * seconds);
    n.height = (float)((double)n.height - (double)motion[1] * seconds);
  }
  if (!isfinite(n.yaw) || !isfinite(n.pitch) || !isfinite(n.radius) ||
      !isfinite(n.height))
    return fail(e);
  if (n.yaw >= 360)
    n.yaw = (float)((double)n.yaw - 360);
  else if (n.yaw <= 0)
    n.yaw = (float)((double)n.yaw + 360);
  n.pitch = clamp(n.pitch, -80, 80);
  n.radius = clamp(n.radius, 5, 100);
  n.height = clamp(n.height, 0, 30);
  if (!bk_orbit_matrix(n.matrix, n.yaw, n.pitch, n.radius, n.height, NULL))
    return fail(e);
  memcpy(n.pose.position, n.matrix + 12, sizeof(n.pose.position));
  memcpy(n.pose.world, n.matrix, sizeof(n.matrix));
  n.fov = .4f;
  *s = n;
  return 1;
}
int bk_menu_camera_track(BkMenuCamera *s, const float track[3],
                         const float *focus, uint8_t flow, float seconds,
                         char e[256]) {
  if (!s || !track || !finite_values(track, 3) ||
      !finite_values(s->pose.position, 3) || !isfinite(seconds) ||
      seconds < 0 || (focus && !finite_values(focus, 3)))
    return fail(e);
  BkMenuCamera n = *s;
  for (unsigned i = 0; i < 3; ++i) {
    float delta = (float)((double)track[i] - s->pose.position[i]);
    delta = (float)((double)seconds * delta);
    n.pose.position[i] = (float)((double)s->pose.position[i] + delta);
  }
  const float fallback[3] = {0, 18, 0};
  memcpy(n.focus, focus ? focus : fallback, sizeof(n.focus));
  if (!finite_values(n.pose.position, 3))
    return fail(e);
  memcpy(n.pose.world + 12, n.pose.position, sizeof(n.pose.position));
  if (!bk_camera_aim(n.pose.world, n.pose.world, n.focus))
    return fail(e);
  memcpy(n.matrix, n.pose.world, sizeof(n.matrix));
  n.fov = flow == 0x10 ? .2f : .4f;
  *s = n;
  return 1;
}
