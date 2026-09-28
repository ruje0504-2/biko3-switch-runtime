#ifndef BK_SCENE_BACKGROUND_ASSETS_H
#define BK_SCENE_BACKGROUND_ASSETS_H
#include "game/background.h"
#include "game/background_config.h"
#include "model/material_pose.h"
#include "resource/store.h"
#include "world/actor_pose.h"
#include "world/collision.h"
typedef struct BkBackgroundAssets BkBackgroundAssets;
/* Primary scene, optional(2,8) door and group0/area0..6 snow model. Quality0
 * selects bk3_17,1/2 selects bk3_03. Mount bk3_20 for enabled snow and an
 * 'collision' loose ATR directory. Owns static collision and separate poses.
 * group4 rain sprites are owned separately by scene/rain_render. */
BkBackgroundAssets *bk_background_assets_create(BkResourceStore *,
                                                uint32_t group, uint32_t area,
                                                unsigned quality,
                                                int snow_enabled,
                                                char error[256]);
void bk_background_assets_destroy(BkBackgroundAssets *);
/*0 primary,1 auxiliary door,2 snow; missing optional objects return NULL. */
const BkActorPose *bk_background_assets_pose(const BkBackgroundAssets *,
                                             unsigned);
/* Borrow for global frame-tree publication. Owner retains lifetime and
 * animation/placement control; destroy the forest before this owner. */
BkActorPose *bk_background_assets_bind_pose(BkBackgroundAssets *, unsigned);
const BkMaterialPose *bk_background_assets_materials(const BkBackgroundAssets *,
                                                     unsigned);
const BkBackgroundConfig *
bk_background_assets_config(const BkBackgroundAssets *);
BkCollision *bk_background_assets_collision(BkBackgroundAssets *);
/* Read pose's real pre-update clips and current instance presence. Caller
 * supplies clock, shared RNG, sound status, player/NPC and settings. */
int bk_background_assets_input(const BkBackgroundAssets *, BkBackgroundInput *);
typedef int (*BkBackgroundConsumer)(void *, const BkBackgroundCommands *,
                                    char error[256]);
/* Plan, consume audio synchronously, then door/background/snow animation in
 * native order. Failures after a consumer side effect terminate the frame. */
int bk_background_assets_step(BkBackgroundAssets *, BkBackgroundState *,
                              uint32_t *shared_random,
                              const BkBackgroundInput *, BkBackgroundCommands *,
                              BkBackgroundConsumer, void *context,
                              char error[256]);
void bk_background_assets_publish(BkBackgroundAssets *);
/* Publish primary/door independently and snow beneath the CURRENT camera
 * world, after camera update. The prior step reads the previously published
 * camera parent cache; do not pass a view/inverse-view projection matrix.
 * Plain publish above is an independent-root asset inspection traversal. */
int bk_background_assets_publish_camera(BkBackgroundAssets *,
                                        const float camera_world[16],
                                        char error[256]);
#endif
