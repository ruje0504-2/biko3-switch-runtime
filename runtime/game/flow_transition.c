#include "game/flow_transition.h"
#include <stdio.h>
int bk_flow_transition_schedule(BkFlowTransition *s,
                                const BkFlowTransitionOps *o, uint8_t target,
                                uint8_t mode, char e[256]) {
  if (!s || !o || !o->release) {
    snprintf(e, 256, "flow transition: missing state/release service");
    return 0;
  }
  if (mode != 2 && !o->release(o->context, s->current, e))
    return 0;
  s->previous = s->current;
  s->target = target;
  s->current = 0x50;
  s->mode = mode;
  return 1;
}
