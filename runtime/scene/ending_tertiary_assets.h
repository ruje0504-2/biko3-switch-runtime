#ifndef BK_SCENE_ENDING_TERTIARY_ASSETS_H
#define BK_SCENE_ENDING_TERTIARY_ASSETS_H
#include "game/ending_tertiary.h"
#include "game/ending_tertiary_presentation.h"
#include "scene/bom_dual_assets.h"
#include "scene/ending_background_assets.h"
#include "scene/ending_camera_assets.h"
#include "scene/eye_assets.h"
#include "scene/face_assets.h"

typedef struct BkEndingTertiaryAssets BkEndingTertiaryAssets;
/* Stable logical roles, not raw forest indices. Only actual actors are
 * registered: group2 has six after background, all other groups have four.
 * Query registry() when adapting a role to actor_forest/render traversal. */
typedef enum {
  BK_ENDING_TERTIARY_ASSET_PRIMARY,
  BK_ENDING_TERTIARY_ASSET_UPPER,
  BK_ENDING_TERTIARY_ASSET_LOWER,
  BK_ENDING_TERTIARY_ASSET_TRACK0,
  BK_ENDING_TERTIARY_ASSET_TRACK1,
  BK_ENDING_TERTIARY_ASSET_BACKGROUND,
  BK_ENDING_TERTIARY_ASSET_ROLES
} BkEndingTertiaryRole;
/* Presentation actor3 means background; asset role3 is a camera track.
 * Adapt explicitly, never cast the two enums. Invalid input returns ROLES. */
BkEndingTertiaryRole bk_ending_tertiary_assets_role(BkEndingTertiaryActor);

/* Actual CPU resource/pose portion of4d2320: primary/FAM, group2's two
 * auxiliaries and3+2 BOM, placement/eyes/visibility, configured requests,
 * flow16 tracks with camera variant5, node5/13/0 cached targets and fixed
 * camera. Camera/presets/RNG commit only on successful construction.
 * UI/audio/video texture replacement and process-state/controller services
 * remain external. This is not a complete or playable third-ending entry. */
BkEndingTertiaryAssets *bk_ending_tertiary_assets_create(
    BkResourceStore *, unsigned group, unsigned action_variant,
    const uint32_t face_clocks[4], uint32_t *random, BkMenuCamera *,
    BkEndingCameraPresets *, char error[256]);
/*4d2320 never replaces background internally, including group1. Retain the
 * caller's actual background and restore its global order before attaching
 * any new actors. Its source variant can differ after4d00fa's m02_90 load.
 * Old/new snapshots can coexist. Later failure retains applied cache writes. */
BkEndingTertiaryAssets *bk_ending_tertiary_assets_create_reloaded(
    BkResourceStore *, unsigned group, unsigned action_variant,
    BkEndingBackgroundAssets *, const uint32_t face_clocks[4], uint32_t *random,
    BkMenuCamera *, BkEndingCameraPresets *, char error[256]);
void bk_ending_tertiary_assets_destroy(BkEndingTertiaryAssets *);
/* Outer4cc582 fresh background insertion. Cached target vectors remain the
 * pre-insertion snapshots; repeated calls keep the live background state.
 * Failure after forest append makes the owner unusable except destruction. */
int bk_ending_tertiary_assets_load_background(BkEndingTertiaryAssets *,
                                               BkResourceStore *, char error[256]);
BkEndingBackgroundAssets *bk_ending_tertiary_assets_background(const BkEndingTertiaryAssets *);
const BkEndingTertiaryConfig *bk_ending_tertiary_assets_config(const BkEndingTertiaryAssets *);
BkActorForest *bk_ending_tertiary_assets_forest(BkEndingTertiaryAssets *);
uint32_t bk_ending_tertiary_assets_registry(const BkEndingTertiaryAssets *, unsigned role);
BkActorPose *bk_ending_tertiary_assets_pose(BkEndingTertiaryAssets *, unsigned role);
uint32_t bk_ending_tertiary_assets_root(const BkEndingTertiaryAssets *, unsigned role);
const char *bk_ending_tertiary_assets_model_name(const BkEndingTertiaryAssets *, unsigned role);
/* Primary model-level effects and FAM targets are checked disjoint. Both
 * auxiliary effects belong exclusively to bom(); no duplicate live buffers
 * or materials may be created by the renderer. Tracks have no effect owner. */
int bk_ending_tertiary_assets_advance(BkEndingTertiaryAssets *, unsigned role,
                                    float seconds, char error[256]);
/* Controlled primary/auxiliary sampling with actual ANIM/MATA/MORP and the
 * same shared BOM buffers. The independent background keeps its ordinary
 * lifecycle. No world publication or camera-track sampling. */
int bk_ending_tertiary_assets_advance_plain(BkEndingTertiaryAssets *, unsigned role,
                                           float seconds, BkClipPlainMode,
                                           char error[256]);
BkMaterialPose *bk_ending_tertiary_assets_materials(BkEndingTertiaryAssets *, unsigned role);
/*4a7d10 on the actual model material registry. Empty/missing names are the
 * original no-op. First matching material wins; a retained background was
 * registered before replacement actors, a fresh background after them.
 * Only diffuse alpha changes, using the same materials borrowed by render. */
int bk_ending_tertiary_assets_material_alpha(BkEndingTertiaryAssets *,
                                             const char *name, uint32_t hidden,
                                             float alpha, char error[256]);
const BkMaterialAnimation *bk_ending_tertiary_assets_material_animation(
    const BkEndingTertiaryAssets *, unsigned role);
const BkMorphGroup *bk_ending_tertiary_assets_morph(const BkEndingTertiaryAssets *, unsigned role);
const BkMorphMesh *bk_ending_tertiary_assets_mesh(const BkEndingTertiaryAssets *,
                                                 unsigned role, uint32_t submesh);
BkBomDualAssets *bk_ending_tertiary_assets_bom(BkEndingTertiaryAssets *);
BkFaceAssets *bk_ending_tertiary_assets_face(BkEndingTertiaryAssets *);
BkFaceState *bk_ending_tertiary_assets_face_state(BkEndingTertiaryAssets *);
BkEyeAssets *bk_ending_tertiary_assets_eyes(BkEndingTertiaryAssets *);
BkEndingCameraAssets *bk_ending_tertiary_assets_cameras(BkEndingTertiaryAssets *);
/* Node getters return primary frame indices. special() corresponds719b48,
 * not secondary's719b44. Missing optional targets do not clear a different
 * stage's retained process fields. */
uint32_t bk_ending_tertiary_assets_node(const BkEndingTertiaryAssets *, unsigned index);
uint32_t bk_ending_tertiary_assets_follow(const BkEndingTertiaryAssets *);
uint32_t bk_ending_tertiary_assets_special(const BkEndingTertiaryAssets *);
uint32_t bk_ending_tertiary_assets_visible_node(const BkEndingTertiaryAssets *, unsigned slot);
unsigned bk_ending_tertiary_assets_missing_visible(const BkEndingTertiaryAssets *);
const float *bk_ending_tertiary_assets_target(const BkEndingTertiaryAssets *, unsigned index);
#endif
