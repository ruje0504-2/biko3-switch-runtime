#include "game/npc_fade.h"
#include <math.h>
#include <stdio.h>
int bk_npc_fade_plan(BkNpcFade *out, float alpha, uint8_t fade_out,
                     unsigned group, float seconds, char error[256]) {
  if (!out || group >= 5 || !isfinite(alpha) || !isfinite(seconds) ||
      seconds < 0) {
    snprintf(error, 256, "NPC fade: invalid input");
    return 0;
  }
  BkNpcFade next = {.alpha = alpha};
  if (fade_out == 0 && alpha < 1) {
    next.alpha = (float)((double)alpha + seconds);
    if (next.alpha >= 1)
      next.alpha = 1;
    next.apply = next.restore_marks = 1;
  } else if (fade_out == 1 && alpha > 0) {
    next.alpha = (float)((double)alpha - seconds);
    if (next.alpha <= 0)
      next.alpha = 0;
    next.apply = 1;
  }
  next.rules[0] = (BkNpcFadeRule){"NULL", 0};
  next.rules[1] = (BkNpcFadeRule){"S2_magane_renzu", .2f};
  next.rules[2] = (BkNpcFadeRule){"G_reiko_kami_u", .99f};
  next.rule_count = group == 1 ? 3 : 1;
  *out = next;
  return 1;
}
