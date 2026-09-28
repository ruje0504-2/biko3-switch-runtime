#ifndef BK_SCENE_NPC_SHADOW_H
#define BK_SCENE_NPC_SHADOW_H
#include "resource/store.h"
#include "world/actor_pose.h"
typedef struct BkNpcShadow BkNpcShadow;
/* Original graphics settings0/1: independent kage_01.xan mesh shadow.
 * Owns model/XAN/headless pose. This does not implement settings2 projected
 * shadows or infer the user's graphics setting. Resource pack is bk3_01. */
BkNpcShadow *bk_npc_shadow_create(BkResourceStore *resources, char error[256]);
void bk_npc_shadow_destroy(BkNpcShadow *shadow);
const BkActorPose *bk_npc_shadow_pose(const BkNpcShadow *shadow);
/* Borrow for global publication; owner retains animation/lifetime. */
BkActorPose *bk_npc_shadow_bind_pose(BkNpcShadow *shadow);
/* Common kage_01 actor support, also used by player4bfea9. These perform
 * no timeline advance and do not publish child caches. */
int bk_npc_shadow_visibility(BkNpcShadow *shadow, uint8_t hidden,
                             char error[256]);
int bk_npc_shadow_place_exact(BkNpcShadow *shadow, const BkActorPlacement *root,
                              char error[256]);
/* Spatial tail 4fcdd0..4fce25: copy the NPC's yaw placement with Y+0.1f.
 * Root publishes immediately, child caches wait for publish. No clock step. */
int bk_npc_shadow_place(BkNpcShadow *shadow, const BkActorPlacement *body,
                        char error[256]);
/* 4fc36d: same hard-hidden byte, independent FULL seconds (NPC body half).
 * Hidden root holds its timeline. No new clip request. Failure leaves both
 * hidden state and playback unchanged. Does not apply the body's fade. */
int bk_npc_shadow_step(BkNpcShadow *shadow, uint8_t hidden, float seconds,
                       char error[256]);
void bk_npc_shadow_publish(BkNpcShadow *shadow);
#endif
