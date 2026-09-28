#ifndef BK_GAME_PROP_INTERACTION_H
#define BK_GAME_PROP_INTERACTION_H
#include "game/player_control.h"
#include "game/prop_sound.h"
typedef struct {
  BkTimer alarm;   /* Prop+854; independent of its route wait at+840. */
  uint8_t car_hit; /* Prop+337; retained until the actor is reinitialized. */
} BkPropInteractionState;
typedef struct {
  int32_t selected;  /* 728de4: index used by outcome5, not prop kind. */
  uint8_t available; /* Player+57b: cleared once at start of every call. */
  float matrix[16];  /* Player+580: retained when no eligible prop is near. */
} BkPropInteractionShared;
typedef struct {
  /* NULL motion denotes an absent slot. Other pointers required for present
   * slots; world is only read by kinds10/11/18/19 after the proximity gates.
   * These borrow live owners; do not keep a second copy of motion/sound state.
   * Distinct slots must refer to distinct mutable actors. */
  BkPropState *motion;
  BkPropSoundState *sound;
  BkPropInteractionState *interaction;
  const float *world;
} BkPropInteractionActor;
typedef struct {
  int32_t player_actions[21];
  const char *wall;
  uint32_t now_ms;
  uint8_t hud_blocked; /* beeb7f */
} BkPropInteractionInput;
typedef struct {
  unsigned index;
  /* 0: DirectSound Stop (pause), 1: direct Play, preserving cursor. */
  int play, loop;
} BkPropInteractionSound;
typedef struct {
  unsigned count;
  BkPropInteractionSound sounds[16];
} BkPropInteractionCommands;
/* Complete4f4306, including every present slot in ascending order, even
 * hidden actors. Player action changes affect later slots. stimulus points
 * to the SAME NPC+319 byte used by AI/footsteps; outcome to71bcd8.
 * No pose advance/publication or device calls. All CPU outputs are atomic on
 * invalid reached geometry; ordered sound commands must be consumed before
 * the following detection/area stages. */
int bk_prop_interaction_step(const BkPropInteractionActor actors[16],
                             BkPlayerControl *, int8_t *stimulus,
                             uint8_t *outcome, BkPropInteractionShared *,
                             const BkPropInteractionInput *,
                             BkPropInteractionCommands *, char error[256]);
/* Exact578a30 collision strings. Includes literal "NULL" for kinds1/2. */
const char *bk_prop_interaction_wall(int32_t kind);
#endif
