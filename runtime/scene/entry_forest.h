#ifndef BK_SCENE_ENTRY_FOREST_H
#define BK_SCENE_ENTRY_FOREST_H
#include "scene/background_assets.h"
#include "scene/entry_assets.h"
#include "scene/item_assets.h"
#include "scene/prop_assets.h"
#include "world/actor_forest.h"
typedef struct BkEntryForest BkEntryForest;
typedef enum {
  BK_ENTRY_TREE_PLAYER,
  BK_ENTRY_TREE_PLAYER_SHADOW,
  BK_ENTRY_TREE_NPC,
  BK_ENTRY_TREE_NPC_SHADOW,
  BK_ENTRY_TREE_TRACK,
  BK_ENTRY_TREE_BACKGROUND,
  BK_ENTRY_TREE_DOOR,
  BK_ENTRY_TREE_SNOW,
  BK_ENTRY_TREE_PROP,
  BK_ENTRY_TREE_ITEM
} BkEntryTreeKind;
typedef struct {
  BkEntryTreeKind kind;
  uint32_t index, root;
  BkActorPose *pose;
} BkEntryTreeObject;
/* Bind existing flow2 owners, all of which must outlive this forest. Contains
 * the implemented player/NPC/mesh shadows, camera track, background/door/snow
 * and up to16 route props/items each. Does not load/advance anything. Owners
 * must refer to the same entry; add optional shadows before construction, not
 * after. Root order follows4e82b8's4bf1a0,4fad20,4b79e0(mode2),4f6bb0,511940;
 * snow is reparented beneath active camera1 as4fa9de, followed by4ec9f0 items.
 * This binds dependencies without creating a playable mission state.
 * Attachments refresh borrowed caches during construction; a later failure
 * terminates assembly without rolling back prior cache publications. */
BkEntryForest *bk_entry_forest_create(BkEntryAssets *, BkBackgroundAssets *,
                                      BkPropAssets *, BkItemAssets *,
                                      char error[256]);
/* After4bfdd6/4bf7db, newly allocated player/shadow roots follow retained
 * NPC/track/background/props/items, preserving native global traversal order.
 */
BkEntryForest *bk_entry_forest_create_failure(BkEntryAssets *,
                                              BkBackgroundAssets *,
                                              BkPropAssets *, BkItemAssets *,
                                              char error[256]);
void bk_entry_forest_destroy(BkEntryForest *);
uint32_t bk_entry_forest_count(const BkEntryForest *);
/* Object indices match actor_forest_binding's actor index. */
const BkEntryTreeObject *bk_entry_forest_object(const BkEntryForest *,
                                                uint32_t);
uint32_t bk_entry_forest_root(const BkEntryForest *, BkEntryTreeKind,
                              uint32_t index);
BkActorForest *bk_entry_forest_frames(BkEntryForest *);
/* Install the entry controller's current active camera, then publish exactly
 * the requested native walk. Animation and missing roots are never invented.
 * Target0 means global. Borrowed visits map via actor_forest_binding above. */
int bk_entry_forest_draw(BkEntryForest *, uint32_t target,
                         const BkFrameVisit **, uint32_t *count,
                         char error[256]);
#endif
