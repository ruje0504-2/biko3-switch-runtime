#ifdef NDEBUG
#undef NDEBUG
#endif
#include "world/player_wall.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

int main(void) {
  char error[256];
  const float t[3][3] = {{-100, 0, 0}, {100, 30, 0}, {100, 0, 20}};
  BkPlayerWallInput in = {.previous = {0, 0, 3},
                          .motion = {0, -1, 0},
                          .camera = {5, 0, -20},
                          .height = 20};
  for (unsigned i = 0; i < 7; ++i) {
    in.rays[i][0] = 5;
    in.rays[i][2] = -20;
  }
  BkPlayerWall s = {.position = {0, 0, 2}, .camera_distance = 100};
  int near = 0;
  assert(bk_player_wall_triangle(&s, &near, &in, t, error));
  assert(near && s.near_wall && s.position[2] > 4);
  assert(s.normal[0] > 0 && s.normal[1] < 0 && s.normal[2] < 0);
  assert(fabsf(s.normal[0] * s.normal[0] + s.normal[1] * s.normal[1] +
               s.normal[2] * s.normal[2] - 1) < 1e-6f);
  assert(s.camera_distance < 3 && !s.singular_camera);
  for (unsigned i = 0; i < 7; ++i)
    assert(s.rays_blocked[i]);
  /* No movement: contact/normal still update, projected correction holds. */
  s.position[2] = 2;
  memset(in.motion, 0, sizeof(in.motion));
  assert(bk_player_wall_triangle(&s, &near, &in, t, error));
  assert(s.position[2] == 2);
  /* Strict height gates differ by five units between passes. */
  s.position[1] = 20;
  BkPlayerWall before = s;
  assert(bk_player_wall_triangle(&s, &near, &in, t, error));
  assert(!near && !memcmp(&s, &before, sizeof(s)));
  s.position[1] = 0;
  assert(bk_player_wall_restore(s.position, in.previous, 20, t, error));
  assert(s.position[0] == in.previous[0] && s.position[2] == 3);
  /* Axis-perpendicular camera lines: native slope NaN has explicit policy. */
  s.position[0] = 0;
  s.position[2] = 2;
  in.camera[0] = 0;
  assert(bk_player_wall_triangle(&s, &near, &in, t, error));
  assert(s.singular_camera);
  const float vertical_edge[3][3] = {{0, 0, 0}, {0, 30, 0}, {30, 0, 10}};
  s.position[0] = 1;
  s.position[2] = 1;
  assert(bk_player_wall_triangle(&s, &near, &in, vertical_edge, error));
  assert(s.missing_projection == 1);
  before = s;
  near = 71;
  in.height = NAN;
  assert(!bk_player_wall_triangle(&s, &near, &in, t, error));
  assert(near == 71 && !memcmp(&s, &before, sizeof(s)));
  assert(!bk_player_wall_restore(s.position, in.previous, NAN, t, error));
  assert(!memcmp(&s, &before, sizeof(s)));
  puts("player wall response tests passed");
  return 0;
}
