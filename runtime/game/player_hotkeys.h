#ifndef BK_GAME_PLAYER_HOTKEYS_H
#define BK_GAME_PLAYER_HOTKEYS_H
#include <stdint.h>
enum {
  BK_PLAYER_PAUSE = 1u << 0,  /* mode1,arg0: 70 OR33453 */
  BK_PLAYER_CAMERA = 1u << 1, /* mode1,arg0: 74 OR33454 */
  BK_PLAYER_PHOTO = 1u << 2   /* mode1,arg0: 43 OR33455 */
};
typedef struct {
  uint8_t menu_request;           /*71bcd9 = player+7c9*/
  int32_t photo_count, photos[5]; /*734050,721b14*/
} BkPlayerHotkeys;
typedef struct {
  void *context;
  int (*sound)(void *, unsigned slot, char error[256]);
  int (*capture)(void *, int photo, char error[256]);
} BkPlayerHotkeyOps;
/*4c0126:01b6..02e8, BEFORE interaction and movement; only old script_phase0.
 * Caller supplies decoded edge pairs above, not held buttons. The three
 * requests are independent and ordered pause/camera/photo. Sound restarts;
 * capture config precedes the count write. Only special_mode==1 suppresses
 * photo. Counter>=100 plays slot5; negatives increment as the original.
 * Native-ordered side effects remain after service failure: discard session.
 * camera_transition is the live729780 byte, never copied into a second state.
 */
int bk_player_hotkeys_step(BkPlayerHotkeys *, uint8_t *camera_transition,
                           unsigned group, uint8_t special_mode,
                           uint32_t buttons, const BkPlayerHotkeyOps *,
                           char error[256]);
#endif
