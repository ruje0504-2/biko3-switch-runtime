#include "game/player_view_policy.h"
int bk_player_view_route(BkPlayerViewFlags *flags, BkPlayerViewKind *route,
                         int8_t camera_mode, uint8_t npc_hidden, int32_t action,
                         const int32_t actions[21]) {
  if (!flags || !route || !actions)
    return 0;
  BkPlayerViewFlags next = *flags;
  BkPlayerViewKind kind = BK_PLAYER_VIEW_HOLD;
  switch (next.mode) {
  case 0:
    next.hidden = 0;
    if (camera_mode == 0)
      kind = BK_PLAYER_VIEW_ORBIT;
    else if (camera_mode == 1)
      kind = BK_PLAYER_VIEW_HEAD;
    break;
  case 1:
    if (camera_mode != 0)
      kind = BK_PLAYER_VIEW_HEAD;
    else if (npc_hidden == 0)
      kind = BK_PLAYER_VIEW_RAY;
    else if (npc_hidden == 1)
      kind = BK_PLAYER_VIEW_ORBIT;
    break;
  case 2:
    next.hidden = 1;
    if (action == actions[11])
      kind = BK_PLAYER_VIEW_PROP_HEAD;
    else if (action == actions[13])
      kind = BK_PLAYER_VIEW_PROP_LOW;
    else if (action == actions[16])
      kind = BK_PLAYER_VIEW_WALL;
    break;
  case 3:
    kind = BK_PLAYER_VIEW_TRACK;
    break;
  case 5:
    kind = BK_PLAYER_VIEW_COVER;
    break;
  case 6:
    if (npc_hidden == 0)
      kind = BK_PLAYER_VIEW_NPC_FRONT;
    else if (npc_hidden == 1)
      next.mode = 0;
    break;
  case 7:
    kind = BK_PLAYER_VIEW_HEAD;
    break;
  default:
    break;
  }
  *flags = next;
  *route = kind;
  return 1;
}
