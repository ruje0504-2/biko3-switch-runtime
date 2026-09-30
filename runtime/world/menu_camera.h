#ifndef BK_WORLD_MENU_CAMERA_H
#define BK_WORLD_MENU_CAMERA_H
#include "core/camera.h"
/* Menu/actor-selection camera uses+434/+438 for radius/height, distinct
 * from gameplay follow distance+43c. Retain these across camera modes. */
typedef struct {
  BkCameraFollowPose pose;
  float yaw, pitch, radius, height;
  float matrix[16], focus[3], fov;
} BkMenuCamera;
/*4bac5b. bit0 rotate, bit1 radius/height; rotate wins. Degree wrap happens
 * only once and maps exactly zero to360. Invalid arithmetic is atomic. */
int bk_menu_camera_orbit(BkMenuCamera *, const float pointer_motion[2],
                         unsigned held_buttons, float seconds, char error[256]);
/*4bb612 math AFTER request0/advance(seconds)/global pre-publication.
 * Adapter must supply the freshly published Cam_AUTO world position and
 * optional focus node's cached world position. NULL focus gives(0,18,0).
 * Sets new position before aiming, unlike the gameplay follow controller.
 * The adapter owns actual XAN/forest operations; they are not simulated here.
 * Native flow10 uses FOV.2, other flows including38 use.4. */
int bk_menu_camera_track(BkMenuCamera *, const float track_world[3],
                         const float *focus_world, uint8_t flow, float seconds,
                         char error[256]);
/*4bb82e math after secondary request/advance and global refresh. The fresh
 *track is copied through native float subtraction/addition (factor1).
 *Aim uses the OLD camera position, then installs the new position; focus
 *and orbit scalars remain held. The original third argument is unused.*/
int bk_menu_camera_opening(BkMenuCamera *, const float track_world[3],
                           const float target_world[3], char error[256]);
/*4bd641: reset yaw/pitch/position and install a fixed world/FOV. Radius,
 * height, focus and the separate +4a4 matrix are retained. The adapter must
 * install pose.world via the camera world setter, respecting parent cache. */
int bk_menu_camera_dialogue(BkMenuCamera *);
#endif
