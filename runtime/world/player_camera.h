#ifndef BK_WORLD_PLAYER_CAMERA_H
#define BK_WORLD_PLAYER_CAMERA_H
#include "core/camera.h"
/* 0x4bdfb0 body-offset handover or 0x4be290 cached-head handover. Input is
 * explicit actor state; this does not move the player or choose camera mode.
 * Base head height is the loader's cached asset head Y, not a guessed height.
 * Head mode uses cached world head position, including its publication delay.
 */
typedef struct {
  float origin[3], head[3], base_head_height, yaw_degrees;
} BkPlayerCameraTarget;
int bk_player_camera_transition(BkCameraFollowPose *pose,
                                const BkPlayerCameraTarget *player,
                                int head_mode, float seconds, int *complete);
#endif
