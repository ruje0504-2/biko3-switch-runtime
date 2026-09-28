#include "world/player_wall.h"
#include "world/collision.h"
#include "world/placement.h"
#include "world/proximity.h"
#include "world/visibility.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static int finite3(const float p[3]) {
  return p && isfinite(p[0]) && isfinite(p[1]) && isfinite(p[2]);
}
static double distance(const float a[3], const float b[3]) {
  double x = (double)a[0] - b[0], z = (double)a[2] - b[2];
  return sqrt(x * x + z * z);
}
static int vertical(const float p[3], float height, const float t[3][3],
                    float offset) {
  float lo = fminf(t[0][1], fminf(t[1][1], t[2][1]));
  float hi = fmaxf(t[0][1], fmaxf(t[1][1], t[2][1]));
  return lo < height && (double)p[1] + offset < hi;
}
static int geometry(const float p[3], const float previous[3], float height,
                    const float t[3][3]) {
  return finite3(p) && finite3(previous) && isfinite(height) && t &&
         finite3(t[0]) && finite3(t[1]) && finite3(t[2]);
}
static BkSightSegment segment(const float a[3], const float b[3]) {
  BkSightSegment s = {0};
  memcpy(s.start, a, sizeof(s.start));
  memcpy(s.end, b, sizeof(s.end));
  return s;
}
static int increment(uint32_t *value) {
  if (*value == UINT32_MAX)
    return 0;
  ++*value;
  return 1;
}
static int move_from_endpoint(float p[3], const float previous[3],
                              const float projection[3], const float end[3]) {
  int old_side, end_side;
  if (!bk_sight_line_side(&old_side, p, projection, previous) ||
      !bk_sight_line_side(&end_side, p, projection, end))
    return 0;
  if (old_side != end_side)
    return 1;
  float yaw;
  if (!bk_route_heading(&yaw, p[0], p[2], end[0], end[2]))
    return 0;
  double d = distance(p, end), radians = (double)yaw * 0.01745329238474369f;
  float dx = (float)(sin(radians) * (4.1 - d));
  float dz = (float)(cos(radians) * (4.1 - (float)d));
  p[0] = (float)((double)p[0] - dx);
  p[2] = (float)((double)p[2] - dz);
  return finite3(p);
}
int bk_player_wall_validate(const BkPlayerWall *state,
                            const BkPlayerWallInput *in) {
  if (!state || !in || !finite3(state->position) || !finite3(in->previous) ||
      !isfinite(in->height) || !finite3(state->normal) ||
      !finite3(in->motion) || !finite3(in->camera) ||
      !isfinite(state->camera_distance) ||
      (state->near_wall != 0 && state->near_wall != 1))
    return 0;
  for (unsigned i = 0; i < 7; ++i)
    if (!finite3(in->rays[i]) ||
        (state->rays_blocked[i] != 0 && state->rays_blocked[i] != 1))
      return 0;
  return 1;
}
int bk_player_wall_triangle(BkPlayerWall *state, int *near,
                            const BkPlayerWallInput *in, const float t[3][3],
                            char error[256]) {
  if (!near || !bk_player_wall_validate(state, in) || !t || !finite3(t[0]) ||
      !finite3(t[1]) || !finite3(t[2]))
    goto invalid;
  if (!vertical(state->position, in->height, t, 10)) {
    *near = 0;
    return 1;
  }
  BkPlayerWall next = *state;
  float *p = next.position;
  float radius = (float)distance(p, in->previous);
  if (!isfinite(radius))
    goto invalid;
  if (radius <= 4)
    radius = 4;
  BkSightSegment edges[3], camera = segment(p, in->camera), rays[7];
  for (unsigned i = 0; i < 3; ++i)
    edges[i] = segment(t[i], t[(i + 1) % 3]);
  for (unsigned i = 0; i < 7; ++i)
    rays[i] = segment(p, in->rays[i]);
  float intersection[3] = {0};
  for (unsigned i = 0; i < 3; ++i) {
    int hit;
    if (!bk_sight_crossing(&hit, &edges[i], &camera))
      goto invalid;
    if (hit &&
        !bk_sight_crossing_point(&hit, intersection, &edges[i], &camera)) {
      if (!increment(&next.singular_camera))
        goto invalid;
      continue;
    }
    /* Native retains the intersection through a miss; (0,0,0) is its sentinel.
     */
    if (intersection[0] || intersection[1] || intersection[2]) {
      float d = (float)distance(p, intersection);
      if (!isfinite(d))
        goto invalid;
      if (d <= next.camera_distance)
        next.camera_distance = d;
    }
  }
  for (unsigned i = 0; i < 3; ++i)
    for (unsigned j = 0; j < 7; ++j) {
      int hit;
      if (!bk_sight_crossing(&hit, &edges[i], &rays[j]))
        goto invalid;
      if (hit)
        next.rays_blocked[j] = 1;
    }
  float projection[3] = {0};
  int projection_valid = 0, result = 0;
  for (unsigned i = 0; i < 3; ++i) {
    const float *a = edges[i].start, *b = edges[i].end;
    int interior, end_a, end_b;
    float expanded = (float)((double)radius + 1);
    if (!bk_proximity_interior_xz(&interior, a, b, p, expanded) ||
        !bk_proximity_xz(&end_a, a, p, expanded) ||
        !bk_proximity_xz(&end_b, b, p, expanded))
      goto invalid;
    if (interior || end_a || end_b)
      result = 1;
    if (!bk_proximity_project_xz(&interior, projection, a, b, p, 6))
      goto invalid;
    if (interior)
      next.near_wall = 1;
    if (!bk_proximity_project_xz(&interior, projection, a, b, p, radius))
      goto invalid;
    if (a[0] != b[0] || a[2] != b[2])
      projection_valid = 1;
    if (interior) {
      float yaw;
      int old_side, current_side;
      if (!bk_route_heading(&yaw, p[0], p[2], projection[0], projection[2]) ||
          !bk_collision_normal(next.normal, t) ||
          !bk_sight_line_side(&old_side, a, b, in->previous))
        goto invalid;
      next.near_wall = 1;
      if (in->motion[0] == 0 && in->motion[1] == 0 && in->motion[2] == 0)
        continue;
      if (!bk_sight_line_side(&current_side, a, b, p))
        goto invalid;
      double radians = (double)yaw * 0.01745329238474369f;
      if (old_side == current_side) {
        float dx = (float)(sin(radians) * 4.1);
        float dz = (float)(cos(radians) * 4.1);
        dx = (float)((double)p[0] + dx);
        dz = (float)((double)p[2] + dz);
        dx = (float)((double)dx - projection[0]);
        dz = (float)((double)dz - projection[2]);
        p[0] = (float)((double)p[0] - dx);
        p[2] = (float)((double)p[2] - dz);
      } else {
        double d = distance(p, projection);
        float dx = (float)(sin(radians) * (d + 4.1));
        float dz = (float)(cos(radians) * ((float)d + 4.1));
        p[0] = (float)((double)projection[0] + dx);
        p[2] = (float)((double)projection[2] + dz);
      }
    } else {
      if (!bk_proximity_xz(&end_a, a, p, radius) ||
          !bk_proximity_xz(&end_b, b, p, radius))
        goto invalid;
      if (end_a || end_b) {
        if (!projection_valid) {
          if (!increment(&next.missing_projection))
            goto invalid;
        } else if (!move_from_endpoint(p, in->previous, projection,
                                       end_a ? a : b))
          goto invalid;
      }
    }
    if (!finite3(p))
      goto invalid;
  }
  *state = next;
  *near = result;
  return 1;
invalid:
  if (error)
    snprintf(error, 256, "Invalid/overflowing player wall triangle");
  return 0;
}
int bk_player_wall_restore(float position[3], const float previous[3],
                           float height, const float t[3][3], char error[256]) {
  if (!geometry(position, previous, height, t))
    goto invalid;
  if (!vertical(position, height, t, 15))
    return 1;
  float p[3];
  memcpy(p, position, sizeof(p));
  float radius = (float)distance(p, previous);
  if (!isfinite(radius))
    goto invalid;
  if (radius <= 4)
    radius = 4;
  for (unsigned i = 0; i < 3; ++i) {
    int hit;
    if (!bk_proximity_interior_xz(&hit, t[i], t[(i + 1) % 3], p, radius))
      goto invalid;
    if (hit) {
      p[0] = previous[0];
      p[2] = previous[2];
    }
  }
  memcpy(position, p, sizeof(p));
  return 1;
invalid:
  if (error)
    snprintf(error, 256, "Invalid/overflowing player wall restore");
  return 0;
}
