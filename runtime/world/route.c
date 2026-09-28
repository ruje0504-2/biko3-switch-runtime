#include "world/route.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
struct BkRoute {
  uint32_t count;
  BkRoutePoint points[1024];
};
static float f32(const uint8_t *p) {
  uint32_t bits = (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 |
                  (uint32_t)p[3] << 24;
  float value;
  memcpy(&value, &bits, 4);
  return value;
}
BkRoute *bk_route_decode(const void *bytes, size_t size, char error[256]) {
  if (!bytes || !size || size > 20480 || size % 20) {
    snprintf(error, 256, "CKP: invalid/truncated record layout");
    return NULL;
  }
  const uint8_t *data = bytes;
  BkRoute *route = calloc(1, sizeof(*route));
  if (!route) {
    snprintf(error, 256, "CKP: allocation failed");
    return NULL;
  }
  for (uint32_t i = 0; i < 1024; i++) {
    BkRoutePoint *point = &route->points[i];
    if (i < size / 20) {
      for (unsigned j = 0; j < 3; j++)
        point->position[j] = f32(data + i * 20 + j * 4);
      point->parameter = f32(data + i * 20 + 12);
      point->flags = data[i * 20 + 16];
    }
    if (!isfinite(point->position[0]) || !isfinite(point->position[1]) ||
        !isfinite(point->position[2]) || !isfinite(point->parameter)) {
      snprintf(error, 256, "CKP: nonfinite active point");
      goto bad;
    }
    if (point->position[0] == 0 && point->position[1] == 0 &&
        point->position[2] == 0 && point->parameter == 0 && !point->flags) {
      if (!i) {
        snprintf(error, 256, "CKP: empty route unsupported");
        goto bad;
      }
      route->count = i;
      return route;
    }
  }
  snprintf(error, 256, "CKP: missing terminator");
bad:
  free(route);
  return NULL;
}
void bk_route_destroy(BkRoute *route) { free(route); }
uint32_t bk_route_count(const BkRoute *route) {
  return route ? route->count : 0;
}
const BkRoutePoint *bk_route_point(const BkRoute *route, uint32_t index) {
  return route && index <= route->count ? &route->points[index] : NULL;
}
int bk_route_set_flags(BkRoute *route, uint32_t index, uint8_t flags) {
  if (!route || index >= route->count)
    return 0;
  BkRoutePoint *point = &route->points[index];
  if (!flags && point->position[0] == 0 && point->position[1] == 0 &&
      point->position[2] == 0 && point->parameter == 0)
    return 0;
  point->flags = flags;
  return 1;
}
