#ifndef BK_GAME_PROP_MOTION_H
#define BK_GAME_PROP_MOTION_H
#include "core/timer.h"
#include "world/prop_route.h"
typedef struct {
  int32_t kind, action, actions[4];
  BkPropRoute path;
  float target_yaw;
  BkTimer wait;
  int8_t background_wait, route_flag;
  uint8_t hidden;
} BkPropState;
typedef struct {
  float seconds;
  int32_t player_action, player_actions[21], background_clip;
  int32_t npc_last_crossed;
  int8_t npc_previous_flags[2], player_mode;
} BkPropMotionInput;
typedef struct {
  /* Original5121ce initializes distance once, before the entire prop loop.
   * Kinds2/6/7 keep it; do not reset between props. Alpha is shared across
   * instances/frames (bf4b58); caller owns initialization/lifetime. */
  float distance, alpha;
  int32_t last_crossed;
} BkPropShared;
typedef struct {
  int allowed;
  /* -2: no material write; -1: all materials; 0/1: native material-name rule
   * group. */
  int material;
  float alpha;
} BkPropMotionEffects;
/* 512595/5147d0/5148f2. Pure state/policy; material writes are explicit. */
int bk_prop_motion_select(BkPropState *, BkPropShared *,
                          const BkPropMotionInput *, BkPropMotionEffects *);
/* 4f56a3 point dispatch. Does not snap position or consume timer. */
int bk_prop_point_apply(BkPropState *, int8_t flag, const BkPropMotionInput *);
/* 5127eb and5149e0. */
int bk_prop_smooth_heading(int32_t kind);
int bk_prop_needs_ground(int32_t group, int32_t area);
#endif
