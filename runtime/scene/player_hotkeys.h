#ifndef BK_SCENE_PLAYER_HOTKEYS_H
#define BK_SCENE_PLAYER_HOTKEYS_H
#include "game/player_hotkeys.h"
#include "scene/system_audio.h"
typedef struct {
  /* Borrow process-lifetime system slots0,5,7, each with a distinct voice. */
  BkSystemAudio *confirm, *limit, *shutter;
  void *capture_context;
  int (*capture)(void *, int photo, unsigned album_group, char error[256]);
} BkPlayerHotkeyServices;
/* album_group is51917c's snapshot of group BEFORE the update dispatch.
 * The request service must retain it through the later HUD capture boundary.
 * An empty edge mask requires no service; nonempty masks require all services.
 */
int bk_scene_player_hotkeys(const BkPlayerHotkeyServices *, BkPlayerHotkeys *,
                            uint8_t *camera, unsigned group,
                            unsigned album_group, uint8_t special,
                            uint32_t buttons, char error[256]);
#endif
