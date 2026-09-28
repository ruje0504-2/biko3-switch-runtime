#ifndef BK_WORLD_GROUND_H
#define BK_WORLD_GROUND_H
/* Original 0x4b43be projected triangle height. Success reports hit/miss
 * separately; height stays unchanged on miss. The original uses X-aligned
 * edge-line extrapolation and height-ordered interpolation, not a plane
 * equation. Nonfinite/overflow and undefined degenerate interpolation fail
 * without changing either output. */
int bk_ground_triangle(int *hit, float *height, const float point[3],
                       const float triangle[3][3]);
#endif
