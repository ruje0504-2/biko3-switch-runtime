#include "ui/text_flow.h"
#include <math.h>
int bk_text_flow_step(BkTextFlow *state, float seconds, int changed,
                      BkTextUpdate *out) {
  if (!state || !out || !isfinite(seconds) || seconds < 0 ||
      !isfinite(state->delay) || !isfinite(state->scroll) ||
      !isfinite(state->target))
    return 0;
  BkTextFlow next = *state;
  BkTextUpdate update = {0};
  if (next.enabled) {
    if (next.started) {
      next.delay = (float)((double)next.delay + (double)seconds * 5);
      if (!isfinite(next.delay))
        return 0;
      update.update = next.delay > 50;
    } else
      update.update = changed != 0;
    if (update.update) {
      next.scroll = (float)((double)next.scroll + (double)seconds * 10);
      if (!isfinite(next.scroll))
        return 0;
      if (next.scroll > next.target) {
        next.scroll = next.target;
        next.enabled = 0;
      }
      if ((double)next.scroll < -2147483648. ||
          (double)next.scroll >= 2147483648.)
        return 0;
      update.source_y = (int32_t)next.scroll;
      next.started = 1;
    }
  }
  *state = next;
  *out = update;
  return 1;
}
