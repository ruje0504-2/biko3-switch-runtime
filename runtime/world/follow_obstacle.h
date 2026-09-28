#ifndef BK_WORLD_FOLLOW_OBSTACLE_H
#define BK_WORLD_FOLLOW_OBSTACLE_H
#include "world/collision.h"
typedef struct {
  float distance, point[3];        /* controller+43c,+524 */
  uint32_t singular_intersections; /* explicit portable diagnostic */
} BkFollowObstacle;
/*4b61e5: triangle normal.Y < float(cos(1.047)); all3 XZ edges in order,
 * no height gate or kind/name exclusion. Query ends at PRE-smoothed camera.
 * Native ignores intersection return and reuses a point across edges within
 * each mesh; (0,0,0) is a miss sentinel. Accept equal/closer XZ distance from
 * NPC body, retain query camera Y in probe, never shorten the query segment.
 * Singular slope crossings would store NaN natively: skip that edge and count
 * it, preserving the previous finite intersection. Invalid data is atomic. */
int bk_follow_obstacle_mesh(BkFollowObstacle *, const BkCollisionMesh *,
                            const float actor[3], const float camera[3],
                            int *hit, char error[256]);
int bk_follow_obstacle_scene(BkFollowObstacle *, const BkCollision *,
                             const float actor[3], const float camera[3],
                             int *hit, char error[256]);
#endif
