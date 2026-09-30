#ifndef BK_WORLD_ROUTE_H
#define BK_WORLD_ROUTE_H
#include <stddef.h>
#include <stdint.h>
typedef struct BkRoute BkRoute;
typedef struct {
  float position[3], parameter;
  uint8_t flags;
} BkRoutePoint;
/* CKP is at most 1024 20-byte records; this decoder zero-extends short input.
 * The first zero position/parameter/flag record terminates active movement.
 * Three padding bytes per record are ignored. Decode does not interpret AI. */
BkRoute *bk_route_decode(const void *bytes, size_t size, char error[256]);
void bk_route_destroy(BkRoute *route);
uint32_t bk_route_count(const BkRoute *route);
/* Index count is the preserved zero sentinel. The native actor initializer
 * reads it as a facing target after a one-point route. Larger indices fail. */
const BkRoutePoint *bk_route_point(const BkRoute *route, uint32_t index);
/* Fixed table flag, including slots past the active terminator. Native prop
 * queries use retained NPC last_crossed after an area switch. Does not expose
 * inactive positions or enlarge movement bounds. Index must be <1024;
 * failure leaves *flags unchanged. Active edits are visible here as well. */
int bk_route_flag_slot(const BkRoute *route, uint32_t index, uint8_t *flags);
/* Mutate an entry-owned active point, preserving the decoded terminator and
 * count. Reject clearing a flag-only point (would create an early sentinel). */
int bk_route_set_flags(BkRoute *, uint32_t index, uint8_t flags);
#endif
