#include "game/player_hotkeys.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef struct {
  unsigned calls, fail_at;
  int events[5];
} Context;
static int record(Context *c, int event) {
  c->events[c->calls++] = event;
  return c->calls != c->fail_at;
}
static int sound(void *ctx, unsigned slot, char error[256]) {
  (void)error;
  return record(ctx, (int)slot);
}
static int capture(void *ctx, int photo, char error[256]) {
  (void)error;
  return record(ctx, 10 + photo);
}
int main(void) {
  char error[256];
  const int sequence[5] = {0, 10, 0, 7, 11};
  for (unsigned at = 0; at <= 5; ++at) {
    BkPlayerHotkeys s = {
        .menu_request = 5, .photo_count = 99, .photos = {30, 31, 32, 33, 34}};
    uint8_t camera = 1;
    Context c = {.fail_at = at};
    BkPlayerHotkeyOps ops = {&c, sound, capture};
    assert(bk_player_hotkeys_step(&s, &camera, 2, 0, 7, &ops, error) == !at);
    assert(c.calls == (at ? at : 5));
    assert(!memcmp(c.events, sequence, c.calls * sizeof(*sequence)));
    assert(s.menu_request == (at == 1 ? 5 : 1));
    assert(camera == (at && at <= 3 ? 1 : 0));
    assert(s.photo_count == (at ? 99 : 100));
    for (unsigned i = 0; i < 5; ++i)
      assert(s.photos[i] == (i == 2 && !at ? 100 : (int)i + 30));
  }
  Context c = {0};
  BkPlayerHotkeyOps ops = {&c, sound, capture};
  BkPlayerHotkeys s = {.photo_count = 100, .photos = {1, 2, 3, 4, 5}};
  uint8_t camera = 255;
  assert(bk_player_hotkeys_step(&s, &camera, 0, 0, 6, &ops, error));
  assert(c.calls == 2 && c.events[0] == 0 && c.events[1] == 5);
  assert(s.photo_count == 100 && s.photos[0] == 1 && camera == 255);
  c = (Context){0};
  assert(bk_player_hotkeys_step(&s, &camera, 0, 1, 4, &ops, error));
  assert(!c.calls);
  BkPlayerHotkeys saved = s;
  assert(!bk_player_hotkeys_step(&s, &camera, 5, 0, 7, &ops, error));
  assert(!memcmp(&saved, &s, sizeof(s)) && !c.calls);
  assert(!bk_player_hotkeys_step(&s, &camera, 0, 0, 8, &ops, error));
  assert(!bk_player_hotkeys_step(&s, &camera, 0, 0, 7, NULL, error));
  puts("PASS hotkey side-effect failure ordering and saturation");
  return 0;
}
