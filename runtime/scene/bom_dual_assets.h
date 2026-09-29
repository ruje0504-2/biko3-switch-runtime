#ifndef BK_SCENE_BOM_DUAL_ASSETS_H
#define BK_SCENE_BOM_DUAL_ASSETS_H
#include "scene/bom_assets.h"

typedef struct BkBomDualAssets BkBomDualAssets;
typedef struct {
  BkBomAssetBinding nodes;
  /* Node indices alone are insufficient: each lookup independently tries
   * auxiliary1 before auxiliary2. Missing optional nodes have actor NONE. */
  uint32_t child_actor, secondary_aux_actor;
} BkBomDualBinding;

/*4a52bc: borrow primary0 and up to two actual auxiliary actors1/2. Own one
 * material/MATA/MORP instance per auxiliary, all VIX and the combined mapping.
 * Metadata never chooses the actors. All nonnull roots must be attached to
 * global. Align every child, refresh global, rebuild mapping using LIVE mesh
 * vertices, then request0/fixed-step/hide each auxiliary in argument order.
 * No publication follows the fixed steps. Destroy before the borrowed poses
 * and model-name strings, which remain live for the later registry rebuild.
 * This CPU owner does not claim complete4d2320, rendering or playable entry. */
BkBomDualAssets *bk_bom_dual_assets_create(
    BkResourceStore *, const char *selection_pack, const BkBomConfig *,
    const BkBomActor actors[3], BkActorForest *, char error[256]);
/*4a65fc followed by another complete4a52bc pass. Prior rows/effect instances
 * survive; all mappings are rebuilt. Resource preflight failure preserves the
 * owner. Failure after the first pose mutation requires destroying the owner;
 * its applied native prefix is not rolled back. No silent partial success. */
int bk_bom_dual_assets_append(BkBomDualAssets *, BkResourceStore *,
                              const char *selection_pack, const BkBomConfig *,
                              BkActorForest *, char error[256]);
void bk_bom_dual_assets_destroy(BkBomDualAssets *);
uint32_t bk_bom_dual_assets_count(const BkBomDualAssets *);
uint32_t bk_bom_dual_assets_mode(const BkBomDualAssets *);
const BkBomDualBinding *bk_bom_dual_assets_binding(const BkBomDualAssets *, uint32_t);
uint32_t bk_bom_dual_assets_mesh_count(const BkBomDualAssets *);
const BkBomAssetMesh *bk_bom_dual_assets_mesh(const BkBomDualAssets *, uint32_t);
int bk_bom_dual_assets_mapping(const BkBomDualAssets *, uint32_t, int32_t *,
                               const uint32_t **, size_t *);
int bk_bom_dual_assets_plan(const BkBomDualAssets *, uint32_t, BkBomDeformBinding *);
BkActorPose *bk_bom_dual_assets_actor(const BkBomDualAssets *, unsigned actor);
BkMaterialPose *bk_bom_dual_assets_materials(const BkBomDualAssets *, unsigned actor);
const BkMaterialAnimation *bk_bom_dual_assets_material_animation(const BkBomDualAssets *, unsigned actor);
const BkMorphGroup *bk_bom_dual_assets_morph(const BkBomDualAssets *, unsigned actor);
/* Explicit later4021a1/4026fe, using these same effects. Hidden-root guards
 * and component failure prefixes follow the ordinary actor functions. */
int bk_bom_dual_assets_advance_frame(BkBomDualAssets *, unsigned actor, char error[256]);
int bk_bom_dual_assets_advance(BkBomDualAssets *, unsigned actor, float seconds,
                              char error[256]);
/*402e18/4e18ad/4a9019 with the same auxiliary ANIM/MATA/MORP ownership.
 * Does not create another effect instance or publish cached worlds. */
int bk_bom_dual_assets_advance_plain(BkBomDualAssets *, unsigned actor,
                                     float seconds, BkClipPlainMode,
                                     char error[256]);
#endif
