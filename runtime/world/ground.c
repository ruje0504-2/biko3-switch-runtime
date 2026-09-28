#include "world/ground.h"
#include <math.h>
static int finite3(const float p[3]) {
  return p && isfinite(p[0]) && isfinite(p[1]) && isfinite(p[2]);
}
static double distance(const float a[3], const float b[3]) {
  double x = (double)a[0] - b[0], z = (double)a[2] - b[2];
  return sqrt(x * x + z * z);
}
/* 0x4ae1d7. Its axis-special equality differs from the sloping edge case. */
static int side(int *out, const float a[3], const float b[3],
                const float point[3]) {
  float dx = (float)((double)a[0] - b[0]);
  float dz = (float)((double)a[2] - b[2]);
  if (!isfinite(dx) || !isfinite(dz))
    return 0;
  if (!dx || !dz) {
    *out = dx == 0 && dz == 0 ? 0
           : dx == 0          ? a[0] <= point[0]
                              : a[2] <= point[2];
    return 1;
  }
  float slope = (float)((double)dz / dx);
  float intercept = (float)((double)a[2] - (double)a[0] * slope);
  if (!isfinite(slope) || !isfinite(intercept))
    return 0;
  *out = (double)point[0] * slope + intercept < point[2];
  return 1;
}
/* 0x4ae4d7 extends the line to point.x, without segment clamping. */
static int slice(float out[3], const float a[3], const float b[3], float x) {
  float dx = (float)((double)a[0] - b[0]);
  float dz = (float)((double)a[2] - b[2]);
  if (!isfinite(dx) || !isfinite(dz) || !dx)
    return 0;
  double slope = (double)dz / dx;
  float rounded = (float)slope;
  float intercept = (float)((double)a[2] - slope * a[0]);
  out[0] = x;
  out[1] = 0;
  out[2] = (float)((double)rounded * x + intercept);
  return isfinite(rounded) && isfinite(intercept) && finite3(out);
}
int bk_ground_triangle(int *hit, float *height, const float point[3],
                       const float triangle[3][3]) {
  if (!hit || !height || !isfinite(*height) || !finite3(point) || !triangle)
    return 0;
  for (unsigned i = 0; i < 3; i++)
    if (!finite3(triangle[i]))
      return 0;
  for (unsigned i = 0; i < 3; i++) {
    int reference, query;
    if (!side(&reference, triangle[i], triangle[(i + 1) % 3],
              triangle[(i + 2) % 3]) ||
        !side(&query, triangle[i], triangle[(i + 1) % 3], point))
      return 0;
    if (reference != query) {
      *hit = 0;
      return 1;
    }
  }
  float length[3];
  for (unsigned i = 0; i < 3; i++) {
    length[i] = (float)distance(triangle[i], triangle[(i + 1) % 3]);
    if (!isfinite(length[i]))
      return 0;
    if (!length[i]) {
      *hit = 0;
      return 1;
    }
  }
  if (triangle[0][1] == triangle[1][1] && triangle[1][1] == triangle[2][1]) {
    *height = triangle[0][1];
    *hit = 1;
    return 1;
  }
  float points[3][3];
  unsigned count = 0;
  for (unsigned i = 0; i < 3; i++) {
    const float *a = triangle[i], *b = triangle[(i + 1) % 3];
    if (a[0] == b[0])
      continue;
    float *p = points[count];
    if (!slice(p, a, b, point[0]))
      return 0;
    const float *hi = a[1] >= b[1] ? a : b;
    const float *lo = a[1] >= b[1] ? b : a;
    float along = (float)distance(lo, p);
    float ratio = (float)((double)along / length[i]);
    float delta = (float)(((double)hi[1] - lo[1]) * ratio);
    p[1] = (float)((double)delta + lo[1]);
    if (!isfinite(along) || !isfinite(ratio) || !isfinite(delta) || !finite3(p))
      return 0;
    count++;
  }
  /* Native otherwise reads uninitialized local points. Scene data should
   * reject this geometry; never publish a guessed height. */
  if (count < 2)
    return 0;
  unsigned hi = 0, lo = 1;
  if (count == 2) {
    if (points[0][1] < points[1][1]) {
      hi = 1;
      lo = 0;
    }
  } else if (points[0][1] >= points[1][1] && points[0][1] >= points[2][1]) {
    hi = 0;
    lo = points[1][1] >= points[2][1] ? 2 : 1;
  } else if (points[1][1] >= points[0][1] && points[1][1] >= points[2][1]) {
    hi = 1;
    lo = points[0][1] >= points[2][1] ? 2 : 0;
  } else {
    hi = 2;
    lo = points[0][1] >= points[1][1] ? 1 : 0;
  }
  float span = (float)distance(points[hi], points[lo]);
  if (!isfinite(span) || !span)
    return 0;
  float ratio = (float)(distance(points[lo], point) / span);
  double delta = ((double)points[hi][1] - points[lo][1]) * ratio;
  float result = (float)(delta + points[lo][1]);
  if (!isfinite(ratio) || !isfinite(delta) || !isfinite(result))
    return 0;
  *height = result;
  *hit = 1;
  return 1;
}
