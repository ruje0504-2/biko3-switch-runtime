#include "game/npc_detection.h"
int bk_npc_detection_resolve(BkNpcSpatialState *npc,
                             BkNpcInteractionState *interaction,
                             const BkNpcDetectionInput *input) {
  if (!npc || !interaction || !input)
    return 0;
  for (unsigned i = 0; i < 4; ++i)
    if (input->player_action == input->suppressed_actions[i]) {
      npc->visible = 0;
      return 1;
    }
  if (input->area >= 8) {
    npc->visible = 0;
    return 1;
  }
  if (interaction->outcome == 0 && npc->visible == 1) {
    interaction->outcome = 2;
    npc->ai.point.motion.behavior = 4;
    if (npc->ai.stimulus == 1 ||
        npc->ai.point.motion.action == input->forced_actions[0] ||
        npc->ai.point.motion.action == input->forced_actions[1])
      interaction->outcome = 3;
  }
  return 1;
}
