#ifndef BK_WORLD_ENDING_CAMERA_H
#define BK_WORLD_ENDING_CAMERA_H
#include "world/menu_camera.h"
typedef struct {
  float active[4][3];   /*+444: yaw/pitch/radius/height, three choices each*/
  float authored[4][3]; /*+474: retained reset values*/
} BkEndingCameraPresets;
typedef struct {
  float preset_progress; /*719c64:4e0ecb*/
  float zoom_progress;   /*7099a8:4bc444*/
  float zoom_fov;        /*7099e4, retained outside flow16*/
} BkEndingCameraTransitions;
/* Explicit live inputs to4e0ecb's FOV gate. These event-state fields must
 * come from the actual ending lifecycle, not inferred from camera choice. */
typedef struct {
  uint8_t previous_flow; /*721ad4*/
  int32_t phase;         /*721e00*/
  int32_t state_ed8, state_ee0, state_ee4, state_eec, state_ef0;
  int32_t state_719b20;
} BkEndingCameraPresetGate;
/*4e0ecb/4bc444: three active presets, independent progress accumulators.
 * Neither advances/selects tracks nor updates live orbit parameters.
 * Blend from retained +4a4, which changes ONLY upon completion; completion
 * resets the selected progress to0. Offset is added after orbit composition.
 * Values outside the three defined choices reject atomically. */
int bk_ending_camera_preset(BkMenuCamera *, BkEndingCameraTransitions *,
                            const BkEndingCameraPresets *, unsigned choice,
                            const float offset[3],
                            const BkEndingCameraPresetGate *, float seconds,
                            int *complete, char error[256]);
int bk_ending_camera_preset_zoom(BkMenuCamera *, BkEndingCameraTransitions *,
                                 const BkEndingCameraPresets *, unsigned choice,
                                 const float offset[3], uint8_t flow,
                                 float seconds, int *complete, char error[256]);
/*4b88fb..4b897b:42363b(mode1) on the two track roots. Pre-multiplies
 * Y rotation into the old local with XYZ translation temporarily removed,
 * then restores XYZ. The integer angle is multiplied before float rounding.
 * Does not publish children; caller installs the resulting root local. */
int bk_ending_camera_root_rotation(float out[16], const float local[16],
                                   int32_t degrees);
/*4bb0a4: interactive orbit about the explicit XYZ offset. Flow0x10 uses
 * radius5..120,height-10..30,FOV.2; other flows5..100,0..30,FOV.4.
 * Rotation wins over distance/height input. One-step yaw wrap, pitch+-80. */
int bk_ending_camera_orbit(BkMenuCamera *, const float motion[2],
                           unsigned held_buttons, const float offset[3],
                           uint8_t flow, float seconds, char error[256]);
/*4df411: same offset orbit math but NO input/wrap/clamps. FOV.2. Retains
 * yaw/pitch/radius/height/focus. Invalid arithmetic leaves all state intact. */
int bk_ending_camera_fixed(BkMenuCamera *, const float offset[3],
                           char error[256]);
/*4e1d16: manual orbit with pitch+-180,radius0..120,height+-100, then
 * aim toward the explicit cached721ef4 position. The +4a4 matrix retains
 * the BEFORE-aim orbit while pose.world is AFTER aim. Focus is retained. */
int bk_ending_camera_manual(BkMenuCamera *, const float motion[2],
                            unsigned held_buttons, const float offset[3],
                            const float target[3], float seconds,
                            char error[256]);
/*4e1711 arithmetic after full-speed animation advance. The caller must
 * supply the OLD published track and target(719448) world positions: this
 * controller does NOT pre-publish the forest. Position is smoothed before
 * aim, unlike gameplay follow. Does not select a clip, advance animation,
 * publish nodes, or alter the retained menu focus(+5c8). FOV.2. */
int bk_ending_camera_track_pose(BkMenuCamera *, const float track[3],
                                const float target[3], float seconds,
                                char error[256]);
#endif
