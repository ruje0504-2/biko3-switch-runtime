#ifndef BK_GAME_SELECTION_ACTOR_H
#define BK_GAME_SELECTION_ACTOR_H
#include <stdint.h>
typedef struct {
  float eye_max, pitch_limit, yaw_limit;
  int32_t expression;
  int8_t gaze, texture;
} BkSelectionActorRules;
/*5073de; the eight persistent unlock bytes are distinct from route progress.
 * No RNG consumption unless all eight are nonzero. */
int bk_selection_actor_variant(const uint8_t unlocked[8], uint32_t *random,
                               uint8_t *alternate);
/*507193 and51ac5d constants. source is the OLD active clip's source tick,
 * read before401b0a and4026fe. No invented wall-clock mapping. */
int bk_selection_actor_rules(BkSelectionActorRules *, unsigned group,
                             uint8_t alternate, float source);
#endif
