#include "game/player_animation.h"
#include <math.h>
#include <stdio.h>
int bk_player_animation_actions(BkPlayerAnimationActions *out) {
  if (!out)
    return 0;
  *out = (BkPlayerAnimationActions){0, 4, 10};
  return 1;
}
int bk_player_animation_step(BkActorPose *actor, const float position[3],
                             float yaw_degrees,
                             const BkPlayerAnimationActions *actions,
                             int32_t action, float seconds, char error[256]) {
  if (!actions || action < 0 || !isfinite(seconds) || seconds < 0) {
    snprintf(error, 256, "player animation: invalid action/timestep");
    return 0;
  }
  BkClipRequestMode mode = (action == actions->idle ||
                            action == actions->walk || action == actions->run)
                               ? BK_CLIP_REQUEST_TEN_TICKS
                               : BK_CLIP_REQUEST_CONFIGURED;
  float scaled = (float)((double)seconds * (action == actions->idle ? .2 : .5));
  return bk_actor_pose_step_mode(actor, position, yaw_degrees, action, mode,
                                 scaled, error);
}

float bk_player_presentation_alpha(int8_t camera_mode, int8_t interface_mode,
                                   int8_t interaction_mode) {
  return camera_mode == 1 && interface_mode != 0x40 &&
                 (interaction_mode == 0 || interaction_mode == 1)
             ? 0
             : 1;
}
