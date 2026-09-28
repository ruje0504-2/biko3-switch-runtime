#include "game/npc_route.h"
#include <stdio.h>
int bk_npc_route_move(BkNpcRouteState *state, const BkRoute *route,
                      const BkNpcMotionActions *actions,
                      int32_t background_clip, float seconds, uint32_t now_ms,
                      BkNpcRouteEffects *effects, char error[256]) {
  if (!state || !route || !actions || !effects) {
    snprintf(error, 256, "NPC route: invalid arguments");
    return 0;
  }
  BkNpcRouteState next = *state;
  BkNpcRouteEffects result = {0};
  if (!bk_npc_motion_select(&next.point.motion, actions, background_clip,
                            seconds, now_ms, &result.movement)) {
    snprintf(error, 256, "NPC route: invalid movement input");
    return 0;
  }
  if (result.movement.allowed) {
    if (!bk_route_motion_step(&next.path, route, result.movement.distance,
                              error))
      return 0;
    if (next.path.crossed == 1) {
      const BkRoutePoint *point = bk_route_point(route, next.path.last_crossed);
      if (!point) {
        snprintf(error, 256, "NPC route: invalid crossed point");
        return 0;
      }
      bk_npc_point_apply(&next.point, actions, next.path.run_remaining,
                         (int8_t)point->flags, background_clip, &result.point);
      if (result.point.snap_to_point) {
        next.path.position[0] = point->position[0];
        next.path.position[2] = point->position[2];
      }
    }
  }
  if (result.point.play_route_sound) {
    result.sound_cursor = next.path.cursor;
    for (unsigned i = 0; i < 3; ++i)
      result.sound_position[i] = next.path.position[i];
  }
  *state = next;
  *effects = result;
  return 1;
}
