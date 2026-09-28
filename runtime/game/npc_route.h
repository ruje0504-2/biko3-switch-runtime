#ifndef BK_GAME_NPC_ROUTE_H
#define BK_GAME_NPC_ROUTE_H
#include "game/npc_point.h"
#include "world/route_motion.h"
typedef struct {
  BkRouteMotion path;
  BkNpcPointState point;
} BkNpcRouteState;
typedef struct {
  BkNpcMotion movement;
  BkNpcPointEffects point;
  uint32_t sound_cursor;
  /* Position at4f6226, before later head/ground/root stages. */
  float sound_position[3];
} BkNpcRouteEffects;
/* 0x4fc97f..0x4fca12 after AI, before head reads/collision/root placement.
 * Select action/distance, advance route, apply the LAST crossed point and
 * optionally snap horizontal position to it, in that order. Preserve yaw
 * computed before snap. Effects are commands for the caller to consume.
 * This is not a full actor update: it does not run AI, start cursor zero,
 * resolve collision, sample/publish animation or dispatch media. */
int bk_npc_route_move(BkNpcRouteState *state, const BkRoute *route,
                      const BkNpcMotionActions *actions,
                      int32_t background_clip, float seconds, uint32_t now_ms,
                      BkNpcRouteEffects *effects, char error[256]);
#endif
