#include "scene/ending_ui_pick.h"
#include "core/matrix.h"
#include <math.h>
#include <stdio.h>

static int fail(char e[256], const char *why) {
  if (e)
    snprintf(e, 256, "ending UI pick: %s", why);
  return 0;
}
static int finite_matrix(const float *m) {
  if (!m)
    return 0;
  for (unsigned i = 0; i < 16; ++i)
    if (!isfinite(m[i]))
      return 0;
  return 1;
}
int bk_ending_ui_camera_sector(const float local[16], int32_t cached,
                               int32_t low, int32_t high, int *outside,
                               int *category) {
  if (!local || !outside || !category || !isfinite(local[8]) ||
      !isfinite(local[10]))
    return 0;
  /*42ee68 stores degrees after using its authored float pi; the atan2
   * result is NOT first rounded to float by its non-popping fst. */
  float yaw =
      (float)(atan2((double)local[8], local[10]) * 180.0 / 3.141592025756836);
  if (yaw < -180)
    yaw = (float)((double)yaw + 360);
  else if (yaw > 180)
    yaw = (float)((double)yaw - 360);
  if (yaw < 0)
    yaw = (float)(360.0 - fabs((double)yaw));
  int out = !((double)high > yaw && (double)low < yaw);
  int special = cached == 26 || cached == 27;
  *outside = out;
  *category = out ? (special ? 1 : 2) : (special ? 3 : 4);
  return 1;
}
static float length2(float x, float y) {
  float squared = (float)((double)x * x + (double)y * y);
  return (float)sqrt((double)squared);
}
int bk_ending_ui_segment_hit(const float a[2], const float b[2],
                             const float p[2], float radius, int *hit) {
  if (!a || !b || !p || !hit || isnan(radius) || radius < 0)
    return 0;
  float ab[2], ap[2];
  for (unsigned i = 0; i < 2; i++) {
    if (!isfinite(a[i]) || !isfinite(b[i]) || !isfinite(p[i]))
      return 0;
    ab[i] = (float)((double)b[i] - a[i]);
    ap[i] = (float)((double)p[i] - a[i]);
  }
  float length = length2(ab[0], ab[1]);
  if (!isfinite(length))
    return 0;
  if (length == 0) {
    *hit = 0;
    return 1;
  }
  float t = (float)(((double)ap[0] * ab[0] + (double)ap[1] * ab[1]) /
                    ((double)length * length));
  if (!isfinite(t))
    return 0;
  if (t < 0 || t > 1) {
    *hit = 0;
    return 1;
  }
  float delta[2];
  for (unsigned i = 0; i < 2; i++) {
    float offset = (float)((double)t * ab[i]);
    float point = (float)((double)a[i] + offset);
    delta[i] = (float)((double)p[i] - point);
  }
  float distance = length2(delta[0], delta[1]);
  if (!isfinite(distance))
    return 0;
  *hit = distance < radius;
  return 1;
}
static int project(float xy[2], const float world[16], const float screen[16]) {
  float m[16];
  bk_matrix_multiply(m, world, screen);
  if (!isfinite(m[15]))
    return 0;
  if (m[15] == 0) {
    xy[0] = xy[1] = -10000;
    return 1;
  }
  for (unsigned i = 0; i < 2; i++) {
    double value = (double)m[12 + i] / m[15];
    if (!isfinite(value) || value <= -2147483649.0 || value >= 2147483648.0)
      return 0;
    xy[i] = (float)(int32_t)value;
  }
  return 1;
}
int bk_ending_ui_pick_targets(const BkEndingUiPickBindings *b,
                              const float pointer[2], float *distance,
                              int32_t *selected, char e[256]) {
  static const unsigned nodes[24] = {24, 20, 25, 21, 20, 18, 21, 19,
                                     33, 35, 34, 36, 35, 31, 36, 32,
                                     31, 37, 32, 38, 37, 29, 38, 30};
  if (!b || !b->world || !b->present || !pointer || !distance || !selected ||
      !isfinite(*distance) || !isfinite(pointer[0]) || !isfinite(pointer[1]) ||
      !b->camera_position || !isfinite(b->camera_position[0]) ||
      !isfinite(b->camera_position[1]) || !isfinite(b->camera_position[2]) ||
      !isfinite(b->ring_width) || b->ring_width < 0 ||
      !finite_matrix(b->view) || !finite_matrix(b->projection) ||
      !finite_matrix(b->viewport_matrix))
    return fail(e, "invalid query input");
  for (unsigned i = 0; i < 24; i++)
    if (nodes[i] >= b->count || !b->present[nodes[i]] ||
        !finite_matrix(b->world[nodes[i]]))
      return fail(e, "missing/invalid required cached node");
  float vp[16], screen[16], points[24][2], depths[24], radii[24];
  bk_matrix_multiply(vp, b->view, b->projection);
  bk_matrix_multiply(screen, vp, b->viewport_matrix);
  for (unsigned i = 0; i < 24; i++) {
    const float *world = b->world[nodes[i]];
    float delta[3];
    for (unsigned j = 0; j < 3; j++)
      delta[j] = (float)((double)b->camera_position[j] - world[12 + j]);
    float square =
        (float)((double)delta[0] * delta[0] + (double)delta[1] * delta[1] +
                (double)delta[2] * delta[2]);
    depths[i] = (float)sqrt((double)square);
    if (!isfinite(depths[i]) || !project(points[i], world, screen))
      return fail(e, "invalid target distance/projection");
    radii[i] = depths[i] == 0
                   ? INFINITY
                   : (float)((i == 0 || i == 2 ? 60.0 : 40.0) / depths[i]);
  }
  float nearest = *distance;
  int32_t result = -1;
  for (unsigned i = 0; i < 12; i++) {
    unsigned a = i * 2, z = a + 1;
    float radius = (float)(((double)b->ring_width / 2) *
                           (((double)radii[a] + radii[z]) / 2));
    int hit;
    if (!bk_ending_ui_segment_hit(points[a], points[z], pointer, radius, &hit))
      return fail(e, "invalid segment geometry");
    double average = ((double)depths[a] + depths[z]) / 2;
    if (hit && average < nearest) {
      nearest = (float)average;
      result = (int32_t)i;
    }
  }
  *distance = nearest;
  *selected = result;
  return 1;
}
