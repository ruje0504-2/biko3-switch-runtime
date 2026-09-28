#ifndef BK_GAME_ITEM_H
#define BK_GAME_ITEM_H
#include <stdint.h>
#define BK_ITEM_LIMIT 16
#define BK_ITEM_TYPES 5
typedef struct {
  uint32_t id;
  const char *clip;
  float position[3];
} BkItemConfig;
typedef struct {
  uint32_t id;
  float position[3], yaw;
  uint8_t hidden;
} BkItemState;
/*4ec9f0's45 profile tables;4ef168 writes the configured positions. No
 * resource loading or inventory mutation. Current profiles use0..2 slots. */
int bk_item_config(const BkItemConfig **out, uint32_t *count, uint32_t group,
                   uint32_t area);
/* Initialize active slots in-place. Preserve yaw and inactive slots; hidden
 * changes only for inventory byte exactly0 or1, otherwise retain prior value.
 * Reject nonfinite active yaw, invalid profile/arguments atomically. */
int bk_item_initialize(BkItemState slots[BK_ITEM_LIMIT], uint32_t *count,
                       uint32_t group, uint32_t area,
                       const uint8_t collected[BK_ITEM_TYPES]);
typedef struct {
  uint8_t collected[BK_ITEM_TYPES], notice_visible, notice_timer_armed;
  int32_t selected_item, message_cursor;
} BkItemPickupState;
typedef struct {
  uint32_t count, slots[BK_ITEM_LIMIT];
} BkItemPickups;
typedef struct {
  void *context;
  int (*play_sound)(void *, char error[256]);
  int (*load_notice)(void *, uint32_t message_id, char error[256]);
} BkItemPickupOps;
/*4f5c6a scans ALL16 present slots, ignoring hidden ones. Project item onto
 * current->previous player segment in XZ, radius10, without endpoint caps.
 * Height/yaw do not gate a hit; zero horizontal motion misses. Planning is
 * atomic. Invalid finite geometry/IDs preserve output. */
int bk_item_pickup_plan(const BkItemState slots[BK_ITEM_LIMIT],
                        uint32_t present_mask, const float current[3],
                        const float previous[3], BkItemPickups *);
/* Per hit in slot order: logical hide -> sound -> inventory/notice flags ->
 * load message(group*10000+item) -> selected item/cursor0. Multiple hits are
 * retained. Required services are preflighted; a later service failure leaves
 * the preceding native writes intact and terminates this frame. Callbacks
 * must not mutate these states; no animation or frame visibility publication.
 */
int bk_item_pickup_run(BkItemState slots[BK_ITEM_LIMIT], uint32_t present_mask,
                       BkItemPickupState *, uint32_t group,
                       const float current[3], const float previous[3],
                       const BkItemPickupOps *, BkItemPickups *,
                       char error[256]);
#endif
