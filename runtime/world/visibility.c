#include "world/visibility.h"
#include "world/placement.h"
#include "world/proximity.h"
#include <math.h>
#include <string.h>
static int finite3(const float a[3]) {
  return a && isfinite(a[0]) && isfinite(a[1]) && isfinite(a[2]);
}
int bk_sight_crossing(int *hit, const BkSightSegment *a,
                      const BkSightSegment *b) {
  if (!hit || !a || !b || !finite3(a->start) || !finite3(a->end) ||
      !finite3(b->start) || !finite3(b->end))
    return 0;
  return bk_segments_intersect_xz(hit, a->start, a->end, b->start, b->end);
}
int bk_sight_crossing_point(int *hit, float point[3], const BkSightSegment *a,
                            const BkSightSegment *b) {
  int cross;
  if (!hit || !point || !bk_sight_crossing(&cross, a, b))
    return 0;
  if (!cross) {
    *hit = 0;
    return 1;
  }
  unsigned independent = 0, dependent = 2;
  if (a->start[2] != a->end[2] && b->start[2] != b->end[2] &&
      (a->start[0] == a->end[0] || b->start[0] == b->end[0])) {
    independent = 2;
    dependent = 0;
  }
  const BkSightSegment *lines[] = {a, b};
  float slope[2], intercept[2];
  for (unsigned i = 0; i < 2; i++) {
    const float *s = lines[i]->start, *e = lines[i]->end;
    double denominator = (double)e[independent] - s[independent];
    if (!denominator)
      return 0;
    slope[i] = (float)(((double)e[dependent] - s[dependent]) / denominator);
    intercept[i] = (float)(((double)e[independent] * s[dependent] -
                            (double)s[independent] * e[dependent]) /
                           denominator);
    if (!isfinite(slope[i]) || !isfinite(intercept[i]))
      return 0;
  }
  double denominator = (double)slope[1] - slope[0];
  if (!denominator)
    return 0;
  float p[3] = {0};
  p[independent] = (float)(((double)intercept[0] - intercept[1]) / denominator);
  p[dependent] = (float)(((double)slope[1] * intercept[0] -
                          (double)slope[0] * intercept[1]) /
                         denominator);
  if (!finite3(p))
    return 0;
  memcpy(point, p, sizeof(p));
  *hit = 1;
  return 1;
}
int bk_sight_line_side(int *side, const float a[3], const float b[3],
                       const float point[3]) {
  if (!side || !finite3(a) || !finite3(b) || !finite3(point))
    return 0;
  float dx = (float)((double)a[0] - b[0]), dz = (float)((double)a[2] - b[2]);
  if (!isfinite(dx) || !isfinite(dz))
    return 0;
  int result = 0;
  if (!dx && !dz)
    result = 0;
  else if (!dx)
    result = point[0] >= a[0];
  else if (!dz)
    result = point[2] >= a[2];
  else {
    float slope = (float)((double)dz / dx);
    float intercept = (float)((double)a[2] - (double)a[0] * slope);
    double projected = (double)point[0] * slope + intercept;
    if (!isfinite(slope) || !isfinite(intercept) || !isfinite(projected))
      return 0;
    result = projected < point[2];
  }
  *side = result;
  return 1;
}
int bk_sight_triangle(int *blocked, const float triangle[3][3],
                      const BkSightSegment *sight) {
  if (!blocked || !triangle || !sight || !finite3(sight->start) ||
      !finite3(sight->end))
    return 0;
  for (unsigned i = 0; i < 3; i++)
    if (!finite3(triangle[i]))
      return 0;
  float hi = fmaxf(sight->start[1], sight->end[1]);
  float lo = fminf(sight->start[1], sight->end[1]);
  int result = 0;
  for (unsigned i = 0; i < 3; i++) {
    const float *a = triangle[i], *b = triangle[(i + 1) % 3];
    float edge_hi = fmaxf(a[1], b[1]), edge_lo = fminf(a[1], b[1]);
    if (hi < edge_lo || lo > edge_hi)
      continue;
    BkSightSegment edge = {0};
    memcpy(edge.start, a, sizeof(edge.start));
    memcpy(edge.end, b, sizeof(edge.end));
    int hit;
    if (!bk_sight_crossing(&hit, sight, &edge))
      return 0;
    if (hit) {
      result = 1;
      break;
    }
  }
  *blocked = result;
  return 1;
}
int bk_sight_cone(int *visible, const BkSightInput *input) {
  if (!visible || !input || !finite3(input->actor_position) ||
      !finite3(input->player_position) || !isfinite(input->head_distance) ||
      input->head_distance < 0 || !isfinite(input->facing))
    return 0;
  float heading;
  if (!bk_route_heading(&heading, input->actor_position[0],
                        input->actor_position[2], input->player_position[0],
                        input->player_position[2]))
    return 0;
  float difference = (float)((double)heading - input->facing);
  if (!isfinite(difference))
    return 0;
  float half_angle = input->actor_kind == 2 || input->actor_kind == 4 ? 90 : 60;
  float range = input->player_action == input->short_range_action ? 40 : 100;
  *visible = (double)input->player_position[1] <=
                 (double)input->actor_position[1] + 20 &&
             (double)input->player_position[1] >=
                 (double)input->actor_position[1] - 20 &&
             input->head_distance <= range && difference <= half_angle &&
             difference >= -half_angle;
  return 1;
}
