#ifndef BK_GAME_NPC_AI_H
#define BK_GAME_NPC_AI_H
#include "game/npc_contact.h"
#include "game/npc_point.h"
typedef struct {
  BkNpcPointState point;
  int8_t stimulus;
} BkNpcAiState;
typedef struct {
  uint8_t prompt, response, outcome;
} BkNpcInteractionState;
typedef struct {
  BkNpcContactInput contact;
  int32_t player_action, suppressed_actions[6], active_clip;
} BkNpcAiInput;
typedef struct {
  /* 0 none; 1 original buffer7299c0; 2 buffer729ae0. Caller plays sound. */
  uint8_t sound;
  /* Original ebp-8 was uninitialized on this branch. Port defines choice0
   * without consuming an extra random value; this correction is explicit. */
  uint8_t used_zero_choice;
} BkNpcAiEffects;
/* 0x4fce2b AI decisions before npc_route_move. Uses original contact queries,
 * timer order, random generator and action completion. No audio playback,
 * route movement, collision or animation is performed here. Failure commits
 * neither actor/shared state, RNG nor effects. Input is previous frame state
 * where required; caller remains responsible for native frame ordering. */
int bk_npc_ai_step(BkNpcAiState *state, BkNpcInteractionState *interaction,
                   uint32_t *random_state, const BkNpcAiInput *input,
                   uint32_t now_ms, BkNpcAiEffects *effects, char error[256]);
#endif
