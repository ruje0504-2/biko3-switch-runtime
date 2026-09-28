#ifndef BK_WORLD_PLACEMENT_H
#define BK_WORLD_PLACEMENT_H
#include "world/route.h"
typedef struct {
  float position[3], yaw_degrees, world[16];
} BkActorPlacement;
/* Native 0x4aeba9, including its rounded distance and degree constant.
 * Equal points yield zero. Invalid/nonfinite math leaves output unchanged. */
int bk_route_heading(float *degrees, float x, float z, float next_x,
                     float next_z);
/* Native Y-axis root rotation (0x5234e5) and actor translation. This sets
 * placement only; it does not sample animation or publish child poses. */
int bk_actor_placement(BkActorPlacement *out, const float position[3],
                       float yaw_degrees);
/* 0x4fb93e initializer: X/Z from CKP, Y from caller's spawn table, facing
 * next point (including the zero sentinel after the final active point).
 * Route cursor is explicit: saved/flow state must never be guessed here. */
int bk_route_placement(BkActorPlacement *out, const BkRoute *route,
                       uint32_t index, float height);
#endif
