#ifndef BK_SCENE_ENDING_CAMERA_ASSETS_H
#define BK_SCENE_ENDING_CAMERA_ASSETS_H
#include "resource/store.h"
#include "world/actor_forest.h"
#include "world/ending_camera.h"
typedef struct BkEndingCameraAssets BkEndingCameraAssets;
/* Own cam00_00/Cam_AUTO and cam00_03/locator1 from retail bk3_04.
 * cam00_03 text X is pose-only. Creation loads authored locals/timelines;
 * no selection, root rotation, forest publication or camera reset yet. */
BkEndingCameraAssets *bk_ending_camera_assets_create(BkResourceStore *,
                                                     unsigned group,
                                                     unsigned variant,
                                                     int32_t root_degrees,
                                                     char error[256]);
void bk_ending_camera_assets_destroy(BkEndingCameraAssets *);
BkActorPose *bk_ending_camera_assets_pose(BkEndingCameraAssets *, unsigned);
uint32_t bk_ending_camera_assets_root(const BkEndingCameraAssets *, unsigned);
uint32_t bk_ending_camera_assets_node(const BkEndingCameraAssets *, unsigned);
/* Finish4b79e0(flow0x10) at the actual global insertion point: attach both
 * tracks in order (full refresh), select0 on both, rotate roots only, reset
 * position/orbit/FOV and copy both preset banks. No animation is consumed.
 * Camera world/matrix/focus are retained. Both roots must initially be
 * detached; indices must map these poses and global0 must be identity.
 * Forest must be destroyed before assets. Failure after attach is fatal
 * to the enclosing load, not an assertion of whole-forest rollback. */
int bk_ending_camera_assets_attach(BkEndingCameraAssets *, BkActorForest *,
                                   const uint32_t indices[2], BkMenuCamera *,
                                   BkEndingCameraPresets *, char error[256]);
typedef enum {
  BK_ENDING_CAMERA_ORBIT,
  BK_ENDING_CAMERA_AUTO,
  BK_ENDING_CAMERA_MANUAL,
  BK_ENDING_CAMERA_FIXED
} BkEndingCameraKind;
/* Implements only the four recovered controllers. AUTO advances primary at
 * full seconds with no request or pre-publication, then consumes old world;
 * secondary stays held. target_node must bind the actual caller-selected
 * actor node for AUTO/MANUAL. Other modes ignore target_node. Installs final
 * camera anchor without drawing/publishing the rest of the scene. */
int bk_ending_camera_assets_step(BkEndingCameraAssets *, BkActorForest *,
                                 const uint32_t indices[2], BkMenuCamera *,
                                 BkEndingCameraKind, const float offset[3],
                                 const float motion[2], unsigned held_buttons,
                                 uint32_t target_node, float seconds,
                                 char error[256]);
typedef enum { BK_ENDING_PRESET, BK_ENDING_PRESET_ZOOM } BkEndingPresetKind;
/*4e0ecb/4bc444 scene adapter. Both tracks and all published actor nodes stay
 * untouched; only the camera anchor is installed. Presets and transition
 * clocks are live borrowed state, never reset by choosing this controller.
 * Gate is required only for BK_ENDING_PRESET. */
int bk_ending_camera_assets_preset(
    BkEndingCameraAssets *, BkActorForest *, const uint32_t indices[2],
    BkMenuCamera *, BkEndingCameraTransitions *, const BkEndingCameraPresets *,
    BkEndingPresetKind, unsigned choice, const float offset[3],
    const BkEndingCameraPresetGate *, uint8_t flow, float seconds,
    int *complete, char error[256]);
#endif
