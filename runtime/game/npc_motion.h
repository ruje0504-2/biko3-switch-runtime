#ifndef BK_GAME_NPC_MOTION_H
#define BK_GAME_NPC_MOTION_H
#include "core/timer.h"
typedef struct {
  int32_t idle, walk, run, stationary[4];
} BkNpcMotionActions;
typedef struct {
  int32_t action, behavior;
  uint8_t hidden;
  int8_t mode, route_flag;
  BkTimer wait;
} BkNpcMotionState;
typedef struct {
  float distance;
  int allowed;
} BkNpcMotion;
/* 0x4fd5f1 policy only: resolves requested action and movement distance.
 * allowed can be true with zero distance for stationary actions. Geometry,
 * route cursor advancement, collision and other AI decisions are separate.
 * Timer is read only on the native wait branch. Failure is transactional. */
int bk_npc_motion_select(BkNpcMotionState *state,
                         const BkNpcMotionActions *actions,
                         int32_t background_clip, float seconds,
                         uint32_t now_ms, BkNpcMotion *out);
/* Verified 0x4fb645..0x4fb73e motion slots for groups 0..4. */
int bk_npc_motion_actions(BkNpcMotionActions *out, unsigned group);
#endif
