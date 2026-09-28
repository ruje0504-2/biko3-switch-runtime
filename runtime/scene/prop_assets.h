#ifndef BK_SCENE_PROP_ASSETS_H
#define BK_SCENE_PROP_ASSETS_H
#include "game/npc_scene.h"
#include "game/prop_config.h"
#include "game/prop_interaction.h"
#include "game/prop_motion.h"
#include "model/material_pose.h"
#include "resource/store.h"
#include "world/actor_pose.h"
typedef struct BkPropAssets BkPropAssets;
/* Owns up to16 independent model/XAN/route/pose/material instances. Store
 * must have bk3_07 and routes mounted. At(0,5), the supplied background world
 * is the already-published cache used by51464a. Inputs are borrowed only for
 * construction. No sound initialization, animation or child publication. */
BkPropAssets *bk_prop_assets_create(BkResourceStore *, uint32_t group,
                                    uint32_t area, const BkModel *background,
                                    const float *background_world,
                                    size_t world_floats, char error[256]);
void bk_prop_assets_destroy(BkPropAssets *);
uint32_t bk_prop_assets_count(const BkPropAssets *);
const BkPropState *bk_prop_assets_state(const BkPropAssets *, uint32_t index);
const BkActorPose *bk_prop_assets_pose(const BkPropAssets *, uint32_t index);
/* Borrow for global publication; owner retains animation/lifetime. */
BkActorPose *bk_prop_assets_bind_pose(BkPropAssets *, uint32_t index);
/* Borrow the live CPU motion/contact state and cached ROOT world for4f4306.
 * Supplies no sound owner: the composing audio service sets out->sound.
 * Fresh contact timers are zero; reload orchestration must retain the old
 * +854 timer through this binding. Initialization clears car_hit (+330..337).
 * Mutations do not change already submitted pose/presentation this frame. */
int bk_prop_assets_bind_interaction(BkPropAssets *, uint32_t index,
                                    BkPropInteractionActor *out);
const BkMaterialPose *bk_prop_assets_materials(const BkPropAssets *,
                                               uint32_t index);
const BkRoute *bk_prop_assets_route(const BkPropAssets *, uint32_t index);
/* Append previous published pose geometry; caller pairs end_props after the
 * complete game frame. Does not publish, move or advance any instance. */
int bk_prop_assets_collision(const BkPropAssets *, BkCollision *,
                             char error[256]);
/*5121ce through placement, BEFORE its512c0e sound update. Call ground inputs
 * for every instance when5149e0 enables them; those cached sight fields are
 * separate from current prop location. Per-instance material/pose/state
 * commits happen in original order; later failure terminates the frame. */
int bk_prop_assets_step_spatial(BkPropAssets *, BkPropShared *,
                                const BkPropMotionInput *, const BkCollision *,
                                const BkNpcSceneInput *ground_inputs,
                                size_t ground_count,
                                BkPropMotionEffects effects[16],
                                char error[256]);
/* Complete5121ce composition boundary: callback runs after each placed prop,
 * before the next prop. NULL selects spatial-only dependency mode. */
typedef int (*BkPropPlacedConsumer)(void *, uint32_t index, const BkPropState *,
                                    char error[256]);
int bk_prop_assets_step_spatial_consume(
    BkPropAssets *, BkPropShared *, const BkPropMotionInput *,
    const BkCollision *, const BkNpcSceneInput *, size_t ground_count,
    BkPropMotionEffects effects[16], BkPropPlacedConsumer, void *context,
    char error[256]);
/*512102 visibility -> request(action) -> half-speed animation, no publish. */
int bk_prop_assets_step_presentation(BkPropAssets *, float seconds,
                                     char error[256]);
void bk_prop_assets_publish(BkPropAssets *);
/* Native4b0de5 or4b124d rule groups. No write when material==-2. */
int bk_prop_material_apply(BkMaterialPose *, uint32_t root,
                           const BkPropMotionEffects *, char error[256]);
#endif
