#include "world/placement.h"
#include <math.h>
#include <string.h>
int bk_route_heading(float *degrees, float x, float z, float next_x,
                     float next_z) {
  if (!degrees || !isfinite(x) || !isfinite(z) || !isfinite(next_x) ||
      !isfinite(next_z))
    return 0;
  float dz = (float)((double)z - next_z), dx = (float)((double)x - next_x);
  double vx = (double)next_x - x, vz = (double)next_z - z;
  float distance = (float)sqrt(vx * vx + vz * vz);
  if (!isfinite(distance) || !isfinite(dx) || !isfinite(dz))
    return 0;
  float yaw = 0;
  if (distance != 0) {
    /* 0x53f598 is deliberately not the usual 180/pi. Do not replace with
     * atan2: the original rounds distance/deltas before the inverse trig. */
    const double degrees_per_radian = 57.29577791868205;
    float a = (float)(asin((double)dz / distance) * degrees_per_radian);
    float b = (float)(acos((double)dx / distance) * degrees_per_radian);
    if (!isfinite(a) || !isfinite(b))
      return 0;
    yaw = a <= 0 ? b : (float)(360.0 - b);
    yaw = (float)((double)yaw - 90.0);
    if (yaw < 0)
      yaw = (float)((double)yaw + 360.0);
  }
  *degrees = yaw;
  return 1;
}
int bk_actor_placement(BkActorPlacement *out, const float position[3],
                       float yaw_degrees) {
  if (!out || !position || !isfinite(yaw_degrees))
    return 0;
  for (unsigned i = 0; i < 3; i++)
    if (!isfinite(position[i]))
      return 0;
  float radians = (float)((double)yaw_degrees * 0.01745329238474369f);
  if (!isfinite(radians))
    return 0;
  float c = (float)cos((double)radians), s = (float)sin((double)radians);
  float t = (float)(1.0 - c);
  BkActorPlacement result = {.yaw_degrees = yaw_degrees};
  memcpy(result.position, position, sizeof(result.position));
  result.world[0] = result.world[10] = c;
  result.world[2] = -s;
  result.world[8] = s;
  /* Original rounds (1-c) before adding c on the unit rotation axis. */
  result.world[5] = (float)((double)t + c);
  result.world[15] = 1;
  memcpy(result.world + 12, position, sizeof(result.position));
  *out = result;
  return 1;
}
int bk_route_placement(BkActorPlacement *out, const BkRoute *route,
                       uint32_t index, float height) {
  if (!out || !isfinite(height) || index >= bk_route_count(route))
    return 0;
  const BkRoutePoint *point = bk_route_point(route, index);
  const BkRoutePoint *next = bk_route_point(route, index + 1);
  float heading;
  if (!bk_route_heading(&heading, point->position[0], point->position[2],
                        next->position[0], next->position[2]))
    return 0;
  float position[3] = {point->position[0], height, point->position[2]};
  return bk_actor_placement(out, position, heading);
}
