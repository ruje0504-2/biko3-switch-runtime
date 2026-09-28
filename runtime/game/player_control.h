#ifndef BK_GAME_PLAYER_CONTROL_H
#define BK_GAME_PLAYER_CONTROL_H
#include "game/player_script.h"
#include "game/player_spatial.h"
typedef struct {
  BkPlayerSpatial spatial;
  BkPlayerInteraction interaction;
  float return_yaw; /* +7fc, set by later event policy, never guessed here. */
  int8_t completion_mode, completion_requested,
      wall_available; /* +7cb,+7c8,+57a. */
} BkPlayerControl;
typedef struct {
  BkPlayerSpatialInput spatial;
  /* actions/active_clip/position/normal/wall_name/wall_heading replaced by
   * live spatial inputs/state; group/area, props, keys and timings supplied. */
  BkPlayerInteractionInput interaction;
  int has_shadow;
} BkPlayerControlInput;
typedef struct {
  unsigned placements;
  BkActorPlacement roots[2];
  int shadow_place;
  BkActorPlacement shadow;
} BkPlayerControlEffects;
/* 4c009b dispatch + ordinary/script spatial paths. Includes ordinary final
 * availability query and optional Y+.1 shadow placement. Menu/UI hotkeys
 * from4c0126 are not represented by the movement/interaction button masks;
 * scene/game_frame executes their flow/audio side effects immediately before
 * this stage, only for old script_phase0. Phase3/other holds.
 * No animation advance, world-child publish or game-camera update. */
int bk_player_control_step(BkPlayerControl *state, const BkCollision *collision,
                           const BkPlayerControlInput *input,
                           BkPlayerControlEffects *effects, char error[256]);
#endif
