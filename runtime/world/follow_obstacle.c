#include "world/follow_obstacle.h"
#include "world/visibility.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
static int fail(char *error) {
  snprintf(error, 256, "follow obstacle: invalid input/geometry");
  return 0;
}
static int finite3(const float *p) {
  return p && isfinite(p[0]) && isfinite(p[1]) && isfinite(p[2]);
}
int bk_follow_obstacle_mesh(BkFollowObstacle *state,
                            const BkCollisionMesh *mesh, const float actor[3],
                            const float camera[3], int *hit, char error[256]) {
  if (!state || !mesh || !hit || !finite3(actor) || !finite3(camera) ||
      !isfinite(state->distance) || !finite3(state->point) ||
      mesh->index_count % 3 ||
      (mesh->index_count &&
       (!mesh->vertices || !mesh->indices || !mesh->normals)))
    return fail(error);
  BkFollowObstacle next = *state;
  int found = 0;
  BkSightSegment line = {0};
  memcpy(line.start, actor, 12);
  memcpy(line.end, camera, 12);
  float point[3] = {0};
  const float limit = (float)cos(1.047);
  for (uint32_t i = 0; i < mesh->index_count; i += 3) {
    if (!isfinite(mesh->normals[i / 3][1]))
      return fail(error);
    if (mesh->normals[i / 3][1] >= limit)
      continue;
    for (unsigned e = 0; e < 3; ++e) {
      uint32_t a = mesh->indices[i + e], b = mesh->indices[i + (e + 1) % 3];
      if (a >= mesh->vertex_count || b >= mesh->vertex_count ||
          !finite3(mesh->vertices[a]) || !finite3(mesh->vertices[b]))
        return fail(error);
      BkSightSegment edge = {0};
      memcpy(edge.start, mesh->vertices[a], 12);
      memcpy(edge.end, mesh->vertices[b], 12);
      int crossing;
      if (!bk_sight_crossing_point(&crossing, point, &edge, &line)) {
        ++next.singular_intersections;
        continue;
      }
      if (point[0] == 0 && point[2] == 0)
        continue;
      double dx = (double)actor[0] - point[0], dz = (double)actor[2] - point[2];
      volatile double xx = dx * dx, zz = dz * dz;
      float distance = (float)sqrt(xx + zz);
      if (!isfinite(distance))
        return fail(error);
      if (distance <= next.distance) {
        found = 1;
        next.distance = distance;
        next.point[0] = point[0];
        next.point[1] = camera[1];
        next.point[2] = point[2];
      }
    }
  }
  *state = next;
  *hit = found;
  return 1;
}
int bk_follow_obstacle_scene(BkFollowObstacle *state,
                             const BkCollision *collision, const float actor[3],
                             const float camera[3], int *hit, char error[256]) {
  if (!state || !collision || !hit || !finite3(actor) || !finite3(camera) ||
      !isfinite(state->distance) || !finite3(state->point))
    return fail(error);
  BkFollowObstacle next = *state;
  int found = 0;
  for (uint32_t i = 0; i < bk_collision_count(collision); ++i) {
    int h;
    if (!bk_follow_obstacle_mesh(&next, bk_collision_mesh(collision, i), actor,
                                 camera, &h, error))
      return 0;
    found |= h;
  }
  *state = next;
  *hit = found;
  return 1;
}
