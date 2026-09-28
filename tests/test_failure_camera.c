#include "world/failure_camera.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <math.h>
#include <string.h>
int main(void) {
  char error[256] = {0};
  BkCameraFollowPose pose = {{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, -30, 1},
                             {1000, 1000, 1000}};
  BkFailureCameraInput input = {.seconds = 1};
  BkFailureCameraEffects out = {0};
  assert(bk_failure_camera_step(&pose, BK_FAILURE_CAMERA_OVERHEAD, &input, &out,
                                error));
  /* Native compares signed deltas, so distant negative steps are ready. */
  assert(out.arrived && !out.write_fov);
  assert(pose.position[0] == 500.5f && pose.position[1] == 525 &&
         pose.position[2] == 500.5f);
  assert(pose.world[12] == pose.position[0]);
  BkCameraFollowPose saved = pose;
  BkFailureCameraEffects old = out;
  input.group = 5;
  assert(!bk_failure_camera_step(&pose, BK_FAILURE_CAMERA_NPC_FRONT, &input,
                                 &out, error));
  assert(!memcmp(&pose, &saved, sizeof(pose)) &&
         !memcmp(&out, &old, sizeof(out)));
  input.group = 0;
  input.seconds = NAN;
  assert(!bk_failure_camera_step(&pose, BK_FAILURE_CAMERA_OVERHEAD, &input,
                                 &out, error));
  assert(!memcmp(&pose, &saved, sizeof(pose)));
  input.seconds = .1f;
  memcpy(input.player, pose.world + 12, sizeof(input.player));
  assert(!bk_failure_camera_step(&pose, BK_FAILURE_CAMERA_OVERHEAD, &input,
                                 &out, error));
  assert(!memcmp(&pose, &saved, sizeof(pose)));
  input.player[0] += 100;
  input.player_height = 18;
  assert(bk_failure_camera_step(&pose, BK_FAILURE_CAMERA_PLAYER, &input, &out,
                                error));
  assert(out.write_fov && out.fov == .2f);
  return 0;
}
