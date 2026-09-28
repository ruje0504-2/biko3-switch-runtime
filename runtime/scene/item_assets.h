#ifndef BK_SCENE_ITEM_ASSETS_H
#define BK_SCENE_ITEM_ASSETS_H
#include "game/item.h"
#include "resource/store.h"
#include "world/actor_pose.h"
typedef struct BkItemAssets BkItemAssets;
/*4ec9f0 item XAN/model lifecycle, mounted bk3_16. Owns instances and a copy
 * of retained slot state; input state/inventory are borrowed only at creation.
 * Load authored clocks without selecting or advancing; place root immediately,
 * children wait for global publication. se101 pickup sound is a separate
 * service, not silently played by model loading. */
BkItemAssets *bk_item_assets_create(BkResourceStore *, uint32_t group,
                                    uint32_t area,
                                    const uint8_t collected[BK_ITEM_TYPES],
                                    const BkItemState retained[BK_ITEM_LIMIT],
                                    char error[256]);
void bk_item_assets_destroy(BkItemAssets *);
uint32_t bk_item_assets_count(const BkItemAssets *);
const BkItemState *bk_item_assets_state(const BkItemAssets *, uint32_t index);
const BkActorPose *bk_item_assets_pose(const BkItemAssets *, uint32_t index);
BkActorPose *bk_item_assets_bind_pose(BkItemAssets *, uint32_t index);
/* Logical hidden only;4ef095 applies it during the display phase. Does not
 * modify collected inventory or claim the pickup interaction has run. */
int bk_item_assets_hidden(BkItemAssets *, uint32_t index, uint8_t hidden);
int bk_item_assets_snapshot(const BkItemAssets *, BkItemState[BK_ITEM_LIMIT]);
/*4ef095: hide subtree, request0, advance FULL seconds. Hidden pauses XAN
 * but still receives requests. No world publication. Later-instance failure
 * terminates this frame; prior instances are not rolled back. */
int bk_item_assets_step(BkItemAssets *, float seconds, char error[256]);
/* End-of-simulation pickup stage, AFTER display/animation as51a682.
 * Logical hidden changes immediately, frame hidden waits for next display.
 * Caller supplies real sound and notice services; failures terminate frame. */
int bk_item_assets_pickup(BkItemAssets *, BkItemPickupState *,
                          const float current[3], const float previous[3],
                          const BkItemPickupOps *, BkItemPickups *,
                          char error[256]);
#endif
