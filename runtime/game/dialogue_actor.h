#ifndef BK_GAME_DIALOGUE_ACTOR_H
#define BK_GAME_DIALOGUE_ACTOR_H
#include <stdint.h>
typedef struct {
  int32_t expression, gaze, texture, clip, once;
} BkDialogueActorRules;
typedef struct {
  int32_t speaker;
  BkDialogueActorRules rules;
  float mouth, visibility;
} BkDialogueActorState;
/*4ef6a0 actor scalar reset only. Gaze/texture/once/mouth remain held. */
int bk_dialogue_actor_initialize(BkDialogueActorState *, unsigned group);
/*4f1b53. Phase0 consumes literal dialogue #F/#M. Every other byte value
 * retains all five fields, including values held by image transitions. */
int bk_dialogue_actor_rules(BkDialogueActorRules *, uint8_t phase,
                            int32_t code_f, int32_t code_m);
#endif
