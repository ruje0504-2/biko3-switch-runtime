#ifndef BK_WORLD_VISIBILITY_H
#define BK_WORLD_VISIBILITY_H
#include <stdint.h>
typedef struct {
  float start[3], end[3], distance;
} BkSightSegment;
typedef struct {
  float actor_position[3], player_position[3], head_distance, facing;
  int32_t actor_kind, player_action, short_range_action;
} BkSightInput;
/* 0x4ae8ff: inclusive XZ segment intersection, except parallel/collinear
 * segments always miss. Height and distance are not used. */
int bk_sight_crossing(int *hit, const BkSightSegment *a,
                      const BkSightSegment *b);
/* 4aed7a writes X/Z intersection and Y0 only on crossing. Preserve original
 * slope-axis selection and float intermediates. Parallel miss leaves point.
 * Original can produce NaN for singular slope choices (e.g. axis-aligned
 * perpendicular pair); those explicitly fail atomically instead of storing
 * nonfinite world coordinates. This is not a general robust line solver. */
int bk_sight_crossing_point(int *hit, float point[3], const BkSightSegment *a,
                            const BkSightSegment *b);
/* 4ae1d7's original side classification. Axis-aligned cases are inclusive;
 * non-axis case is strictly point.Z above the rounded slope/intercept line.
 * Degenerate line returns0. Not oriented-cross-product winding. */
int bk_sight_line_side(int *side, const float a[3], const float b[3],
                       const float point[3]);
/* 0x518cd3: overlap in endpoint height ranges plus XZ edge intersection.
 * This deliberately preserves the original broad vertical test; it is not
 * a conventional 3D ray/triangle intersection. */
int bk_sight_triangle(int *blocked, const float triangle[3][3],
                      const BkSightSegment *sight);
/* 0x518bb0 visibility cone before scene occlusion. Actor kind is original
 * actor+0xc. Native computes but never uses a normalized facing temporary:
 * the actual comparison uses the raw heading difference (no seam wrap). */
int bk_sight_cone(int *visible, const BkSightInput *input);
#endif
