#include "game/camera_policy.h"
BkGameCameraRoute bk_game_camera_route(const BkGameCameraState *state) {
  if (!state)
    return BK_GAME_CAMERA_HOLD;
  if (state->phase == 1)
    return BK_GAME_CAMERA_PLAYER_INPUT;
  if (state->phase != 0 && state->phase != 2)
    return BK_GAME_CAMERA_HOLD;
  if (state->stage == 0)
    return BK_GAME_CAMERA_FOLLOW_ACTOR;
  if (state->stage == 1) {
    if (state->transition == 0)
      return BK_GAME_CAMERA_TO_PLAYER_BODY;
    if (state->transition == 1)
      return BK_GAME_CAMERA_TO_PLAYER_HEAD;
  }
  return BK_GAME_CAMERA_HOLD;
}
int bk_game_camera_finish(BkGameCameraState *state, int complete) {
  if (!state || (complete != 0 && complete != 1))
    return 0;
  BkGameCameraRoute route = bk_game_camera_route(state);
  if (complete && (route == BK_GAME_CAMERA_TO_PLAYER_BODY ||
                   route == BK_GAME_CAMERA_TO_PLAYER_HEAD))
    state->stage = 2;
  return 1;
}

int bk_game_follow_obstacles_enabled(int32_t group, int32_t area) {
  return !(group == 0 && area == 6) &&
         !(group == 4 && (area == 4 || area == 5));
}
