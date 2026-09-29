#ifndef BK_SCENE_ENDING_SELECTED_ASSETS_H
#define BK_SCENE_ENDING_SELECTED_ASSETS_H
#include "game/ending_selected.h"
#include "scene/ending_background_assets.h"
#include "scene/ending_camera_assets.h"
#include "scene/eye_assets.h"
#include "scene/face_assets.h"
typedef struct BkEndingSelectedAssets BkEndingSelectedAssets;
typedef struct {
  unsigned group, variant, selection;
  int32_t selected; /*live721ed8 before4D1C64*/
  const char *primary_path; /*live model_paths[config.event], copied on load*/
  BkEndingBackgroundAssets *background; /*optional actual retained owner*/
} BkEndingSelectedLoad;
/*CPU asset portion of4D1025/4D1D22, packaged retail branch. Independent
 *primary0, tracks1/2 and optional background3; no auxiliary/BOM actor.
 *The retained background precedes fresh actors in the global registry.
 *After face/eye initialization and configured request1, actual4026fe(30.f)
 *runs ANIM/MATA/MORP before reading the OLD cached target5/13/0 matrices.
 *Group1 replacement and group0 material hiding follow the original tail.
 *Caller camera/presets/RNG commit only on success; retained model cache or
 *material writes before a later failure keep their executed prefix.
 *UI/audio/video/process aliases and phase5/6 controllers are separate;
 *this constructor alone does not register a playable scene. */
BkEndingSelectedAssets *bk_ending_selected_assets_create(
    BkResourceStore *, const BkEndingSelectedLoad *, const uint32_t clocks[4],
    uint32_t *random, BkMenuCamera *, BkEndingCameraPresets *, char error[256]);
void bk_ending_selected_assets_destroy(BkEndingSelectedAssets *);
/*Outer4CC582 background stage after a fresh load. No reinitialization of an
 *already retained/replaced background. A failed append is terminal. */
int bk_ending_selected_assets_load_background(BkEndingSelectedAssets *,
                                               BkResourceStore *, char error[256]);
const BkEndingSelectedConfig *bk_ending_selected_assets_config(const BkEndingSelectedAssets *);
int bk_ending_selected_assets_replaced_background(const BkEndingSelectedAssets *);
int bk_ending_selected_assets_background_first(const BkEndingSelectedAssets *);
BkEndingBackgroundAssets *bk_ending_selected_assets_background(const BkEndingSelectedAssets *);
BkActorForest *bk_ending_selected_assets_forest(BkEndingSelectedAssets *);
BkActorPose *bk_ending_selected_assets_pose(BkEndingSelectedAssets *, unsigned actor);
uint32_t bk_ending_selected_assets_root(const BkEndingSelectedAssets *, unsigned actor);
const char *bk_ending_selected_assets_model_name(const BkEndingSelectedAssets *, unsigned actor);
int bk_ending_selected_assets_advance(BkEndingSelectedAssets *, unsigned actor,
                                      float seconds, char error[256]);
/*Primary-only402e18/4e18ad/4a9019. Real shared ANIM/MATA/MORP sampling,
 *including held plain caches across blending; no world publication.*/
int bk_ending_selected_assets_advance_plain(BkEndingSelectedAssets *,
                                             float seconds, BkClipPlainMode,
                                             char error[256]);
/*495469/4952C8 against the actual primary's captured active descriptor.
 * Only its source is edited. Elapsed/requests, locals, cached worlds,
 * ANIM/MATA/MORP caches and background are preserved until a later explicit
 * advance/publish. Coordinates are the caller's existing projected UI words;
 * drag components are floats and receive no additional time multiplier. */
int bk_ending_selected_assets_pointer(BkEndingSelectedAssets *,
                                       const int32_t target[2],
                                       const int32_t menu[2],
                                       const int32_t pointer[2], char error[256]);
int bk_ending_selected_assets_drag(BkEndingSelectedAssets *,
                                    int32_t plain_scheduled, int32_t reverse,
                                    const float motion[2], char error[256]);
BkMaterialPose *bk_ending_selected_assets_materials(BkEndingSelectedAssets *, unsigned actor);
const BkMaterialAnimation *bk_ending_selected_assets_material_animation(
    const BkEndingSelectedAssets *, unsigned actor);
const BkMorphGroup *bk_ending_selected_assets_morph(const BkEndingSelectedAssets *, unsigned actor);
const BkMorphMesh *bk_ending_selected_assets_mesh(const BkEndingSelectedAssets *,
                                                  unsigned actor, uint32_t submesh);
/*4A7D10 resolves the first exact name in actual registration order. Empty
 *or absent names are native no-ops; hidden forcesalpha0, even foralpha-1. */
int bk_ending_selected_assets_material_alpha(BkEndingSelectedAssets *, const char *name,
                                              uint32_t hidden, float alpha, char error[256]);
BkFaceAssets *bk_ending_selected_assets_face(BkEndingSelectedAssets *);
BkFaceState *bk_ending_selected_assets_face_state(BkEndingSelectedAssets *);
BkEyeAssets *bk_ending_selected_assets_eyes(BkEndingSelectedAssets *);
BkEndingCameraAssets *bk_ending_selected_assets_cameras(BkEndingSelectedAssets *);
uint32_t bk_ending_selected_assets_node(const BkEndingSelectedAssets *, unsigned index);
uint32_t bk_ending_selected_assets_anchor(const BkEndingSelectedAssets *);
uint32_t bk_ending_selected_assets_follow(const BkEndingSelectedAssets *);
/*Only group2 sets one of719B44/OYU or719B48/cris_baiza_A_Layer1.
 *Absent nodes do not authorize clearing another retained process slot. */
uint32_t bk_ending_selected_assets_special(const BkEndingSelectedAssets *);
uint32_t bk_ending_selected_assets_visible_node(const BkEndingSelectedAssets *, unsigned slot);
unsigned bk_ending_selected_assets_missing_visible(const BkEndingSelectedAssets *);
const float *bk_ending_selected_assets_target(const BkEndingSelectedAssets *, unsigned index);
#endif
