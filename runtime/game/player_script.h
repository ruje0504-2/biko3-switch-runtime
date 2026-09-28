#ifndef BK_GAME_PLAYER_SCRIPT_H
#define BK_GAME_PLAYER_SCRIPT_H
#include "game/player_interaction.h"
#include "world/placement.h"
typedef struct {
  int complete;
  unsigned placements;
  BkActorPlacement roots[2];
} BkPlayerScriptEffects;
/* Complete4c0d9e. Completion precedes interaction; delta is computed before
 * it, while movement gates use the action AFTER it. Root writes preserve
 * native order (0,1,2 writes), without advancing animation or child caches.
 * Caller supplies +7fc return_yaw and applies the enclosing phase policy.
 * Ordinary collision and vertical_position are deliberately not changed.
 * Failure preserves all outputs; successful completion may have no writes. */
int bk_player_script_step(BkPlayerMovement *movement,
                          BkPlayerInteraction *interaction, float seconds,
                          float return_yaw,
                          const BkPlayerInteractionInput *input,
                          BkPlayerScriptEffects *effects, char error[256]);
#endif
