#include "game/ending_entry.h"
#include <math.h>
#include <stdio.h>
static int fail(char e[256], const char *why) {
  snprintf(e, 256, "ending entry: %s", why);
  return 0;
}
static int load(const BkEndingEntryOps *o, BkEndingLoader loader, int32_t arg,
                char e[256]) {
  return o->load ? o->load(o->context, loader, arg, e)
                 : fail(e, "missing required resource loader");
}
static int profile(const BkEndingEntryBindings *b, char e[256]) {
  return b->frame->group < 5 || fail(e, "invalid live group");
}
int bk_ending_entry_dispatch(const BkEndingEntryBindings *b, int8_t previous,
                             uint32_t selected, float scale,
                             const BkEndingEntryOps *o, char e[256]) {
  if (!b || !b->frame || !b->control || !b->auxiliary || !b->option_a ||
      !b->option_b || !b->selected_group || !b->gauge_y || !o)
    return fail(e, "missing live state/services");
  if (previous == 8) {
    b->control->variant = b->auxiliary->variant ? 5 : 0;
    b->frame->phase = b->auxiliary->variant ? 3 : 1;
    if (!load(o,
              b->auxiliary->variant ? BK_ENDING_LOAD_4D2320
                                    : BK_ENDING_LOAD_4CF318,
              -1, e) ||
        !profile(b, e))
      return 0;
    return o->clear_record ? o->clear_record(o->context, b->frame->group, e)
                           : fail(e, "missing recording reset service");
  }
  if (previous != 0x18)
    return 1;
  *b->option_a = 1;
  *b->option_b = 1;
  *b->selected_group = (int8_t)b->frame->group;
  switch (selected) {
  case 0:
    b->control->variant = 0;
    b->frame->phase = 1;
    return load(o, BK_ENDING_LOAD_4CF318, -1, e);
  case 1:
    b->control->variant = 1;
    b->frame->phase = 2;
    return load(o, BK_ENDING_LOAD_4D00FA, -1, e);
  case 2:
  case 5: {
    if (!profile(b, e) || !isfinite(scale) || scale <= 0 ||
        !isfinite(*b->gauge_y))
      return fail(e, "invalid gauge/group input");
    b->auxiliary->progress = .4f;
    *b->gauge_y = (float)((double)*b->gauge_y - 80. * scale);
    b->frame->phase = selected == 2 ? 5 : 6;
    int arg;
    if (selected == 2) {
      arg = (int[]){0, 2, 1, 0, 1}[b->frame->group];
      b->control->variant = (uint8_t)(2 + arg);
    } else {
      arg = b->frame->group == 2;
      b->control->variant = (uint8_t)(7 + arg);
    }
    return load(o, BK_ENDING_LOAD_4D1025, arg, e);
  }
  case 3:
    b->control->variant = 5;
    b->frame->phase = 3;
    return load(o, BK_ENDING_LOAD_4D2320, -1, e);
  case 4:
    b->control->variant = 6;
    b->frame->phase = 4;
    return load(o, BK_ENDING_LOAD_4D39E6, -1, e);
  case 6:
    if (!o->prepare_final)
      return fail(e, "missing final-entry service");
    if (!o->prepare_final(o->context, e))
      return 0;
    /*48d7f2 may have updated the retained variant. Phase8 is written only
     * AFTER the actual loader, unlike the other selections. */
    b->control->variant = b->auxiliary->variant ? 5 : 0;
    if (!load(o,
              b->auxiliary->variant ? BK_ENDING_LOAD_4D2320
                                    : BK_ENDING_LOAD_4CF318,
              -1, e))
      return 0;
    b->frame->phase = 8;
    b->frame->state_721ee0 = 3;
    return 1;
  default:
    return 1;
  }
}
