#ifndef BK_SCENE_DIALOGUE_WORLD_H
#define BK_SCENE_DIALOGUE_WORLD_H
#include "scene/dialogue_actor_assets.h"
#include "scene/lighting_assets.h"
#include "world/actor_forest.h"
#include "world/menu_camera.h"
typedef struct BkDialogueWorld BkDialogueWorld;
/* Owns one real dialogue body, original light registry and global topology.
 * Camera/RNG borrowed; constructor preserves both on failure. Phase/scalar
 * initialization belongs to the enclosing retained session. */
BkDialogueWorld *bk_dialogue_world_create(BkResourceStore *, unsigned group,
                                          BkMenuCamera *,
                                          const uint32_t clocks[4],
                                          uint32_t *random, char error[256]);
void bk_dialogue_world_destroy(BkDialogueWorld *);
BkDialogueActorAssets *bk_dialogue_world_body(BkDialogueWorld *);
BkActorForest *bk_dialogue_world_forest(BkDialogueWorld *);
BkSceneLighting *bk_dialogue_world_lighting(BkDialogueWorld *);
uint32_t bk_dialogue_world_root(const BkDialogueWorld *);
/* Install4bd641 camera before4f11f2. No final actor-cache publication. */
int bk_dialogue_world_step(BkDialogueWorld *, BkDialogueActorState *,
                           BkDialogue *, uint8_t *phase, BkTimer *,
                           const BkDialogueActorInput *, uint32_t *random,
                           char error[256]);
/*51c736 flow8 ->4a4701 mode1, body in root slot4. */
int bk_dialogue_world_pass(BkDialogueWorld *, BkLightingPass *,
                           char error[256]);
#endif
