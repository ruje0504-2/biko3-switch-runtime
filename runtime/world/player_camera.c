#include "world/player_camera.h"
#include <math.h>
#include <string.h>
int bk_player_camera_transition(BkCameraFollowPose *pose,
                                const BkPlayerCameraTarget *player,
                                int head_mode, float seconds, int *complete) {
  if (!player || (head_mode != 0 && head_mode != 1) ||
      !isfinite(player->yaw_degrees))
    return 0;
  float target[3];
  if (head_mode)
    memcpy(target, player->head, sizeof(target));
  else {
    for (unsigned i = 0; i < 3; i++)
      if (!isfinite(player->origin[i]))
        return 0;
    if (!isfinite(player->base_head_height))
      return 0;
    /* Native offset uses the approximate DOUBLE 0.01745; rotation uses the
     * distinct rounded FLOAT pi/180. Conflating these moves the camera. */
    double radians = (double)player->yaw_degrees * .01745;
    float dx = (float)(sin(radians) * -40.0),
          dz = (float)(cos(radians) * -40.0);
    target[0] = (float)((double)dx + player->origin[0]);
    target[1] = (float)((double)player->origin[1] + player->base_head_height);
    target[2] = (float)((double)dz + player->origin[2]);
  }
  return bk_camera_transition_pose(pose, target, player->yaw_degrees, seconds,
                                   complete);
}
