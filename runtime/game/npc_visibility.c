#include "game/npc_visibility.h"
#include <stdio.h>
int bk_npc_visibility_plan(BkNpcVisibility *out,
                           const BkNpcVisibilityInput *input) {
  if (!out || !input)
    return 0;
  BkNpcVisibility next = {
      .root_hidden = input->hidden, .marks = {1, 1, 1, 1}, .override_marks = 1};
  if (input->interface_mode == 2 && input->area < 8) {
    if (input->phase == 1) {
      switch (input->behavior) {
      case 0:
        next.marks[0] = 0;
        break;
      case 1:
        break;
      case 2:
        next.marks[1] = 0;
        break;
      case 3:
        next.marks[2] = 0;
        break;
      case 4:
        next.marks[3] = 0;
        break;
      default:
        next.override_marks = 0;
      }
    } else if (input->phase != 0 && input->phase != 2 && input->phase != 3)
      next.override_marks = 0;
  }
  *out = next;
  return 1;
}
int bk_npc_visibility_apply(BkActorPose *actor, uint32_t root,
                            const uint32_t marks[4],
                            const BkNpcVisibilityInput *input,
                            char error[256]) {
  BkNpcVisibility plan;
  if (!actor || !marks || !bk_npc_visibility_plan(&plan, input)) {
    snprintf(error, 256, "NPC visibility: invalid input");
    return 0;
  }
  BkActorVisibilityEdit edits[5] = {{root, plan.root_hidden}};
  if (plan.override_marks)
    for (unsigned i = 0; i < 4; i++)
      edits[i + 1] = (BkActorVisibilityEdit){marks[i], plan.marks[i]};
  return bk_actor_pose_visibility(actor, edits, plan.override_marks ? 5 : 1,
                                  error);
}
