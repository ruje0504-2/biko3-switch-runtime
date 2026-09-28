#ifndef BK_WORLD_PROP_ROUTE_H
#define BK_WORLD_PROP_ROUTE_H
#include "world/collision.h"
#include "world/route.h"
typedef struct {
  float position[3], yaw, velocity[3];
  int32_t cursor, first, last;
} BkPropRoute;
typedef struct {
  int32_t crossed, last_crossed, blocked;
} BkPropRouteEffects;
/* 515145: closed route, independently wrapping previous/current indices.
 * A nonzero point flag snaps only when the following segment is completely
 * consumed. Facing uses the OLD position and ORIGINAL previous-previous
 * index even after multiple crossings. Collision holds position/yaw but
 * still commits the advanced route cursor and crossing event. Y is held.
 * Invalid/empty routes and native nonterminating zero-length cycles fail
 * atomically. collision=NULL explicitly means an empty obstacle set. */
int bk_prop_route_step(BkPropRoute *, const BkRoute *, float distance,
                       const BkCollision *, int32_t kind, BkPropRouteEffects *,
                       char error[256]);
#endif
