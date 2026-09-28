#ifndef BK_GAME_NPC_POINT_H
#define BK_GAME_NPC_POINT_H
#include "game/npc_motion.h"
typedef struct {
  BkNpcMotionState motion;
  BkTimer action_wait;
  uint8_t background_wait, fade_out, gate_state;
} BkNpcPointState;
typedef struct {
  uint8_t snap_to_point, play_wait_sound, play_route_sound;
} BkNpcPointEffects;
/* 0x4f5410 handles the last crossed point after geometric movement. Audio
 * calls are explicit commands; the caller must consume them, not discard
 * them and claim complete gameplay. Route sound selection needs group,
 * area, post-movement cursor and actor/player positions (0x4f6226).
 * The run countdown takes priority over the new point's flag. */
int bk_npc_point_apply(BkNpcPointState *state,
                       const BkNpcMotionActions *actions, int32_t run_remaining,
                       int8_t flag, int32_t background_clip,
                       BkNpcPointEffects *effects);
#endif
