#ifndef BK_GAME_FLOW_TRANSITION_H
#define BK_GAME_FLOW_TRANSITION_H
#include <stdint.h>
typedef struct {
  uint8_t current, previous, target, mode;
} BkFlowTransition;
typedef struct {
  void *context;
  int (*release)(void *, uint8_t flow, char error[256]);
} BkFlowTransitionOps;
/*51c47e schedules flow50. Unless mode==2, releases current scene first.
 * No loader execution or scene existence is fabricated. Failure preserves
 * these flow bytes, though release may have partial ownership effects. */
int bk_flow_transition_schedule(BkFlowTransition *, const BkFlowTransitionOps *,
                                uint8_t target, uint8_t mode, char error[256]);
#endif
