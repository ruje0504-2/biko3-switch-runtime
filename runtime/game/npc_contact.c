#include "game/npc_contact.h"
#include "world/proximity.h"
#include <math.h>
static int near_player(const BkNpcContactInput *input, int *hit) {
  float start[3], end[3];
  for (unsigned i = 0; i < 3; i++) {
    double delta = (double)input->player_direction[i] * 4;
    start[i] = (float)((double)input->player_position[i] + delta);
    end[i] = (float)((double)input->player_position[i] - delta);
  }
  return bk_proximity_segment_xz(hit, start, end, input->actor_position, 15);
}
static int same_height(const BkNpcContactInput *input) {
  return (double)input->actor_position[1] >=
             (double)input->player_position[1] - 20 &&
         (double)input->actor_position[1] <=
             (double)input->player_position[1] + 20;
}
int bk_npc_contact_query(BkNpcContactState *state,
                         const BkNpcContactInput *input,
                         BkNpcContactQuery query, uint8_t *result) {
  if (!state || !input || !result || !isfinite(input->alpha) ||
      query < BK_NPC_CONTACT_SCRIPTED || query > BK_NPC_CONTACT_AREA)
    return 0;
  for (unsigned i = 0; i < 3; i++)
    if (!isfinite(input->actor_position[i]) ||
        !isfinite(input->player_position[i]) ||
        !isfinite(input->player_direction[i]))
      return 0;
  BkNpcContactState next = *state;
  uint8_t code = 0;
  int hit;
  if (query == BK_NPC_CONTACT_SCRIPTED) {
    const int32_t areas[] = {4, 5, 5, 6, 5};
    const uint32_t cursors[] = {67, 83, 195, 47, 231};
    if (input->group >= 0 && input->group < 5 &&
        input->area == areas[input->group] &&
        input->cursor == cursors[input->group] && next.behavior == 0) {
      next.prompt = 1;
      if (!near_player(input, &hit))
        return 0;
      code = (uint8_t)hit;
    }
  } else if (query == BK_NPC_CONTACT_WAITING) {
    if (input->interaction_e0 != 0 && next.behavior == 0) {
      next.prompt = 1;
      if (same_height(input)) {
        if (!near_player(input, &hit))
          return 0;
        if (hit) {
          next.response = 3;
          code = 1;
        }
      }
    }
  } else if (!(input->group == 2 && (double)input->alpha >= .99) &&
             input->area >= 8) {
    code = 1;
    if (same_height(input)) {
      if (!near_player(input, &hit))
        return 0;
      if (hit) {
        if (input->interaction_df == 1) {
          next.response = 4;
          code = 2;
        } else if (input->interaction_e0 == 1) {
          next.response = 3;
          code = 3;
        } else {
          next.outcome = 6;
          next.behavior = 4;
        }
      }
    }
  }
  *state = next;
  *result = code;
  return 1;
}
