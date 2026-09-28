#include "game/npc_motion.h"
#include <math.h>
int bk_npc_motion_actions(BkNpcMotionActions *out, unsigned group) {
  if (!out || group >= 5)
    return 0;
  *out = (BkNpcMotionActions){
      .idle = 0, .walk = 1, .run = 4, .stationary = {10, 7, 12, 9}};
  return 1;
}
int bk_npc_motion_select(BkNpcMotionState *state,
                         const BkNpcMotionActions *actions,
                         int32_t background_clip, float seconds, uint32_t now,
                         BkNpcMotion *out) {
  if (!state || !actions || !out || !isfinite(seconds) || seconds < 0)
    return 0;
  BkNpcMotionState next = *state;
  BkNpcMotion result = {0};
  if (next.hidden == 1)
    goto done;
  if (next.route_flag == 5) {
    next.action = actions->idle;
    goto done;
  }
  if (next.action == actions->walk) {
    result.distance = (float)((double)seconds * 8);
    result.allowed = 1;
  } else if (next.action == actions->run) {
    result.distance = (float)((double)seconds * 16);
    result.allowed = 1;
  } else {
    for (unsigned i = 0; i < 4; i++)
      if (next.action == actions->stationary[i]) {
        result.allowed = 1;
        goto done;
      }
    if (next.mode == 1 && next.route_flag != 4) {
      int ready = next.route_flag == 3 ? background_clip == 2
                                       : bk_timer_poll(&next.wait, now);
      if (ready) {
        next.action = actions->walk;
        next.behavior = 1;
      }
    }
  }
done:
  if (!isfinite(result.distance))
    return 0;
  *state = next;
  *out = result;
  return 1;
}
