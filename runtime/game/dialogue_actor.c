#include "game/dialogue_actor.h"
int bk_dialogue_actor_initialize(BkDialogueActorState *s, unsigned group) {
  if (!s || group >= 5)
    return 0;
  s->speaker = (int32_t)group + 1;
  s->rules.expression = -1;
  s->rules.clip = 0;
  s->visibility = 0;
  return 1;
}
int bk_dialogue_actor_rules(BkDialogueActorRules *s, uint8_t phase, int32_t f,
                            int32_t m) {
  if (!s)
    return 0;
  if (phase)
    return 1;
  *s = (BkDialogueActorRules){.expression = -1};
  if (f >= 0 && f <= 56 && f % 10 <= 6) {
    static const int32_t gaze[3] = {0, 2, 1};
    s->expression = f % 10;
    s->gaze = gaze[f / 20];
    s->texture = (f / 10) % 2;
  }
  if (m >= 0 && m <= 15) {
    s->once = m >= 9;
    s->clip = m - (s->once ? 7 : 0);
  }
  return 1;
}
