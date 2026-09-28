#ifndef BK_GAME_PLAYER_INTERACTION_H
#define BK_GAME_PLAYER_INTERACTION_H
#include "game/player_movement.h"
#include "game/player_trigger.h"
#define BK_PLAYER_CLIP_SLOTS 128
/* CPU snapshot of each live animation slot, including inactive slots. */
typedef struct {
  float start, end, source;
} BkPlayerClipTiming;
enum { BK_PLAYER_STANCE = 1u, BK_PLAYER_INTERACT = 2u };
typedef struct {
  BkPlayerTrigger trigger;
  int8_t script_phase; /* Native+7f8. */
} BkPlayerInteraction;
typedef struct {
  /* position/action replaced by live movement state; actions and world
   * queries remain caller-owned. Inputs merge original device aliases. */
  BkPlayerTriggerInput trigger;
  float normal[3];
  uint32_t buttons;
  int32_t active_clip;
  BkPlayerClipTiming clips[BK_PLAYER_CLIP_SLOTS];
} BkPlayerInteractionInput;
/* Complete4c20ee: stance precedes interaction; mutating target queries are
 * short-circuited in native order. Output controls_allowed is its AL result.
 * This selects actions/phases, without advancing animation or moving roots.
 * Invalid inputs reached by a branch leave all three outputs unchanged. */
int bk_player_interaction(BkPlayerMovement *movement,
                          BkPlayerInteraction *state, int *controls_allowed,
                          const BkPlayerInteractionInput *input,
                          char error[256]);
#endif
