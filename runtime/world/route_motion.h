#ifndef BK_WORLD_ROUTE_MOTION_H
#define BK_WORLD_ROUTE_MOTION_H
#include "world/route.h"
typedef struct {
  float position[3], yaw_degrees;
  uint32_t cursor;
  int32_t run_remaining;
  uint32_t segment_start, segment_end, last_crossed;
  uint8_t crossed;
} BkRouteMotion;
/* Original 0x50132f/0x50164b horizontal route movement and facing.
 * A zero cursor leaves state untouched, including prior crossing fields.
 * Crossing beyond the decoded sentinel fails atomically; route ending and
 * point actions belong to the caller. Height is preserved; no collision/AI.
 * Nonzero point flags stop multi-point skipping, not the geometric step. */
int bk_route_motion_step(BkRouteMotion *state, const BkRoute *route,
                         float distance, char error[256]);
#endif
