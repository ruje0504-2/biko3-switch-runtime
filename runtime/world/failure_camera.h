#ifndef BK_WORLD_FAILURE_CAMERA_H
#define BK_WORLD_FAILURE_CAMERA_H
#include "core/camera.h"
typedef enum {
  BK_FAILURE_CAMERA_OVERHEAD,  /*4bc919, outcome1*/
  BK_FAILURE_CAMERA_PLAYER,    /*4bcab3, outcome4*/
  BK_FAILURE_CAMERA_NPC_FRONT, /*4bcca9, outcomes2/6*/
  BK_FAILURE_CAMERA_NPC_REAR,  /*4bd04f, outcome3*/
  BK_FAILURE_CAMERA_PROP       /*4bd3ed, outcome5*/
} BkFailureCameraKind;
typedef struct {
  uint32_t group;
  float player[3], player_height, player_yaw;
  float npc[3], npc_height, npc_yaw;
  float prop[3], prop_yaw;
  float seconds;
} BkFailureCameraInput;
typedef struct {
  int arrived, write_fov;
  float fov;
} BkFailureCameraEffects;
/* Five native outcome controllers. Aim reads PREVIOUS world translation,
 * then installs new smoothed XYZ. Completion compares signed step deltas
 * against0.1, deliberately not absolute distance. No actors/animation touched.
 * OVERHEAD retains FOV; other kinds request0.2. Invalid geometry is atomic. */
int bk_failure_camera_step(BkCameraFollowPose *, BkFailureCameraKind,
                           const BkFailureCameraInput *,
                           BkFailureCameraEffects *, char error[256]);
#endif
