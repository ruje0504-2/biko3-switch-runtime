#ifndef BK_SCENE_ENDING_SECONDARY_ASSETS_H
#define BK_SCENE_ENDING_SECONDARY_ASSETS_H
#include "scene/ending_background_assets.h"
#include "game/ending_secondary.h"
#include "model/material_animation.h"
#include "model/morph_group.h"
#include "scene/ending_camera_assets.h"
#include "scene/eye_assets.h"
#include "scene/face_assets.h"
typedef struct BkEndingSecondaryAssets BkEndingSecondaryAssets;
/* CPU resource/pose portion of4D00FA, packaged retail branch only.
 * Forest actors are primary0, camera tracks1/2, optional background3.
 * No auxiliary or BOM owner exists in this topology. Camera variant is1;
 * background_variant is the separate outer action-table/background choice.
 * Face/RNG initialization, root placement, named visibility, configured
 * request1, flow16 tracks, cached targets, fixed camera and group1 background
 * keep their original order. Outputs commit only after successful creation.
 * Process flags, UI/audio/video and phase2 controllers remain separate; this
 * component alone must never be exposed as a complete playable entry. */
BkEndingSecondaryAssets *bk_ending_secondary_assets_create(
    BkResourceStore *, unsigned group, unsigned background_variant,
    const uint32_t face_clocks[4], uint32_t *random, BkMenuCamera *,
    BkEndingCameraPresets *, char error[256]);
void bk_ending_secondary_assets_destroy(BkEndingSecondaryAssets *);
/*4D00FA stage replacement: retain the live outer background except group1,
 * whose4d460b explicitly replaces it with m02_90. Restore its global order
 * before loading new actors, without clip selection or effect reset. Old
 * and new stage snapshots may coexist. Later failure retains cache writes. */
BkEndingSecondaryAssets *bk_ending_secondary_assets_create_reloaded(
    BkResourceStore *, unsigned group, unsigned background_variant,
    BkEndingBackgroundAssets *, const uint32_t clocks[4], uint32_t *random,
    BkMenuCamera *, BkEndingCameraPresets *, char error[256]);
BkEndingBackgroundAssets *bk_ending_secondary_assets_background(
    const BkEndingSecondaryAssets *);
/* Outer4CC582 background stage; group1 retains its own m02_90. A failure
 * after registration makes the owner unusable except for destruction. */
int bk_ending_secondary_assets_load_background(BkEndingSecondaryAssets *,
                                                BkResourceStore *, char error[256]);
const BkEndingSecondaryConfig *
bk_ending_secondary_assets_config(const BkEndingSecondaryAssets *);
BkActorForest *bk_ending_secondary_assets_forest(BkEndingSecondaryAssets *);
BkActorPose *bk_ending_secondary_assets_pose(BkEndingSecondaryAssets *, unsigned actor);
/* Primary0/background3 own their material instances and model-level effects.
 * Advance follows4026fe: ANIM, blend MORP, MATA at actual source, then plain
 * MORP when not blending. Hidden roots retain all effect clocks. No world
 * publication or face controller runs here. Later failure retains its prefix.
 * Renderers borrow these exact materials/vertices; no parallel material copy. */
int bk_ending_secondary_assets_advance(BkEndingSecondaryAssets *, unsigned actor,
                                      float seconds, char error[256]);
BkMaterialPose *bk_ending_secondary_assets_materials(BkEndingSecondaryAssets *, unsigned actor);
const BkMaterialAnimation *bk_ending_secondary_assets_material_animation(
    const BkEndingSecondaryAssets *, unsigned actor);
const BkMorphGroup *bk_ending_secondary_assets_morph(const BkEndingSecondaryAssets *, unsigned actor);
const BkMorphMesh *bk_ending_secondary_assets_mesh(const BkEndingSecondaryAssets *,
                                                 unsigned actor, uint32_t submesh);
uint32_t bk_ending_secondary_assets_root(const BkEndingSecondaryAssets *, unsigned actor);
const char *bk_ending_secondary_assets_model_name(const BkEndingSecondaryAssets *, unsigned actor);
BkFaceAssets *bk_ending_secondary_assets_face(BkEndingSecondaryAssets *);
BkFaceState *bk_ending_secondary_assets_face_state(BkEndingSecondaryAssets *);
BkEyeAssets *bk_ending_secondary_assets_eyes(BkEndingSecondaryAssets *);
BkEndingCameraAssets *bk_ending_secondary_assets_cameras(BkEndingSecondaryAssets *);
/* Frame indices in primary. Missing optional nodes return MODEL_NONE.
 * An absent group-specific OYU must not clear another owner's retained slot.
 * Target vectors are captured before background insertion, not live views. */
uint32_t bk_ending_secondary_assets_node(const BkEndingSecondaryAssets *, unsigned index);
uint32_t bk_ending_secondary_assets_anchor(const BkEndingSecondaryAssets *);
uint32_t bk_ending_secondary_assets_follow(const BkEndingSecondaryAssets *);
uint32_t bk_ending_secondary_assets_oyu(const BkEndingSecondaryAssets *);
uint32_t bk_ending_secondary_assets_visible_node(const BkEndingSecondaryAssets *, unsigned slot);
unsigned bk_ending_secondary_assets_missing_visible(const BkEndingSecondaryAssets *);
const float *bk_ending_secondary_assets_target(const BkEndingSecondaryAssets *, unsigned index);
#endif
