#ifndef BK_WORLD_PROXIMITY_H
#define BK_WORLD_PROXIMITY_H
/* Native horizontal distance predicates. Height is ignored geometrically;
 * callers provide their own vertical gate. Finite vectors and a nonnegative
 * finite radius are required. Invalid/overflowing geometry preserves hit. */
int bk_proximity_xz(int *hit, const float a[3], const float b[3], float radius);
/* 0x4ae783: zero-length XZ segment is always false, even at its endpoint.
 * Endpoint distance tests precede the rounded-float projection test. */
int bk_proximity_segment_xz(int *hit, const float start[3], const float end[3],
                            const float point[3], float radius);
/* 4ae68d: projected point must lie within segment; no endpoint-radius shortcut.
 * 4b6bb4 additionally writes the UNCLAMPED projection even on miss, preserving
 * point.Y. Zero-length segment misses and leaves projection unchanged. */
int bk_proximity_project_xz(int *hit, float projection[3], const float start[3],
                            const float end[3], const float point[3],
                            float radius);
int bk_proximity_interior_xz(int *hit, const float start[3], const float end[3],
                             const float point[3], float radius);
/* 4ae8ff: inclusive XZ segment intersection. Parallel/collinear segments
 * miss; Y and the native cached segment length are not consulted. */
int bk_segments_intersect_xz(int *hit, const float a[3], const float b[3],
                             const float c[3], const float d[3]);
#endif
