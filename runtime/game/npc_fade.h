#ifndef BK_GAME_NPC_FADE_H
#define BK_GAME_NPC_FADE_H
#include <stdint.h>
typedef struct {
  const char *name;
  float alpha;
} BkNpcFadeRule;
typedef struct {
  float alpha;
  int apply, restore_marks;
  unsigned rule_count;
  BkNpcFadeRule rules[3];
} BkNpcFade;
/* 0x4fc7a2 policy after primary/accessory animation. Mode0 adds seconds
 * belowalpha1 and restores marker subtrees to encodedalpha2 (additive).
 * Mode1 subtracts seconds abovealpha0; other bytes do nothing. Group1
 * preserves glasses.2/hair.99; all groups include literal NULL=>0.
 * Produces a material edit plan; scene commits it with actor alpha only
 * after material validation succeeds. Does not own or mutate model state. */
int bk_npc_fade_plan(BkNpcFade *out, float alpha, uint8_t fade_out,
                     unsigned group, float seconds, char error[256]);
#endif
