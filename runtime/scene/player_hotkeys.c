#include "scene/player_hotkeys.h"
typedef struct {
  const BkPlayerHotkeyServices *services;
  unsigned album_group;
} Context;
static int sound(void *context, unsigned slot, char error[256]) {
  Context *c = context;
  return bk_system_audio_restart(slot == 0   ? c->services->confirm
                                 : slot == 5 ? c->services->limit
                                             : c->services->shutter,
                                 error);
}
static int capture(void *context, int photo, char error[256]) {
  Context *c = context;
  return c->services->capture(c->services->capture_context, photo,
                              c->album_group, error);
}
int bk_scene_player_hotkeys(const BkPlayerHotkeyServices *v, BkPlayerHotkeys *s,
                            uint8_t *camera, unsigned group, unsigned album,
                            uint8_t special, uint32_t buttons,
                            char error[256]) {
  if (!s || !camera || group >= 5 || album >= 5 || (buttons & ~7u) ||
      (buttons &&
       (!v || !v->confirm || !v->limit || !v->shutter || !v->capture))) {
    snprintf(error, 256, "player hotkeys: missing live services/bindings");
    return 0;
  }
  if (!buttons)
    return 1;
  Context c = {v, album};
  const BkPlayerHotkeyOps ops = {&c, sound, capture};
  return bk_player_hotkeys_step(s, camera, group, special, buttons, &ops,
                                error);
}
