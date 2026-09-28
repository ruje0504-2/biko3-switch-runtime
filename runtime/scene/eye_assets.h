#ifndef BK_SCENE_EYE_ASSETS_H
#define BK_SCENE_EYE_ASSETS_H
#include "model/model.h"
#include "resource/face_config.h"
#include "resource/store.h"
#include "world/actor_pose.h"
typedef struct BkEyeAssets BkEyeAssets;
typedef struct {
  uint32_t frames[2], target_submesh, texture_mode;
} BkEyeBinding;
/* 4a07da + 4f2971/4f2430 primary eye assets. FAM row2/3 name eye FRAMs;
 * slot0 uses first eye-MORP TARGET submesh's existing texture. Row5 alone
 * loads slot1. Row4 is metadata, not a replacement for the base texture.
 * Owns alternate pixels and scalar binding; model/config/store need not live
 * after construction. Missing named frames disable gaze (BK_MODEL_NONE).
 * Missing optional target disables texture switching, as in the original.
 * A missing alternate texture leaves slot1 empty; corrupt data fails. */
BkEyeAssets *bk_eye_assets_create(BkResourceStore *resources,
                                  const char *texture_pack,
                                  const BkModel *model,
                                  const char *model_filename,
                                  const BkFaceConfig *config, char error[256]);
void bk_eye_assets_destroy(BkEyeAssets *eyes);
const BkEyeBinding *bk_eye_assets_binding(const BkEyeAssets *eyes);
/* Slots0..4 match native storage. Unavailable slots leave selection intact;
 * out-of-range indexes fail safely. Selection does NOT change gaze variant.
 * slot0 returns NULL image: the renderer retains the original mesh texture. */
int bk_eye_assets_select(BkEyeAssets *eyes, uint32_t slot, char error[256]);
uint32_t bk_eye_assets_selected(const BkEyeAssets *eyes);
const BkImage *bk_eye_assets_image(const BkEyeAssets *eyes, uint32_t slot,
                                   int *alpha_hint);
/* 4f3bb9: 0/1 select gaze variant; 2 selects variant0 with zero limits.
 * Other signed-byte commands are no-ops. This never selects a texture.
 * Target is the caller's cached active-camera world; no implicit clock or
 * camera policy. Actor must use the model these eye indices were bound to. */
int bk_eye_assets_gaze(BkEyeAssets *eyes, BkActorPose *actor, int8_t command,
                       const float target_world[16], float pitch_limit,
                       float yaw_limit, char error[256]);
uint32_t bk_eye_assets_gaze_variant(const BkEyeAssets *eyes);
#endif
