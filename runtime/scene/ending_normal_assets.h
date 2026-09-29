#ifndef BK_SCENE_ENDING_NORMAL_ASSETS_H
#define BK_SCENE_ENDING_NORMAL_ASSETS_H
#include "scene/ending_background_assets.h"
#include "game/ending_normal.h"
#include "scene/bom_assets.h"
#include "scene/ending_camera_assets.h"
#include "scene/eye_assets.h"
#include "scene/face_assets.h"
typedef struct BkEndingNormalAssets BkEndingNormalAssets;
/* Actual registered root of actor0/1/4; camera tracks use their own API.
 * Returns BK_FRAME_NONE for an absent actor, not a fabricated root. */
uint32_t bk_ending_normal_assets_root(const BkEndingNormalAssets *, unsigned actor);
/* CPU resource/pose portion of normal4cf318, packaged retail layout.
 * Own primary/auxiliary, face/eyes/BOM, two camera tracks, optional group1
 * background, and their forest. Actual insertion order matters: primary,
 * auxiliary, BOM alignment/effects, tracks, initial fixed camera, background.
 * variant is the already normalized0/1 value. The retained camera is input
 * and output; clocks are the four actual face initialization reads. RNG,
 * camera and presets commit only when construction succeeds.
 * This is not the complete loader: video texture, shared process flags,
 * lighting/device state, audio/UI and application flow remain separate. */
BkEndingNormalAssets *
bk_ending_normal_assets_create(BkResourceStore *, unsigned group,
                               unsigned variant, const uint32_t face_clocks[4],
                               uint32_t *random, BkMenuCamera *,
                               BkEndingCameraPresets *, char error[256]);
void bk_ending_normal_assets_destroy(BkEndingNormalAssets *);
/* Stage replacement keeps the existing outer background for groups0/2/3/4.
 * Group1 actually invokes4d460b and loads m02_92. Restore the old background
 * in global insertion order before new actors; do not reselect its clip or
 * reset materials, visibility or playback. The old GPU owner may coexist.
 * A later construction failure retains executed background cache updates. */
BkEndingNormalAssets *bk_ending_normal_assets_create_reloaded(
    BkResourceStore *, unsigned group, unsigned variant,
    BkEndingBackgroundAssets *, const uint32_t clocks[4], uint32_t *random,
    BkMenuCamera *, BkEndingCameraPresets *, char error[256]);
BkEndingBackgroundAssets *bk_ending_normal_assets_background(
    const BkEndingNormalAssets *);
/* CPU portion of outer4cc582/4d460b, after initial camera/UI construction.
 * Keep group1's already loaded special background; otherwise append the
 * selected common background, globally attach/orient, then request slot0.
 * Idempotent after success. Resource/registration failure leaves it retryable;
 * failure after registration terminates this owner (destroy/reload required).
 * No camera target recapture or device fog change is implied here. */
int bk_ending_normal_assets_load_background(BkEndingNormalAssets *,
                                            BkResourceStore *, char error[256]);
const BkEndingNormalConfig *
bk_ending_normal_assets_config(const BkEndingNormalAssets *);
BkActorForest *bk_ending_normal_assets_forest(BkEndingNormalAssets *);
/* actor0=primary,1=auxiliary,2/3=tracks,4=background (NULL until loaded). */
BkActorPose *bk_ending_normal_assets_pose(BkEndingNormalAssets *,
                                          unsigned actor);
const char *bk_ending_normal_assets_model_name(const BkEndingNormalAssets *,
                                               unsigned actor);
BkBomAssets *bk_ending_normal_assets_bom(BkEndingNormalAssets *);
BkFaceAssets *bk_ending_normal_assets_face(BkEndingNormalAssets *);
BkFaceState *bk_ending_normal_assets_face_state(BkEndingNormalAssets *);
BkEyeAssets *bk_ending_normal_assets_eyes(BkEndingNormalAssets *);
BkEndingCameraAssets *bk_ending_normal_assets_cameras(BkEndingNormalAssets *);
/* Frame indices in primary; optional named nodes may be MODEL_NONE.
 * OYU is looked up only for group2: this result must not be used to clear
 * a caller's retained719b44 in other groups. Targets are the three cached
 * positions captured before background insertion, never live pointers. */
uint32_t bk_ending_normal_assets_node(const BkEndingNormalAssets *,
                                      unsigned index);
uint32_t bk_ending_normal_assets_oyu(const BkEndingNormalAssets *);
const float *bk_ending_normal_assets_target(const BkEndingNormalAssets *,
                                            unsigned index);
#endif
