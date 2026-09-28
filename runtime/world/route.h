#ifndef BK_WORLD_ROUTE_H
#define BK_WORLD_ROUTE_H
#include <stddef.h>
#include <stdint.h>
typedef struct BkRoute BkRoute;
typedef struct {
  float position[3], parameter;
  uint8_t flags;
} BkRoutePoint;
/* Original CKP is at most 1024 20-byte records, zero-extended by the loader.
 * First all-zero numeric record terminates the route; padding is not data.
 * Decode does not interpret parameter/flags as AI behavior. */
BkRoute *bk_route_decode(const void *bytes, size_t size, char error[256]);
void bk_route_destroy(BkRoute *route);
uint32_t bk_route_count(const BkRoute *route);
/* Index count is the preserved zero sentinel. The native actor initializer
 * reads it as a facing target after a one-point route. Larger indices fail. */
const BkRoutePoint *bk_route_point(const BkRoute *route, uint32_t index);
/* Mutate an entry-owned active point, preserving the decoded terminator and
 * count. Reject clearing a flag-only point (would create an early sentinel). */
int bk_route_set_flags(BkRoute *, uint32_t index, uint8_t flags);
#endif
