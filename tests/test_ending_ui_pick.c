#include "scene/ending_ui_pick.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static void identity(float m[16]) {
  memset(m, 0, sizeof(float) * 16);
  m[0] = m[5] = m[10] = m[15] = 1;
}
int main(void) {
  float matrix[16];
  identity(matrix);
  int outside = -7, category = -8, hit = -9;
  assert(bk_ending_ui_camera_sector(matrix, 26, -1, 1, &outside, &category));
  assert(outside == 0 && category == 3);
  assert(bk_ending_ui_camera_sector(matrix, 0, 0, 1, &outside, &category));
  assert(outside == 1 && category == 2);
  matrix[8] = NAN;
  assert(!bk_ending_ui_camera_sector(matrix, 0, 0, 1, &outside, &category));
  assert(outside == 1 && category == 2);
  identity(matrix);
  const float a[2] = {0, 0}, z[2] = {100, 0};
  float pointer[2] = {50, 5};
  assert(bk_ending_ui_segment_hit(a, z, pointer, 5, &hit) && !hit);
  assert(bk_ending_ui_segment_hit(a, z, pointer, 6, &hit) && hit);
  assert(bk_ending_ui_segment_hit(a, a, pointer, INFINITY, &hit) && !hit);
  assert(bk_ending_ui_segment_hit(a, z, pointer, INFINITY, &hit) && hit);
  assert(!bk_ending_ui_segment_hit(a, z, pointer, NAN, &hit) && hit);
  float world[39][16], camera[3] = {0, 0, 100};
  uint8_t present[39];
  memset(present, 1, sizeof(present));
  for (unsigned i = 0; i < 39; i++) {
    identity(world[i]);
    world[i][12] = (float)i * 10;
  }
  BkEndingUiPickBindings b = {world,  present, 39,     camera,
                              matrix, matrix,  matrix, 32};
  pointer[0] = 220;
  pointer[1] = 0;
  float distance = 10000;
  int32_t selected = -77;
  char error[256];
  assert(bk_ending_ui_pick_targets(&b, pointer, &distance, &selected, error));
  assert(selected == 0 && distance > 200 && distance < 300);
  float saved = distance;
  assert(bk_ending_ui_pick_targets(&b, pointer, &distance, &selected, error));
  /* The unrounded average comparison may win again after float storage.
   * An exact zero bound instead proves strict retention on a miss. */
  distance = 0;
  assert(bk_ending_ui_pick_targets(&b, pointer, &distance, &selected, error));
  assert(selected == -1 && distance == 0);
  distance = saved;
  selected = 44;
  present[38] = 0;
  assert(!bk_ending_ui_pick_targets(&b, pointer, &distance, &selected, error));
  assert(selected == 44 && distance == saved);
  present[38] = 1;
  b.count = 38;
  assert(!bk_ending_ui_pick_targets(&b, pointer, &distance, &selected, error));
  assert(selected == 44 && distance == saved);
  b.count = 39;
  for (unsigned i = 0; i < 39; i++)
    world[i][15] = 0;
  distance = 10000;
  assert(bk_ending_ui_pick_targets(&b, pointer, &distance, &selected, error));
  assert(selected == -1 && distance == 10000);
  puts("ending UI pick PASS");
  return 0;
}
