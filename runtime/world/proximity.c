#include "world/proximity.h"
#include <math.h>
static float determinant(double ax, double az, double bx, double bz) {
  /* Keep independent native products: fused subtraction leaves a nonzero
   * rounding residue for two identical/parallel segments on ARM64. */
  volatile double p = ax * bz, q = az * bx;
  return (float)(p - q);
}
int bk_segments_intersect_xz(int *hit, const float a[3], const float b[3],
                             const float c[3], const float d[3]) {
  if (!hit || !a || !b || !c || !d)
    return 0;
  for (unsigned i = 0; i < 3; i += 2)
    if (!isfinite(a[i]) || !isfinite(b[i]) || !isfinite(c[i]) ||
        !isfinite(d[i]))
      return 0;
  double ax = (double)b[0] - a[0], az = (double)b[2] - a[2];
  double bx = (double)d[0] - c[0], bz = (double)d[2] - c[2];
  double x = (double)a[0] - c[0], z = (double)a[2] - c[2];
  float denominator = determinant(ax, az, bx, bz);
  float p = determinant(z, x, bz, bx), q = determinant(z, x, az, ax);
  if (!isfinite(denominator) || !isfinite(p) || !isfinite(q))
    return 0;
  *hit = denominator > 0
             ? p >= 0 && p <= denominator && q >= 0 && q <= denominator
             : denominator < 0 && p >= denominator && p <= 0 &&
                   q >= denominator && q <= 0;
  return 1;
}
static int valid(const float v[3]) {
  return v && isfinite(v[0]) && isfinite(v[1]) && isfinite(v[2]);
}
static double distance(float ax, float az, float bx, float bz) {
  double x = (double)ax - bx, z = (double)az - bz;
  return sqrt(x * x + z * z);
}
int bk_proximity_xz(int *hit, const float a[3], const float b[3],
                    float radius) {
  if (!hit || !valid(a) || !valid(b) || !isfinite(radius) || radius < 0)
    return 0;
  *hit = (double)radius >= distance(a[0], a[2], b[0], b[2]);
  return 1;
}
int bk_proximity_segment_xz(int *hit, const float start[3], const float end[3],
                            const float point[3], float radius) {
  if (!hit || !valid(start) || !valid(end) || !valid(point) ||
      !isfinite(radius) || radius < 0)
    return 0;
  if (start[0] == end[0] && start[2] == end[2]) {
    *hit = 0;
    return 1;
  }
  if ((double)radius >= distance(start[0], start[2], point[0], point[2]) ||
      (double)radius >= distance(end[0], end[2], point[0], point[2])) {
    *hit = 1;
    return 1;
  }
  double dx = (double)end[0] - start[0], dz = (double)end[2] - start[2];
  float dot = (float)(((double)point[0] - start[0]) * dx +
                      ((double)point[2] - start[2]) * dz);
  float length_squared = (float)(dx * dx + dz * dz);
  if (!isfinite(dot) || !isfinite(length_squared) || !length_squared)
    return 0;
  float t = (float)((double)dot / length_squared);
  float x = (float)(dx * t + start[0]), z = (float)(dz * t + start[2]);
  if (!isfinite(t) || !isfinite(x) || !isfinite(z))
    return 0;
  *hit =
      t >= 0 && t <= 1 && (double)radius >= distance(x, z, point[0], point[2]);
  return 1;
}
int bk_proximity_project_xz(int *hit, float projection[3], const float start[3],
                            const float end[3], const float point[3],
                            float radius) {
  if (!hit || !projection || !valid(start) || !valid(end) || !valid(point) ||
      !isfinite(radius) || radius < 0)
    return 0;
  if (start[0] == end[0] && start[2] == end[2]) {
    *hit = 0;
    return 1;
  }
  double dx = (double)end[0] - start[0], dz = (double)end[2] - start[2];
  float dot = (float)(((double)point[0] - start[0]) * dx +
                      ((double)point[2] - start[2]) * dz);
  float length_squared = (float)(dx * dx + dz * dz);
  if (!isfinite(dot) || !isfinite(length_squared) || !length_squared)
    return 0;
  float t = (float)((double)dot / length_squared);
  float x = (float)(dx * t + start[0]), z = (float)(dz * t + start[2]);
  if (!isfinite(t) || !isfinite(x) || !isfinite(z))
    return 0;
  int result =
      t >= 0 && t <= 1 && (double)radius >= distance(x, z, point[0], point[2]);
  float y = point[1];
  projection[0] = x;
  projection[1] = y;
  projection[2] = z;
  *hit = result;
  return 1;
}
int bk_proximity_interior_xz(int *hit, const float start[3], const float end[3],
                             const float point[3], float radius) {
  float projection[3];
  return bk_proximity_project_xz(hit, projection, start, end, point, radius);
}
