#include "game/player_hotkeys.h"
#include <stdio.h>
int bk_player_hotkeys_step(BkPlayerHotkeys *s, uint8_t *camera, unsigned group,
                           uint8_t special, uint32_t buttons,
                           const BkPlayerHotkeyOps *ops, char error[256]) {
  if (!s || !camera || group >= 5 || (buttons & ~7u) || !ops || !ops->sound ||
      !ops->capture) {
    snprintf(error, 256, "player hotkeys: invalid state/input/services");
    return 0;
  }
  if (buttons & BK_PLAYER_PAUSE) {
    if (!ops->sound(ops->context, 0, error))
      return 0;
    s->menu_request = 1;
    if (!ops->capture(ops->context, 0, error))
      return 0;
  }
  if (buttons & BK_PLAYER_CAMERA) {
    if (!ops->sound(ops->context, 0, error))
      return 0;
    if (*camera == 0)
      *camera = 1;
    else if (*camera == 1)
      *camera = 0;
  }
  if (special != 1 && (buttons & BK_PLAYER_PHOTO)) {
    if (s->photo_count >= 100)
      return ops->sound(ops->context, 5, error);
    if (!ops->sound(ops->context, 7, error) ||
        !ops->capture(ops->context, 1, error))
      return 0;
    /* The guard guarantees signed addition cannot overflow. */
    ++s->photo_count;
    s->photos[group] = s->photo_count;
  }
  return 1;
}
