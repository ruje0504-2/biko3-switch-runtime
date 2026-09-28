#ifdef NDEBUG
#undef NDEBUG
#endif
#include "world/proximity.h"
#include "world/visibility.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
int main(void) {
  float a[] = {0, 1, 0}, b[] = {10, 5, 0}, p[] = {11, 7, 0},
        out[] = {77, 88, 99};
  int hit;
  assert(bk_proximity_segment_xz(&hit, a, b, p, 2) && hit);
  assert(bk_proximity_interior_xz(&hit, a, b, p, 2) && !hit);
  assert(bk_proximity_project_xz(&hit, out, a, b, p, 2) && !hit);
  assert(out[0] == 11 && out[1] == 7 && out[2] == 0);
  float held[3];
  memcpy(held, out, sizeof(out));
  assert(bk_proximity_project_xz(&hit, out, a, a, p, 2) && !hit);
  assert(!memcmp(held, out, sizeof(out)));
  assert(bk_sight_line_side(&hit, a, b, a) && hit);
  b[2] = 10;
  assert(bk_sight_line_side(&hit, a, b, a) && !hit);
  BkSightSegment first = {{0, 0, 0}, {10, 0, 10}, 0},
                 second = {{0, 0, 10}, {10, 0, 0}, 0};
  assert(bk_sight_crossing_point(&hit, out, &first, &second) && hit);
  assert(out[0] == 5 && out[1] == 0 && out[2] == 5);
  memcpy(held, out, sizeof(out));
  first.end[2] = 0;
  second = (BkSightSegment){{5, 0, -5}, {5, 0, 5}, 0};
  assert(bk_sight_crossing(&hit, &first, &second) && hit);
  assert(!bk_sight_crossing_point(&hit, out, &first, &second));
  assert(!memcmp(held, out, sizeof(out)) && hit == 1);
  p[0] = NAN;
  assert(!bk_proximity_project_xz(&hit, out, a, b, p, 2));
  assert(!memcmp(held, out, sizeof(out)));
  puts("PASS edge projection, native side policy and singular intersection "
       "rejection");
}
