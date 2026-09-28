#ifndef BK_SCENE_MENU_CAMERA_ASSETS_H
#define BK_SCENE_MENU_CAMERA_ASSETS_H
#include "resource/store.h"
#include "world/actor_forest.h"
#include "world/menu_camera.h"
typedef struct BkMenuCameraAssets BkMenuCameraAssets;
/* Retail4b79e0(flow38): cam00_00 + group-specific cam01..05_50, select0 on
 * both, advance only the second by loading seconds. Camera pose/world,
 * matrix and focus are retained; smoothing position and orbit defaults reset.
 * Assets own both tracks; an external global forest must be destroyed first.
 * The text cam01_50 is deliberately POSE ONLY, never renderable geometry.
 * Failed creation preserves the caller's camera state. */
BkMenuCameraAssets *bk_menu_camera_assets_create(BkResourceStore *,
                                                 unsigned group, float seconds,
                                                 BkMenuCamera *,
                                                 char error[256]);
void bk_menu_camera_assets_destroy(BkMenuCameraAssets *);
BkActorPose *bk_menu_camera_assets_pose(BkMenuCameraAssets *, unsigned track);
uint32_t bk_menu_camera_assets_root(const BkMenuCameraAssets *, unsigned track);
uint32_t bk_menu_camera_assets_node(const BkMenuCameraAssets *, unsigned track);
/* Call at the camera stage of51ac5d, before actor animation. Both supplied
 * registry indices must bind these tracks directly under identity global0.
 * mode0 only orbits; mode1 advances primary0 at full seconds, refreshes the
 * WHOLE forest (423be2 ignores hidden), reads fresh Cam_AUTO and optional
 * focus node, then installs camera anchor1. BK_FRAME_NONE focus=(0,18,0).
 * Secondary+4 clock is held here, as in retail51ac5d.
 * After partial progression, a failure is fatal to this frame, not rollback.
 * Draw/publication and other actors belong to the surrounding scene. */
int bk_menu_camera_assets_step(BkMenuCameraAssets *, BkActorForest *,
                               const uint32_t indices[2], BkMenuCamera *,
                               unsigned mode, const float motion[2],
                               unsigned held_buttons, uint32_t focus_node,
                               float seconds, char error[256]);
#endif
