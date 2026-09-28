#ifndef BK_GAME_ACTOR_ENTRY_H
#define BK_GAME_ACTOR_ENTRY_H
#include "game/entry.h"
#include "game/item.h"
#include "game/npc_footsteps.h"
#include "game/npc_spatial.h"
#include "game/player_control.h"
#include "world/player_view.h"
/* Actual4befc0 placement, including flow48 overrides. Selection still exposes
 * the normal spawn table, which independently supplies the NPC body height. */
int bk_player_entry_placement(BkActorPlacement *, const BkEntryRequest *);
/* Represented CPU fields of4befc0+4bf1a0. Retain previous/turn/acceleration,
 * latches, script phase, return yaw, wall sensors, action slots7/15 and
 * ordinary inventory. Base head is the loaded, PRE-placement head Y, not
 * body+head. */
int bk_player_entry_reset(BkPlayerControl *, int32_t actions[21],
                          uint8_t collected[BK_ITEM_TYPES],
                          const BkEntryRequest *, float base_head_height);
/*4eb0ee/4bf7db failure reload, preserving body/physics/inventory and the
 * outer saved outcome byte. Original action slots7/15 remain untouched. */
int bk_player_failure_reset(BkPlayerControl *, int32_t actions[21],
                            float base_head_height);
typedef struct {
  uint32_t start,
      end; /* Actor+834/+838, NOT interpolation segment_start/end. */
} BkNpcEntryRoute;
/* Represented CPU fields of4fad20. route_start is retained BF3EA8 profile
 * state supplied by the owner; never inferred from cursor. Reads its flag
 * BEFORE flow48 changes the current point to1. Does not reset hidden, AI
 * stimulus, head caches, run countdown or timers except action_wait.duration.
 * Unwritten footstep walk[1]/run[1] retain their live values. All outputs,
 * including the entry-owned route, remain intact on failure. */
int bk_npc_entry_reset(BkNpcSpatialState *, BkNpcFootstepActions *,
                       BkNpcEntryRoute *, BkRoute *, const BkEntryRequest *,
                       uint32_t route_start);
/* Mode2 controller reset4b7d50..4b7e38, then4ebfd0's NPC smoothing seed.
 * Retains probe/focus/rays/blocked/lean and unrelated cached sensor fields. */
int bk_entry_player_view_reset(BkPlayerView *, const float npc_origin[3]);
#endif
