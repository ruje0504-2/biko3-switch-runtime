#include "game/npc_action.h"
#include <stddef.h>
int bk_npc_action_finish(BkNpcPointState *state,
                         const BkNpcMotionActions *actions, int32_t active_clip,
                         uint32_t now_ms) {
  if (!state || !actions)
    return 0;
  BkNpcMotionState *motion = &state->motion;
  if (motion->behavior == 3) {
    if (bk_timer_poll(&state->action_wait, now_ms)) {
      motion->action = actions->stationary[0];
      if (motion->action != 0)
        motion->action = actions->stationary[2];
    }
    if (motion->action == actions->stationary[2] && active_clip == 0) {
      motion->behavior = 1;
      motion->action = actions->walk;
    }
  } else if (motion->behavior == 2) {
    if (bk_timer_poll(&state->action_wait, now_ms)) {
      if (motion->action == actions->stationary[1])
        motion->action = actions->stationary[0];
      else {
        motion->action = actions->stationary[0];
        if (motion->action != 0)
          motion->action = actions->stationary[2];
      }
    }
    if (motion->action == actions->stationary[2] && active_clip == 0) {
      if (motion->route_flag == 3)
        motion->action = actions->idle;
      else {
        motion->action = actions->walk;
        motion->route_flag = 0;
      }
      motion->behavior = 1;
    }
  } else {
    if (motion->action == actions->stationary[0]) {
      if (bk_timer_poll(&state->action_wait, now_ms))
        motion->action = actions->stationary[2];
    } else if (motion->action == actions->stationary[2] && active_clip == 0) {
      motion->action = actions->walk;
      motion->route_flag = 0;
    }
    if (motion->action == actions->stationary[1]) {
      if (bk_timer_poll(&state->action_wait, now_ms))
        motion->action = actions->stationary[3];
    } else if (motion->action == actions->stationary[3] &&
               active_clip != actions->stationary[3]) {
      motion->action = actions->walk;
      motion->route_flag = 0;
    }
  }
  return 1;
}
