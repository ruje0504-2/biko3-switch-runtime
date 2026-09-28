#include "world/prop_route.h"
#include "core/camera.h"
#include "world/placement.h"
#include <math.h>
#include <stdio.h>
static float distance_xz(const float a[3], const float b[3]) {
  double x = (double)a[0] - b[0], z = (double)a[2] - b[2];
  return (float)sqrt(x * x + z * z);
}
static int heading(float *out, const float a[3], const float b[3], float old) {
  if (a[0] == b[0] && a[2] == b[2]) {
    *out = old;
    return 1;
  }
  return bk_route_heading(out, a[0], a[2], b[0], b[2]);
}
int bk_prop_route_step(BkPropRoute *state, const BkRoute *route, float distance,
                       const BkCollision *collision, int32_t kind,
                       BkPropRouteEffects *effects, char error[256]) {
  if (!state || !route || !effects || !isfinite(distance) || distance < 0 ||
      state->first < 0 || state->last < state->first ||
      state->last >= (int32_t)bk_route_count(route) || state->last >= 64 ||
      state->cursor < 0 || state->cursor > state->last)
    goto invalid;
  for (unsigned i = 0; i < 3; i++)
    if (!isfinite(state->position[i]) || !isfinite(state->velocity[i]))
      goto invalid;
  if (!isfinite(state->yaw))
    goto invalid;
  BkPropRoute next = *state;
  BkPropRouteEffects out = {0};
  int32_t earlier = state->cursor - 2, previous = state->cursor - 1;
  int32_t current = state->cursor;
  if (earlier < 0) {
    earlier = state->last;
    if (previous < 0) {
      earlier--;
      previous = state->last;
    }
  }
  /* Native indexes -1 for a one-point route at cursor0. */
  if (earlier < 0)
    goto invalid;
  float remaining = distance;
  const float *start = state->position;
  const BkRoutePoint *end = bk_route_point(route, current);
  float length = distance_xz(start, end->position);
  float proposed[3] = {state->position[0], state->position[1],
                       state->position[2]};
  float proposed_yaw = state->yaw;
  int snap = 0;
  if (length < distance) {
    remaining = (float)((double)distance - length);
    if (++previous > state->last)
      previous = state->first;
    if (++current > state->last)
      current = state->first;
    unsigned stalled = 0;
    for (;;) {
      out.crossed = 1;
      const BkRoutePoint *point = bk_route_point(route, previous);
      end = bk_route_point(route, current);
      start = point->position;
      length = distance_xz(start, end->position);
      if (length > remaining)
        break;
      if (point->flags) {
        proposed[0] = point->position[0];
        proposed[2] = point->position[2];
        proposed_yaw = point->parameter;
        snap = 1;
        break;
      }
      if (++previous > state->last)
        previous = state->first;
      if (++current > state->last)
        current = state->first;
      float reduced = (float)((double)remaining - length);
      stalled = reduced == remaining ? stalled + 1 : 0;
      if (stalled > (unsigned)(state->last - state->first + 1))
        goto invalid;
      remaining = reduced;
    }
  }
  float direction;
  if (!heading(&direction, start, end->position, state->yaw))
    goto invalid;
  if (!snap) {
    double radians = (double)direction * 0.01745329238474369f;
    next.velocity[0] = (float)(remaining * sin(radians));
    next.velocity[2] = (float)(remaining * cos(radians));
    proposed[0] = (float)((double)start[0] + next.velocity[0]);
    proposed[2] = (float)((double)start[2] + next.velocity[2]);
    proposed_yaw = direction;
  }
  next.cursor = current;
  const BkRoutePoint *point = bk_route_point(route, previous);
  end = bk_route_point(route, current);
  const BkRoutePoint *old = bk_route_point(route, earlier);
  float target_yaw, old_yaw;
  if (!heading(&target_yaw, point->position, end->position, proposed_yaw) ||
      !heading(&old_yaw, old->position, point->position, proposed_yaw))
    goto invalid;
  length = distance_xz(point->position, end->position);
  float traveled = distance_xz(point->position, state->position);
  float weight = length != 0 ? (float)((double)traveled / length) : 0;
  if (weight >= 1)
    weight = 1;
  float yaw;
  if (!bk_angle_blend_degrees(&yaw, target_yaw, old_yaw, weight))
    goto invalid;
  float look[3] = {(float)((double)next.velocity[0] * 8 + state->position[0]),
                   state->position[1],
                   (float)((double)next.velocity[2] * 8 + state->position[2])};
  if (!isfinite(proposed[0]) || !isfinite(proposed[2]) || !isfinite(look[0]) ||
      !isfinite(look[2]))
    goto invalid;
  if (collision && !bk_collision_prop_blocked(collision, kind, state->position,
                                              look, &out.blocked, error))
    return 0;
  if (out.blocked)
    next.velocity[0] = next.velocity[2] = 0;
  else {
    next.position[0] = proposed[0];
    next.position[2] = proposed[2];
    next.yaw = yaw;
  }
  if (out.crossed)
    out.last_crossed = previous;
  *state = next;
  *effects = out;
  return 1;
invalid:
  snprintf(error, 256,
           "prop route: invalid state/geometry or nonterminating route");
  return 0;
}
