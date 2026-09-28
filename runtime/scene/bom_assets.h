#ifndef BK_SCENE_BOM_ASSETS_H
#define BK_SCENE_BOM_ASSETS_H
#include "model/bom_deform.h"
#include "model/material_animation.h"
#include "model/morph_group.h"
#include "resource/bom.h"
#include "resource/store.h"
#include "world/actor_forest.h"
#include "world/bom_motion.h"
typedef struct BkBomAssets BkBomAssets;
typedef struct {
  BkActorPose *pose;
  const char *model_name;
  uint32_t forest_actor, root;
} BkBomActor;
typedef struct {
  uint32_t parent, reference, child, primary_aux, secondary_aux;
  uint32_t source, target, selection_count, missing_selection;
} BkBomAssetBinding;
typedef struct {
  uint32_t actor, submesh, frame; /* actor0=primary, actor1=secondary */
} BkBomAssetMesh;
/*4a4dc3: borrow the caller's actual loaded actors (BOM metadata is not a
 * selection policy). Both roots must already be attached to the given forest.
 * Own VIX/mapping, secondary MATA/material/MORP instances and binding state.
 * Align children -> global refresh -> map BASE mesh vertices -> request0 ->
 * fixed ANIM/MATA/MORP step -> hide secondary. No post-step publication.
 * Missing VIX is native count0; corrupt/read-error resources fail. Unknown
 * secondary effect chunks fail explicitly. Resource preflight is read-only;
 * a later failure aborts loading and may leave an applied pose prefix.
 * Destroy borrowers before the two actors; destroy never rewrites their pose.
 * No GPU callback, per-frame physics or complete ending loader is implied. */
BkBomAssets *bk_bom_assets_create(BkResourceStore *, const char *selection_pack,
                                  const BkBomConfig *,
                                  const BkBomActor actors[2], BkActorForest *,
                                  char error[256]);
void bk_bom_assets_destroy(BkBomAssets *);
BkActorPose *bk_bom_assets_actor(const BkBomAssets *, unsigned actor);
uint32_t bk_bom_assets_count(const BkBomAssets *);
const BkBomAssetBinding *bk_bom_assets_binding(const BkBomAssets *, uint32_t);
uint32_t bk_bom_assets_mesh_count(const BkBomAssets *);
const BkBomAssetMesh *bk_bom_assets_mesh(const BkBomAssets *, uint32_t);
const BkMaterialPose *bk_bom_assets_materials(const BkBomAssets *);
const BkMorphGroup *bk_bom_assets_morph(const BkBomAssets *);
int bk_bom_assets_mapping(const BkBomAssets *, uint32_t binding, int32_t *group,
                          const uint32_t **sources, size_t *count);
int bk_bom_assets_plan(const BkBomAssets *, uint32_t binding,
                       BkBomDeformBinding *out);
/* Later explicit4021a1 calls use the same owned effect instances. Hidden root
 * pauses all three; no requests, visibility changes or publication implicit.
 * Components commit in native order; downstream errors abort the frame. */
int bk_bom_assets_advance_frame(BkBomAssets *, char error[256]);
/*4026fe: ANIM/optional MORP blend -> MATA at actual source -> optional plain
 * MORP. Hidden root pauses; no implicit request or world publication. A late
 * effect failure retains previous stages and must abort the caller's frame. */
int bk_bom_assets_advance(BkBomAssets *, float seconds, char error[256]);
/*49aa50/49ad16 actual binding parent/reference nodes in the PRIMARY actor.
 * State is caller-owned process state, retained across actor reloads. The
 * direct49afde pair is external to BOM and uses world/bom_motion directly. */
int bk_bom_assets_manual(BkBomAssets *, BkBomManual *, BkBomManualKind,
                         unsigned binding, int32_t dx, int32_t dy, float radius,
                         float degrees, int32_t flip, char error[256]);
int bk_bom_assets_return_single(BkBomAssets *, BkBomReturn *, unsigned binding,
                                const float scene_world[16], float degrees,
                                int32_t flip, uint32_t ms, int32_t reset,
                                int *done, char error[256]);
/*49b900 processes first count bindings (actual caller uses2); per-node prefix
 * side effects and cross-reference/duplicate identities are preserved. */
int bk_bom_assets_return_multiple(BkBomAssets *, BkBomReturn *, unsigned count,
                                  const float scene_world[16], float degrees,
                                  int32_t flip, uint32_t ms, int *done,
                                  char error[256]);
/*4dfa62..4dfad5: align each auxiliary child to its PRIMARY reference node,
 * unlike initial binder alignment to the parent node. No implicit refresh. */
int bk_bom_assets_follow_references(BkBomAssets *, char error[256]);
#endif
