#ifndef BK_SCENE_ENDING_AUXILIARY_ASSETS_H
#define BK_SCENE_ENDING_AUXILIARY_ASSETS_H
#include "scene/ending_background_assets.h"
#include "game/ending_4d39e6_config.h"
#include "model/material_animation.h"
#include "model/morph_group.h"
#include "scene/ending_camera_assets.h"
#include "scene/eye_assets.h"
#include "scene/face_assets.h"
typedef struct BkEndingAuxiliaryAssets BkEndingAuxiliaryAssets;
/* CPU resource/pose portion of4D39E6, packaged retail branch only.
 * Forest actors are primary0, camera tracks1/2, optional background3.
 * No BOM owner exists in this topology. Camera variant is0;
 * background_variant is the separate retained outer action-table choice.
 * Face/RNG initialization, root placement, named visibility, configured
 * request1, flow16 tracks, cached targets, fixed camera and group1 background
 * keep their original order. Outputs commit only after successful creation.
 * Process flags, UI/audio/video and the 47DC79/48181F controllers remain separate; this
 * component alone must never be exposed as a complete playable entry. */
BkEndingAuxiliaryAssets *bk_ending_auxiliary_assets_create(
    BkResourceStore *, unsigned group, unsigned background_variant,
    const uint32_t face_clocks[4], uint32_t *random, BkMenuCamera *,
    BkEndingCameraPresets *, char error[256]);
void bk_ending_auxiliary_assets_destroy(BkEndingAuxiliaryAssets *);
/*4D00FA stage replacement: retain the live outer background except group1,
 * whose4d460b explicitly replaces it with m02_90. Restore its global order
 * before loading new actors, without clip selection or effect reset. Old
 * and new stage snapshots may coexist. Later failure retains cache writes. */
BkEndingAuxiliaryAssets *bk_ending_auxiliary_assets_create_reloaded(
    BkResourceStore *, unsigned group, unsigned background_variant,
    BkEndingBackgroundAssets *, const uint32_t clocks[4], uint32_t *random,
    BkMenuCamera *, BkEndingCameraPresets *, char error[256]);
BkEndingBackgroundAssets *bk_ending_auxiliary_assets_background(
    const BkEndingAuxiliaryAssets *);
/* Outer4CC582 background stage; group1 retains its own m02_90. A failure
 * after registration makes the owner unusable except for destruction. */
int bk_ending_auxiliary_assets_load_background(BkEndingAuxiliaryAssets *,
                                                BkResourceStore *, char error[256]);
const BkEnding4d39Config *
bk_ending_auxiliary_assets_config(const BkEndingAuxiliaryAssets *);
BkActorForest *bk_ending_auxiliary_assets_forest(BkEndingAuxiliaryAssets *);
BkActorPose *bk_ending_auxiliary_assets_pose(BkEndingAuxiliaryAssets *, unsigned actor);
/* Primary0/background3 own their material instances and model-level effects.
 * Advance follows4026fe: ANIM, blend MORP, MATA at actual source, then plain
 * MORP when not blending. Hidden roots retain all effect clocks. No world
 * publication or face controller runs here. Later failure retains its prefix.
 * Renderers borrow these exact materials/vertices; no parallel material copy. */
int bk_ending_auxiliary_assets_advance(BkEndingAuxiliaryAssets *, unsigned actor,
                                      float seconds, char error[256]);
/* 402E18 plain scheduled submission for the primary. This is a separate
 * scheduler from 4026FE; it keeps the playback plain-time cache and submits
 * the actual ANIM/MATA/MORP owner. */
int bk_ending_auxiliary_assets_advance_plain(BkEndingAuxiliaryAssets *,
                                             float seconds,
                                             BkClipPlainMode mode,
                                             char error[256]);
BkMaterialPose *bk_ending_auxiliary_assets_materials(BkEndingAuxiliaryAssets *, unsigned actor);
/* 4A7D10 against the actual primary/background registries. Empty or absent
 * names are native no-ops; the first registration match wins. */
int bk_ending_auxiliary_assets_material_alpha(BkEndingAuxiliaryAssets *,
                                              const char *name,
                                              uint32_t hidden, float alpha,
                                              char error[256]);
const BkMaterialAnimation *bk_ending_auxiliary_assets_material_animation(
    const BkEndingAuxiliaryAssets *, unsigned actor);
const BkMorphGroup *bk_ending_auxiliary_assets_morph(const BkEndingAuxiliaryAssets *, unsigned actor);
const BkMorphMesh *bk_ending_auxiliary_assets_mesh(const BkEndingAuxiliaryAssets *,
                                                 unsigned actor, uint32_t submesh);
uint32_t bk_ending_auxiliary_assets_root(const BkEndingAuxiliaryAssets *, unsigned actor);
const char *bk_ending_auxiliary_assets_model_name(const BkEndingAuxiliaryAssets *, unsigned actor);
BkFaceAssets *bk_ending_auxiliary_assets_face(BkEndingAuxiliaryAssets *);
BkFaceState *bk_ending_auxiliary_assets_face_state(BkEndingAuxiliaryAssets *);
BkEyeAssets *bk_ending_auxiliary_assets_eyes(BkEndingAuxiliaryAssets *);
BkEndingCameraAssets *bk_ending_auxiliary_assets_cameras(BkEndingAuxiliaryAssets *);
/* Frame indices in primary. Missing optional nodes return MODEL_NONE.
 * An absent group-specific OYU must not clear another owner's retained slot.
 * Target vectors are captured before background insertion, not live views. */
uint32_t bk_ending_auxiliary_assets_node(const BkEndingAuxiliaryAssets *, unsigned index);
uint32_t bk_ending_auxiliary_assets_anchor(const BkEndingAuxiliaryAssets *);
uint32_t bk_ending_auxiliary_assets_follow(const BkEndingAuxiliaryAssets *);
uint32_t bk_ending_auxiliary_assets_oyu(const BkEndingAuxiliaryAssets *);
uint32_t bk_ending_auxiliary_assets_visible_node(const BkEndingAuxiliaryAssets *, unsigned slot);
unsigned bk_ending_auxiliary_assets_missing_visible(const BkEndingAuxiliaryAssets *);
const float *bk_ending_auxiliary_assets_target(const BkEndingAuxiliaryAssets *, unsigned index);
#endif
