#include "game/player_movement.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
int bk_player_actions_initialize(int32_t actions[21]) {
  if (!actions)
    return 0;
  static const int32_t values[21] = {0,  4,  5,  10, 11, 6,  8,  0,  15, 17, 24,
                                     18, 19, 20, 21, 0,  22, 23, 12, 14, 13};
  for (unsigned i = 0; i < 21; ++i)
    if (i != 7 && i != 15)
      actions[i] = values[i];
  return 1;
}
static int match(int32_t action, const int32_t *slots, const uint8_t *indices,
                 unsigned count) {
  for (unsigned i = 0; i < count; i++)
    if (action == slots[indices[i]])
      return 1;
  return 0;
}
static int finite_state(const BkPlayerMovement *s) {
  for (unsigned i = 0; i < 3; i++)
    if (!isfinite(s->position[i]) || !isfinite(s->previous[i]) ||
        !isfinite(s->velocity[i]))
      return 0;
  return isfinite(s->yaw) && isfinite(s->pitch) && isfinite(s->turn[0]) &&
         isfinite(s->turn[1]) && isfinite(s->acceleration);
}
int bk_player_movement(BkPlayerMovement *state,
                       const BkPlayerMovementInput *input, char error[256]) {
  if (!state || !input || !finite_state(state) || !isfinite(input->seconds) ||
      input->seconds < 0 || !isfinite(input->look[0]) ||
      !isfinite(input->look[1]) ||
      (input->controls_allowed != 0 && input->controls_allowed != 1) ||
      (input->buttons & ~63u)) {
    snprintf(error, 256, "player movement: invalid input");
    return 0;
  }
  static const uint8_t locked[] = {8, 11, 13, 16};
  static const uint8_t back_locked[] = {8, 9, 11, 13, 16};
  static const uint8_t idle_locked[] = {9, 8, 11, 12, 13, 14, 16, 17};
  static const uint8_t pitch_locked[] = {11, 13, 16};
  BkPlayerMovement next = *state;
  memcpy(next.previous, next.position, sizeof(next.previous));
  next.turn[0] = next.turn[1] = 0;
  const int32_t *actions = input->actions;
  float seconds = input->seconds;
  int slow = (input->buttons & BK_PLAYER_SLOW) || next.move_latch == 1;
  float distance = (float)((slow ? 8. : 16.) * seconds);
  int movement = 0, turn = 0;
  if (input->controls_allowed && next.interaction_mode != 5) {
    if (input->buttons & BK_PLAYER_LATCH_TRIGGER)
      next.move_latch = 1;
    else if (input->buttons & BK_PLAYER_FORWARD)
      movement = 1;
    else if (input->buttons & BK_PLAYER_BACKWARD)
      movement = 2;
    else {
      movement = 99;
      next.move_latch = 0;
    }
    turn = (input->buttons & BK_PLAYER_LEFT)    ? 11
           : (input->buttons & BK_PLAYER_RIGHT) ? 12
                                                : 99;
    next.turn[0] = (float)((double)input->look[0] * seconds);
    if (!match(next.action, actions, pitch_locked, 3))
      next.turn[1] = (float)((double)input->look[1] * seconds);
  }
  if (movement == 1 || movement == 2) {
    int frozen = match(next.action, actions, locked, 4);
    if (frozen)
      distance = 0;
    else if (movement == 2)
      distance = (float)(8. * seconds);
    if (next.interaction_mode != 5) {
      /* Native fst stores a float temporary but leaves extended product for
       * the double CRT argument. Do not round radians to float here. */
      double radians = (double)next.yaw * .01745;
      next.velocity[0] = (float)(sin(radians) * distance);
      next.velocity[2] = (float)(cos(radians) * distance);
      if (movement == 2) {
        next.velocity[0] = -next.velocity[0];
        next.velocity[2] = -next.velocity[2];
      }
    }
    /* Original unconditionally writes1 after its apparent ramp/clamp. */
    next.acceleration = 1;
    next.position[0] = (float)((double)next.position[0] + next.velocity[0]);
    next.position[2] = (float)((double)next.position[2] + next.velocity[2]);
    if (movement == 1 && !frozen)
      next.action = actions[slow ? 1 : 3];
    else if (movement == 2 && !match(next.action, actions, back_locked, 5))
      next.action = actions[5];
  } else if (movement == 99) {
    if (!match(next.action, actions, idle_locked, 8))
      next.action = actions[0];
    if (next.interaction_mode == 5)
      next.action = actions[20];
    if (next.action != actions[8] && next.action != actions[12] &&
        input->active_clip == 0)
      next.action = actions[0];
    next.acceleration = 0;
  }
  if (turn == 11 || turn == 12) {
    next.turn[0] = (float)((turn == 11 ? -50. : 50.) * seconds);
    if (!match(next.action, actions, locked, 4) && movement == 0)
      next.action = actions[1];
  }
  next.yaw = (float)((double)next.yaw + next.turn[0]);
  next.pitch = (float)((double)next.pitch + next.turn[1]);
  if (next.yaw < 0)
    next.yaw = (float)((double)next.yaw + 360);
  else if (next.yaw >= 360)
    next.yaw = (float)((double)next.yaw - 360);
  if (next.pitch <= -80)
    next.pitch = -80;
  else if (next.pitch >= 80)
    next.pitch = 80;
  if (!finite_state(&next)) {
    snprintf(error, 256, "player movement: result overflow");
    return 0;
  }
  *state = next;
  return 1;
}
