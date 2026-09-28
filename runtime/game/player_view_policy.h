#ifndef BK_GAME_PLAYER_VIEW_POLICY_H
#define BK_GAME_PLAYER_VIEW_POLICY_H
#include "world/player_view.h"
typedef struct {
  int8_t mode;
  uint8_t hidden;
} BkPlayerViewFlags;
/* Complete4b8a89 route/flags before the selected controller. camera_mode is
 * native729780 (not main camera phase), npc_hidden is the raw byte. */
int bk_player_view_route(BkPlayerViewFlags *flags, BkPlayerViewKind *route,
                         int8_t camera_mode, uint8_t npc_hidden, int32_t action,
                         const int32_t actions[21]);
#endif
