#include "game/npc_head.h"
#include "world/placement.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
static float wrap(float degrees) {
  if (degrees < -180)
    degrees = (float)((double)degrees + 360);
  else if (degrees > 180)
    degrees = (float)((double)degrees - 360);
  return degrees;
}
static int roll(float *out, const float m[16]) {
  if (!isfinite(m[1]) || !isfinite(m[5]) || m[1] < -1 || m[1] > 1)
    return 0;
  double degrees = asin((double)m[1]) * 180.0 / 3.141592025756836;
  if (m[5] < 0)
    degrees = 180.0 - degrees;
  *out = wrap((float)degrees);
  return isfinite(*out);
}
int bk_npc_head_update(BkNpcHeadState *state, const BkNpcHeadInput *input,
                       char error[256]) {
  if (!state || !input || !isfinite(input->actor_yaw))
    goto invalid;
  for (unsigned i = 0; i < 16; i++)
    if (!isfinite(input->actor_head_world[i]))
      goto invalid;
  for (unsigned i = 0; i < 3; i++)
    if (!isfinite(input->player_head[i]) ||
        !isfinite(input->player_position[i]))
      goto invalid;
  BkNpcHeadState next = {0};
  memcpy(next.sight.start, input->actor_head_world + 12,
         sizeof(next.sight.start));
  memcpy(next.sight.end, input->player_head, sizeof(next.sight.end));
  double d[3];
  for (unsigned i = 0; i < 3; i++)
    d[i] = (double)next.sight.end[i] - next.sight.start[i];
  next.sight.distance = (float)sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
  if (!isfinite(next.sight.distance) ||
      !bk_route_heading(&next.bearing, next.sight.start[0], next.sight.start[2],
                        input->player_position[0], input->player_position[2]))
    goto invalid;
  if (input->actor_kind == 1 || input->actor_kind == 2) {
    float head, parent;
    if (!roll(&head, input->actor_head_local) ||
        !roll(&parent, input->torso_local))
      goto invalid;
    next.facing = (float)((double)head + parent + input->actor_yaw);
    if (next.facing >= 360)
      next.facing = (float)((double)next.facing - 360);
    else if (next.facing < 0)
      next.facing = (float)((double)next.facing + 360);
  } else {
    next.facing = wrap((float)(atan2((double)input->actor_head_world[8],
                                     input->actor_head_world[10]) *
                               180.0 / 3.141592025756836));
    if (next.facing <= 0)
      next.facing = (float)(180.0 + next.facing + 180.0);
  }
  if (!isfinite(next.facing))
    goto invalid;
  *state = next;
  return 1;
invalid:
  snprintf(error, 256, "NPC head: invalid cached head/angle input");
  return 0;
}
