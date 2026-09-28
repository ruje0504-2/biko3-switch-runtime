#include "world/route_motion.h"
#include "world/placement.h"
#include <math.h>
#include <stdio.h>
static float distance_xz(const float a[3], const float b[3]) {
  double x = (double)a[0] - b[0], z = (double)a[2] - b[2];
  return (float)sqrt(x * x + z * z);
}
static int heading(float *out, const float a[3], const float b[3],
                   float fallback) {
  if (a[0] == b[0] && a[2] == b[2]) {
    *out = fallback;
    return 1;
  }
  return bk_route_heading(out, a[0], a[2], b[0], b[2]);
}
static float lerp(float target, float old, float weight) {
  float delta = (float)((double)target - old);
  delta = (float)((double)delta * weight);
  return (float)((double)old + delta);
}
/* 0x4adcd9 blends the four Y-rotation matrix entries before atan2. */
static float angle_lerp(float target, float old, float weight) {
  float t = (float)((double)target * 0.01745329238474369f);
  float o = (float)((double)old * 0.01745329238474369f);
  float s = lerp((float)sin(t), (float)sin(o), weight);
  float c = lerp((float)cos(t), (float)cos(o), weight);
  float angle =
      (float)(atan2((double)s, (double)c) * 180.0 / 3.141592025756836);
  if (angle < -180)
    angle = (float)((double)angle + 360);
  else if (angle > 180)
    angle = (float)((double)angle - 360);
  return angle;
}
int bk_route_motion_step(BkRouteMotion *state, const BkRoute *route,
                         float distance, char error[256]) {
  if (!state || !route || !isfinite(distance) || distance < 0 ||
      state->cursor > bk_route_count(route)) {
    snprintf(error, 256, "route motion: invalid state/distance/cursor");
    return 0;
  }
  if (!isfinite(state->yaw_degrees) || !isfinite(state->position[0]) ||
      !isfinite(state->position[1]) || !isfinite(state->position[2])) {
    snprintf(error, 256, "route motion: nonfinite placement");
    return 0;
  }
  if (!state->cursor)
    return 1;
  BkRouteMotion next = *state;
  next.crossed = 0;
  next.segment_start = next.cursor - 1;
  next.segment_end = next.cursor;
  const float *start = state->position;
  const BkRoutePoint *end = bk_route_point(route, next.segment_end);
  float remaining = distance, length = distance_xz(start, end->position);
  if (length < distance) {
    remaining = (float)((double)distance - length);
    next.segment_start++;
    next.segment_end++;
    for (;;) {
      next.crossed = 1;
      next.last_crossed = next.segment_start;
      const BkRoutePoint *point = bk_route_point(route, next.segment_start);
      end = bk_route_point(route, next.segment_end);
      if (!point || !end) {
        snprintf(error, 256,
                 "route motion: movement past decoded route sentinel");
        return 0;
      }
      start = point->position;
      length = distance_xz(start, end->position);
      if (point->flags == 6)
        next.run_remaining = 3;
      else if (point->flags == 0 && next.run_remaining > 0)
        next.run_remaining--;
      if (length > remaining || point->flags != 0)
        break;
      next.segment_start++;
      next.segment_end++;
      remaining = (float)((double)remaining - length);
    }
  }
  float direction;
  if (!heading(&direction, start, end->position, state->yaw_degrees))
    goto nonfinite;
  /* Movement trig consumes double radians; frame rotation rounds to float. */
  double radians = (double)direction * 0.01745329238474369f;
  float dx = (float)(sin(radians) * remaining);
  float dz = (float)(cos(radians) * remaining);
  next.position[0] = (float)((double)start[0] + dx);
  next.position[2] = (float)((double)start[2] + dz);
  const BkRoutePoint *segment = bk_route_point(route, next.segment_start);
  float target, old = state->yaw_degrees;
  if (!heading(&target, segment->position, end->position, state->yaw_degrees))
    goto nonfinite;
  if (next.segment_start >= 3) {
    const BkRoutePoint *previous =
        bk_route_point(route, next.segment_start - 1);
    if (!heading(&old, previous->position, segment->position,
                 state->yaw_degrees))
      goto nonfinite;
  }
  length = distance_xz(segment->position, end->position);
  float traveled = distance_xz(segment->position, next.position);
  float weight = length != 0 ? (float)((double)traveled / length) : 0;
  if (weight >= 1)
    weight = 1;
  next.cursor = next.segment_end;
  next.yaw_degrees = angle_lerp(target, old, weight);
  if (!isfinite(next.position[0]) || !isfinite(next.position[2]) ||
      !isfinite(next.yaw_degrees) || !isfinite(length) || !isfinite(traveled))
    goto nonfinite;
  *state = next;
  return 1;
nonfinite:
  snprintf(error, 256, "route motion: nonfinite geometry");
  return 0;
}
