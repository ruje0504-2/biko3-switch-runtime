#include "game/player_trigger.h"
#include "game/player_trigger_tables.inc"
#include "world/placement.h"
#include "world/proximity.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static int finite3(const float p[3]) {
  return isfinite(p[0]) && isfinite(p[1]) && isfinite(p[2]);
}
static int endpoints(BkPlayerTrigger *out, const float position[3],
                     const float target[3]) {
  float forward, backward;
  if (!bk_route_heading(&forward, position[0], position[2], target[0],
                        target[2]) ||
      !bk_route_heading(&backward, target[0], target[2], position[0],
                        position[2]))
    return 0;
  memcpy(out->origin, position, 12);
  memcpy(out->target, target, 12);
  out->origin[3] = backward;
  out->target[3] = forward;
  return 1;
}
int bk_player_trigger_prop(BkPlayerTrigger *state, int *hit,
                           const BkPlayerTriggerInput *in, unsigned variant,
                           char error[256]) {
  if (!state || !hit || !in || variant > 1 || !finite3(in->position))
    goto invalid;
  for (unsigned i = 0; i < 16; ++i) {
    const BkPlayerTriggerProp *p = &in->props[i];
    if (!p->active ||
        (variant == 0 && p->kind != 10 && p->kind != 19 && p->kind != 18) ||
        (variant == 1 && p->kind != 11))
      continue;
    int close;
    if (!bk_proximity_xz(&close, p->position, in->position, 20))
      goto invalid;
    if (!close)
      continue;
    if (in->action != in->actions[variant ? 13 : 11]) {
      BkPlayerTrigger next = *state;
      if (!endpoints(&next, in->position, p->position))
        goto invalid;
      next.prop_kind = p->kind;
      *state = next;
    }
    *hit = 1;
    return 1;
  }
  *hit = 0;
  return 1;
invalid:
  if (error)
    snprintf(error, 256, "Invalid player prop interaction");
  return 0;
}
static int region(const BkPlayerTriggerInput *in) {
  return in && in->wall_name && in->group >= 0 && in->group < 5 &&
         in->area >= 0 && in->area < 9;
}
int bk_player_trigger_wall(BkPlayerTrigger *state, int *hit,
                           const BkPlayerTriggerInput *in, char error[256]) {
  if (!state || !hit || !region(in) || !finite3(in->position) ||
      !isfinite(in->wall_heading))
    goto invalid;
  int match = 0;
  if (!bk_player_trigger_wall_available(&match, in))
    goto invalid;
  if (match && in->action != in->actions[16] && in->action != in->actions[17]) {
    float yaw = (float)((double)in->wall_heading + 180);
    if (yaw >= 360)
      yaw = (float)((double)yaw - 360);
    double radians = (double)yaw * 0.01745329238474369f;
    float x = (float)(sin(radians) * 15), z = (float)(cos(radians) * 15);
    const float target[3] = {(float)((double)x + in->position[0]),
                             in->position[1],
                             (float)((double)z + in->position[2])};
    BkPlayerTrigger next = *state;
    if (!endpoints(&next, in->position, target))
      goto invalid;
    *state = next;
    *hit = 1;
  } else
    *hit = in->action == in->actions[16];
  return 1;
invalid:
  if (error)
    snprintf(error, 256, "Invalid player wall interaction");
  return 0;
}
int bk_player_trigger_cover(int *hit, const BkPlayerTriggerInput *in) {
  if (!hit || !region(in))
    return 0;
  *hit = !strcmp(in->wall_name, cover_names[in->group][in->area]);
  return 1;
}
int bk_player_trigger_wall_available(int *hit, const BkPlayerTriggerInput *in) {
  if (!hit || !region(in))
    return 0;
  static const unsigned scenes[5][4] = {
      {0, 1, 3, 2}, {2, 0, 1, 3}, {1, 0, 2, 3}, {3, 1, 0, 2}, {0, 2, 3, 1}};
  int match = 0;
  if (in->area < 4)
    for (unsigned i = 0; i < 8; ++i)
      if (!strcmp(in->wall_name, wall_names[scenes[in->group][in->area]][i]))
        match = 1;
  *hit = match;
  return 1;
}
