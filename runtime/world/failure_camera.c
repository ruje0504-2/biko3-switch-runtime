#include "world/failure_camera.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
static float add(float a, float b) { return (float)((double)a + b); }
static void offset(float *x, float *z, double degrees, double radius) {
  double radians = degrees * .01745329238474369f;
  *x = (float)(sin(radians) * radius);
  *z = (float)(cos(radians) * radius);
}
int bk_failure_camera_step(BkCameraFollowPose *pose, BkFailureCameraKind kind,
                           const BkFailureCameraInput *in,
                           BkFailureCameraEffects *effects, char error[256]) {
  if (!pose || !in || !effects || kind < BK_FAILURE_CAMERA_OVERHEAD ||
      kind > BK_FAILURE_CAMERA_PROP || !isfinite(in->seconds) ||
      in->seconds < 0)
    goto invalid;
  BkCameraFollowPose next = *pose;
  BkFailureCameraEffects out = {1, kind != BK_FAILURE_CAMERA_OVERHEAD, .2f};
  float destination[3], focus[3], dx, dz;
  double speed = .5;
  switch (kind) {
  case BK_FAILURE_CAMERA_OVERHEAD:
    destination[0] = add(in->player[0], 1);
    destination[1] = add(in->player[1], 50);
    destination[2] = add(in->player[2], 1);
    memcpy(focus, in->player, sizeof(focus));
    break;
  case BK_FAILURE_CAMERA_PLAYER:
    offset(&dx, &dz, in->player_yaw, 55);
    destination[0] = add(in->player[0], dx);
    destination[1] = add(in->player[1], 15);
    destination[2] = add(in->player[2], dz);
    memcpy(focus, in->player, sizeof(focus));
    focus[1] = (float)((double)in->player_height - 2.5 + in->player[1]);
    break;
  case BK_FAILURE_CAMERA_NPC_FRONT:
  case BK_FAILURE_CAMERA_NPC_REAR: {
    if (in->group >= 5)
      goto invalid; /* Original table leaves locals undefined outside0..4. */
    int rear = kind == BK_FAILURE_CAMERA_NPC_REAR;
    static const float heights[2][5] = {{-2.5f, -1, -4, -2, -2.5f},
                                        {-2.5f, -1, -10, -2, -4}};
    static const float radii[2][5] = {{0, 2, 0, 1, 0}, {0, 2, 0, 0, 2}};
    offset(&dx, &dz, in->npc_yaw, rear ? -55 : 55);
    destination[0] = add(in->npc[0], dx);
    destination[1] = add(in->npc[1], 20);
    destination[2] = add(in->npc[2], dz);
    offset(&dx, &dz, (double)in->npc_yaw + 90, radii[rear][in->group]);
    focus[0] = add(in->npc[0], dx);
    focus[1] =
        (float)((double)in->npc_height + heights[rear][in->group] + in->npc[1]);
    focus[2] = add(in->npc[2], dz);
    speed = 1;
    break;
  }
  case BK_FAILURE_CAMERA_PROP:
    offset(&dx, &dz, in->prop_yaw, -55);
    destination[0] = add(in->prop[0], dx);
    destination[1] = add(in->prop[1], 20);
    destination[2] = add(in->prop[2], dz);
    memcpy(focus, in->prop, sizeof(focus));
    focus[1] = (float)(22.0 - 2.5 + in->prop[1]);
    break;
  }
  for (unsigned i = 0; i < 3; ++i) {
    float delta = (float)((double)destination[i] - pose->position[i]);
    delta = (float)((double)delta * ((double)in->seconds * speed));
    next.position[i] = add(pose->position[i], delta);
    if (!isfinite(next.position[i]) || !isfinite(focus[i]))
      goto invalid;
    if (delta > .1)
      out.arrived = 0;
  }
  if (!bk_camera_aim(next.world, pose->world, focus))
    goto invalid;
  memcpy(next.world + 12, next.position, sizeof(next.position));
  *pose = next;
  *effects = out;
  return 1;
invalid:
  snprintf(error, 256, "failure camera: invalid input/pose/aim geometry");
  return 0;
}
