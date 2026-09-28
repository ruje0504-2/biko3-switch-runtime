#ifndef BK_GAME_NPC_DETECTION_H
#define BK_GAME_NPC_DETECTION_H
#include "game/npc_spatial.h"
typedef struct {
  int32_t area, player_action;
  int32_t suppressed_actions[4]; /* Player live slots11..14. */
  int32_t forced_actions[2];     /* NPC live actor+68/+6c bindings. */
} BkNpcDetectionInput;
/* Original4f3c80 after prop interaction and before area-boundary processing.
 * Operates the SAME detected byte(+318), stimulus(+319), behavior(+850) and
 * global outcome used by spatial/AI/contact; no second detection state.
 * Suppressed actions/area>=8 clear detection even with an existing outcome.
 * Otherwise only detection==1 and outcome==0 set behavior4/outcome2 or3.
 * This changes outcome codes; presentation/scene transition is subsequent.
 * All raw byte/integer states are supported. NULL inputs preserve outputs. */
int bk_npc_detection_resolve(BkNpcSpatialState *, BkNpcInteractionState *,
                             const BkNpcDetectionInput *);
#endif
