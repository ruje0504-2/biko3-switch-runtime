#ifndef BK_GAME_NPC_ACTION_H
#define BK_GAME_NPC_ACTION_H
#include "game/npc_point.h"
/* Original 0x4fd3d2..0x4fd5ed action-completion tail, reached only after
 * the earlier AI gates/reactions. Caller supplies current active clip
 * from playback state and a frame clock. Do not call unconditionally instead of AI. */
int bk_npc_action_finish(BkNpcPointState *state,
                         const BkNpcMotionActions *actions, int32_t active_clip,
                         uint32_t now_ms);
#endif
