#ifndef BK_SCENE_SPECIAL_CAMERA_ASSETS_H
#define BK_SCENE_SPECIAL_CAMERA_ASSETS_H
#include "game/special_event.h"
#include "resource/store.h"
#include "world/actor_forest.h"
#include "world/ending_camera.h"
typedef struct BkSpecialCameraAssets BkSpecialCameraAssets;
/*4b79e0(flow48): same two filenames as selection, different preset bank,
 *initialization and owner. Decode only; attach at the real loader position.*/
BkSpecialCameraAssets *bk_special_camera_assets_create(BkResourceStore *,
                                                       unsigned group, char[256]);
void bk_special_camera_assets_destroy(BkSpecialCameraAssets *);
BkActorPose *bk_special_camera_assets_pose(BkSpecialCameraAssets *, unsigned);
uint32_t bk_special_camera_assets_root(const BkSpecialCameraAssets *, unsigned);
uint32_t bk_special_camera_assets_node(const BkSpecialCameraAssets *, unsigned);
/*Attach both tracks in order, select0 on both, advance secondary once at
 *loading_seconds, then initialize orbit/presets/FOV. No publication after
 *secondary advance. Forest dies before assets. Later failure is terminal
 *for this loader; resources remain destructible, not wholly rolled back.*/
int bk_special_camera_assets_attach(BkSpecialCameraAssets *, BkActorForest *,
    const uint32_t indices[2], float loading_seconds, BkMenuCamera *,
    BkEndingCameraPresets *, char[256]);
/*Actual OPEN4bb82e, TRANSITION4bc444, ORBIT4bb0a4, TRACK4bb612.
 *seconds MUST be live733700, including OPEN (its third native argument is
 *ignored). OPEN requires actual71af48 focus; TRACK has native absent fallback.
 *Global refresh occurs only in OPEN/TRACK, before reading fresh targets.*/
int bk_special_camera_assets_step(BkSpecialCameraAssets *, BkActorForest *,
    const uint32_t indices[2], BkMenuCamera *, BkEndingCameraTransitions *,
    const BkEndingCameraPresets *, BkSpecialEventCamera, int32_t clip,
    const float offset[3], const float motion[2], unsigned buttons,
    uint32_t focus, float seconds, uint8_t *done, char[256]);
/*4e620a's root setters. Rotation receives raw radians, including the
 *original group2 literal160. No degree conversion or descendant publication.*/
int bk_special_camera_assets_place(BkSpecialCameraAssets *, BkActorForest *,
    const uint32_t indices[2], unsigned track, const float values[3],
    float radians, char[256]);
#endif
