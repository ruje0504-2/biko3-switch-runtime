#include "world/placement.h"
#include "world/proximity.h"
#include "world/route.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static void word(uint8_t *p, uint32_t n) {
  for (unsigned i = 0; i < 4; i++)
    p[i] = (uint8_t)(n >> (i * 8));
}
static void proximity(void) {
  const float a[] = {0, 0, 0}, b[] = {10, 500, 0};
  int hit = -1;
  assert(bk_proximity_segment_xz(&hit, a, b, (float[]){5, 900, 15}, 15) && hit);
  assert(bk_proximity_segment_xz(
             &hit, a, b, (float[]){5, 900, nextafterf(15, INFINITY)}, 15) &&
         !hit);
  assert(bk_proximity_segment_xz(&hit, a, b, (float[]){25, -900, 0}, 15) &&
         hit);
  assert(bk_proximity_segment_xz(
             &hit, a, b, (float[]){nextafterf(25, INFINITY), -900, 0}, 15) &&
         !hit);
  assert(bk_proximity_segment_xz(&hit, a, (float[]){0, 100, 0}, a, 15) && !hit);
  assert(bk_proximity_xz(&hit, a, (float[]){3, 100, 4}, 5) && hit);
  hit = 99;
  assert(!bk_proximity_segment_xz(&hit, a, b, a, -1) && hit == 99);
  assert(!bk_proximity_segment_xz(&hit, a, b, (float[]){NAN, 0, 0}, 15) &&
         hit == 99);
  assert(!bk_proximity_xz(&hit, a, NULL, 15) && hit == 99);
  assert(!bk_proximity_xz(&hit, a, b, INFINITY) && hit == 99);
  assert(!bk_proximity_segment_xz(&hit, a, (float[]){FLT_MAX, 0, FLT_MAX},
                                  (float[]){1, 0, 2}, 0) &&
         hit == 99);
}
int main(void) {
  proximity();
  uint8_t data[20480] = {0};
  char error[256];
  assert(!bk_route_decode(data, sizeof(data), error));
  data[16] = 5;
  memset(data + 37, 0xff, 3);  /* ignored padding on the zero sentinel */
  word(data + 40, 0x7fc00000); /* inactive coordinates are not consumed */
  data[56] = 3;              /* inactive flags still belong to the table */
  data[1023 * 20 + 16] = 255;
  BkRoute *route = bk_route_decode(data, sizeof(data), error);
  assert(route && bk_route_count(route) == 1);
  assert(bk_route_point(route, 0)->flags == 5);
  uint8_t flags = 99;
  assert(bk_route_flag_slot(route, 0, &flags) && flags == 5);
  assert(bk_route_flag_slot(route, 1, &flags) && flags == 0);
  assert(bk_route_flag_slot(route, 2, &flags) && flags == 3);
  assert(bk_route_flag_slot(route, 1023, &flags) && flags == 255);
  assert(!bk_route_flag_slot(route, 1024, &flags) && flags == 255);
  assert(!bk_route_flag_slot(route, UINT32_MAX, &flags) && flags == 255);
  assert(!bk_route_flag_slot(NULL, 0, &flags) && flags == 255);
  assert(!bk_route_flag_slot(route, 0, NULL));
  assert(bk_route_set_flags(route, 0, 1));
  assert(bk_route_flag_slot(route, 0, &flags) && flags == 1);
  assert(!bk_route_set_flags(route, 0, 0)); /* Would create early terminator. */
  assert(!bk_route_set_flags(route, 1, 1));
  assert(!bk_route_set_flags(NULL, 0, 1));
  assert(bk_route_count(route) == 1 && bk_route_point(route, 0)->flags == 1);
  assert(bk_route_point(route, 1)->position[0] == 0);
  assert(!bk_route_point(route, 2) && !bk_route_point(route, UINT32_MAX));
  BkActorPlacement placement;
  assert(bk_route_placement(&placement, route, 0, 7.5f));
  assert(placement.position[1] == 7.5f && placement.yaw_degrees == 0);
  BkActorPlacement saved = placement;
  assert(!bk_route_placement(&placement, route, 1, 0));
  assert(!memcmp(&placement, &saved, sizeof(saved)));
  assert(!bk_route_placement(&placement, route, UINT32_MAX, 0));
  assert(!bk_route_placement(&placement, NULL, 0, 0));
  assert(!bk_route_placement(&placement, route, 0, NAN));
  bk_route_destroy(route);
  const float directions[][3] = {
      {0, 1, 0}, {1, 0, 90}, {0, -1, 180}, {-1, 0, 270}};
  for (unsigned i = 0; i < 4; i++) {
    float yaw = -1;
    assert(bk_route_heading(&yaw, 0, 0, directions[i][0], directions[i][1]));
    assert(fabsf(yaw - directions[i][2]) < .00002f);
  }
  float yaw = 123;
  assert(!bk_route_heading(&yaw, NAN, 0, 1, 0) && yaw == 123);
  assert(!bk_route_heading(&yaw, FLT_MAX, 0, -FLT_MAX, 0) && yaw == 123);
  assert(!bk_actor_placement(&placement, (float[]){1, NAN, 3}, 0));
  assert(!bk_actor_placement(&placement, (float[]){1, 2, 3}, INFINITY));
  assert(!memcmp(&placement, &saved, sizeof(saved)));
  route = bk_route_decode(data, 20, error); /* decoder short-input policy */
  assert(route && bk_route_count(route) == 1);
  assert(bk_route_flag_slot(route, 2, &flags) && flags == 0);
  assert(bk_route_flag_slot(route, 1023, &flags) && flags == 0);
  bk_route_destroy(route);
  assert(!bk_route_decode(data, 19, error));
  assert(!bk_route_decode(data, 21, error));
  assert(!bk_route_decode(data, sizeof(data) + 1, error));
  word(data, 0x7fc00000);
  assert(!bk_route_decode(data, sizeof(data), error));
  memset(data, 0, sizeof(data));
  for (unsigned i = 0; i < 1024; i++)
    data[i * 20 + 16] = 1;
  assert(!bk_route_decode(data, sizeof(data), error));
  data[1023 * 20 + 16] = 0;
  route = bk_route_decode(data, sizeof(data), error);
  assert(route && bk_route_count(route) == 1023);
  bk_route_destroy(route);
  uint32_t state = 0x361234;
  for (unsigned n = 0; n < 4000; n++) {
    uint8_t small[100] = {0};
    small[16] = 1;
    for (unsigned i = 0; i < 1 + n % 8; i++) {
      state ^= state << 13;
      state ^= state >> 17;
      state ^= state << 5;
      small[state % sizeof(small)] ^= (uint8_t)(state >> 24);
    }
    route = bk_route_decode(small, sizeof(small), error);
    bk_route_destroy(route);
  }
  puts("PASS: CKP zero extension, sentinel, one-point route, padding, bounds, "
       "4000 mutations");
  return 0;
}
