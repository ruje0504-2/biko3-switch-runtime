#ifndef BK_GAME_CAMERA_POLICY_H
#define BK_GAME_CAMERA_POLICY_H
#include <stdint.h>
typedef struct {
  uint8_t phase, transition;
  int32_t stage;
} BkGameCameraState;
typedef enum {
  BK_GAME_CAMERA_HOLD,
  BK_GAME_CAMERA_FOLLOW_ACTOR,
  BK_GAME_CAMERA_PLAYER_INPUT,
  BK_GAME_CAMERA_TO_PLAYER_BODY,
  BK_GAME_CAMERA_TO_PLAYER_HEAD
} BkGameCameraRoute;
/* Native main-flow 2 dispatch (0x51a682 -> 0x4ec78d). This chooses a
 * controller; it does not imply every controller has been implemented. */
BkGameCameraRoute bk_game_camera_route(const BkGameCameraState *state);
/* Only a completed handover can move stage 1 to 2. Returns zero on invalid
 * arguments; holding or following never reports a mission completed. */
int bk_game_camera_finish(BkGameCameraState *state, int handover_complete);
/*4bef54 and4bed86 gates: skip(0,6),(4,4),(4,5), query otherwise. */
int bk_game_follow_obstacles_enabled(int32_t group, int32_t area);
#endif
