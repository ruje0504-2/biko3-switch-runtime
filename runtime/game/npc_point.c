#include "game/npc_point.h"
#include <stddef.h>
int bk_npc_point_apply(BkNpcPointState *state,
                       const BkNpcMotionActions *actions, int32_t run_remaining,
                       int8_t flag, int32_t background_clip,
                       BkNpcPointEffects *effects) {
  if (!state || !actions || !effects)
    return 0;
  BkNpcPointState next = *state;
  BkNpcPointEffects result = {0};
  if (run_remaining > 0) {
    next.motion.action = actions->run;
    result.snap_to_point = 1;
    goto done;
  }
  next.motion.route_flag = flag;
  switch (flag) {
  case 0:
    next.motion.action = actions->walk;
    break;
  case 1:
    if (next.motion.mode == 0 || next.motion.mode == 1)
      next.motion.action = actions->idle;
    if (next.motion.mode == 1) {
      next.motion.wait.duration = 2000;
      next.motion.wait.armed = 0;
    }
    result.snap_to_point = 1;
    break;
  case 2:
    next.motion.action = actions->idle;
    next.motion.wait.duration = 5000;
    next.motion.wait.armed = 0;
    next.motion.behavior = 0;
    result.snap_to_point = result.play_wait_sound = 1;
    break;
  case 3:
    if (background_clip == 2)
      next.motion.action = actions->walk;
    else {
      next.background_wait = 1;
      next.motion.action = actions->idle;
      result.snap_to_point = 1;
    }
    break;
  case 4:
    if (!next.gate_state) {
      next.motion.action = actions->idle;
      result.snap_to_point = 1;
    } else
      next.motion.route_flag = 0;
    break;
  case 5:
    next.motion.action = actions->idle;
    result.snap_to_point = 1;
    break;
  case 6:
    next.motion.action = actions->run;
    result.snap_to_point = 1;
    break;
  case 7:
    next.motion.action = actions->stationary[0];
    next.action_wait.duration = 2000;
    next.action_wait.armed = 0;
    result.snap_to_point = 1;
    break;
  case 8:
    next.motion.action = actions->walk;
    if (next.fade_out == 0)
      next.fade_out = 1;
    else if (next.fade_out == 1)
      next.fade_out = 0;
    result.play_route_sound = 1;
    break;
  case 9:
    next.motion.action = actions->stationary[1];
    next.action_wait.duration = 3000;
    next.action_wait.armed = 0;
    result.snap_to_point = 1;
    break;
  default:
    next.motion.action = actions->walk;
    break;
  }
done:
  *state = next;
  *effects = result;
  return 1;
}
