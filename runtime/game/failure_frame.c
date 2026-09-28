#include "game/failure_frame.h"
#include <stdio.h>
int bk_failure_frame_step(uint8_t outcome, const BkFailureFrameBindings *b,
                          const BkFailureFrameOps *ops, char error[256]) {
  if (!b || !ops || !b->visible || !b->player_hidden || !b->npc_hidden ||
      !b->player_action || !b->npc_action || !b->player_idle || !b->npc_idle ||
      !b->npc_rear_action || (outcome == 5 && !b->prop_action) ||
      !ops->camera || !ops->restart || !ops->present) {
    snprintf(error, 256, "failure frame: missing live binding/service");
    return 0;
  }
  if (outcome >= 1 && outcome <= 6) {
    static const BkFailureCameraKind kinds[6] = {
        BK_FAILURE_CAMERA_OVERHEAD, BK_FAILURE_CAMERA_NPC_FRONT,
        BK_FAILURE_CAMERA_NPC_REAR, BK_FAILURE_CAMERA_PLAYER,
        BK_FAILURE_CAMERA_PROP,     BK_FAILURE_CAMERA_NPC_FRONT};
    int arrived = 0;
    if (!ops->camera(ops->context, kinds[outcome - 1], &arrived, error))
      return 0;
    if (arrived) {
      if (*b->visible == 0 &&
          (outcome == 2 || outcome == 3 || outcome == 5 || outcome == 6)) {
        if (!ops->restart(ops->context, outcome == 5, error))
          return 0;
        if (outcome == 5)
          *b->prop_action = 1;
      }
      *b->visible = 1;
    }
    if (outcome == 2 || outcome == 3 || outcome == 5 || outcome == 6)
      *b->player_hidden = 1;
    *b->player_action = outcome == 1 ? 25 : *b->player_idle;
    *b->npc_action = outcome == 3 ? *b->npc_rear_action : *b->npc_idle;
    if (outcome == 4 || outcome == 5)
      *b->npc_hidden = 1;
  }
  for (BkFailureFrameStage stage = BK_FAILURE_FRAME_PLAYER;
       stage <= BK_FAILURE_FRAME_PROPS; ++stage)
    if (!ops->present(ops->context, stage, error))
      return 0;
  return 1;
}
