#include "scene/selection_ui.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
typedef struct {
  float pointer[2];
  unsigned calls, fail_at, replaced, released;
} Services;
static int event(Services *s) { return ++s->calls != s->fail_at; }
static int sound(void *p, unsigned slot, char e[256]) {
  (void)e;
  assert(slot <= 4);
  return event(p);
}
static int gain(void *p, int32_t volume, char e[256]) {
  (void)e;
  assert(volume >= -10000 && volume <= 0);
  return event(p);
}
static int position(void *p, float out[2], char e[256]) {
  (void)e;
  memcpy(out, ((Services *)p)->pointer, 2 * sizeof(float));
  return event(p);
}
static int motion(void *p, float out[2], char e[256]) {
  (void)e;
  out[0] = out[1] = 0;
  return event(p);
}
static int warp(void *p, float x, float y, char e[256]) {
  (void)e;
  Services *s = p;
  s->pointer[0] = x;
  s->pointer[1] = y;
  return event(p);
}
static int stop(void *p, char e[256]) {
  (void)e;
  return event(p);
}
static int status(void *p, int *playing, char e[256]) {
  (void)e;
  *playing = 1;
  return event(p);
}
static int replace(void *p, unsigned group, char e[256]) {
  (void)e;
  assert(group < 5);
  Services *s = p;
  ++s->replaced;
  return event(s);
}
static int release(void *p, uint8_t flow, char e[256]) {
  (void)e;
  assert(flow == 0x38);
  Services *s = p;
  ++s->released;
  return event(s);
}
int main(void) {
  char e[256];
  Services svc = {0};
  BkSelectionOps o = {&svc, sound, gain,   position, motion, warp,
                      stop, gain,  status, replace,  release};
  BkSelectionUi s = {.selected = 3, .row = 4};
  assert(bk_selection_ui_initialize(&s, 1280, 0, -900, e));
  assert(s.selected == 3 && s.row == 4 &&
         s.loaded == ((UINT64_C(1) << 42) - 2));
  BkSelectionSprite retained = s.sprites[1];
  assert(bk_selection_ui_initialize(&s, 640, 1, -800, e));
  assert(!memcmp(&retained, &s.sprites[1], sizeof(retained)));
  assert(!bk_selection_image(1, 1) &&
         !strcmp(bk_selection_image(47, 1), "pr_99.tga"));
  memset(&s, 0, sizeof(s));
  assert(bk_selection_ui_initialize(&s, 640, 0, -900, e));
  BkCommonHudState c = {.curtain = {0, 2, 0}};
  BkFlowTransition flow = {0x38, 1, 0, 0};
  BkMenuCursor cur = {0};
  assert(bk_menu_cursor_initialize(&cur, 640, 480));
  uint8_t latch = 0;
  int32_t photos[5] = {1, 2, 3, 4, 5}, group = 0, area = 0, count = 0;
  BkSelectionBindings b = {&c,     &flow,  &cur,  &latch,
                           photos, &group, &area, &count};
  BkSelectionInput in = {.seconds = .1f,
                         .scale = .5f,
                         .music_master = -900,
                         .voice_master = -700,
                         .voice_present = 1};
  BkSelectionFrame f;
  svc.pointer[0] = 30;
  svc.pointer[1] = 94;
  assert(bk_selection_ui_view(&s, &b, &in, &o, &f, e));
  BkSelectionFrame frozen = f;
  in.buttons = BK_PAUSE_CONFIRM | BK_PAUSE_RIGHT;
  BkSelectionUi initial = s;
  BkCommonHudState ci = c;
  BkMenuCursor cu = cur;
  svc.calls = 0;
  assert(bk_selection_ui_control(&s, &b, &in, &o, e));
  assert(s.selected == 1 && svc.replaced == 1 && s.character_hover == 9 &&
         !memcmp(&f, &frozen, sizeof(f)));
  assert(s.pointer[0] == 30 && s.pointer[1] == 94 && svc.pointer[0] != 30);
  unsigned calls = svc.calls;
  for (unsigned i = 1; i <= calls; ++i) {
    s = initial;
    c = ci;
    cur = cu;
    latch = 0;
    svc = (Services){.fail_at = i};
    assert(!bk_selection_ui_control(&s, &b, &in, &o, e));
    assert(svc.calls == i && flow.current == 0x38);
  }
  s = initial;
  c = ci;
  cur = cu;
  latch = 0;
  svc = (Services){0};
  o.replace_actor = NULL;
  assert(!bk_selection_ui_control(&s, &b, &in, &o, e) && !svc.calls);
  o.replace_actor = replace;
  in.buttons = 0;
  s.selected = 4;
  s.pointer[0] = s.pointer[1] = -10;
  c = (BkCommonHudState){.curtain = {1, 2, 3}, .action = 23, .blocked = 1};
  assert(bk_selection_ui_control(&s, &b, &in, &o, e));
  assert(flow.current == 0x50 && flow.target == 8 && group == 4 && area == 0 &&
         count == 5 && s.selected == 0 && !s.loaded);
  flow.current = 0x38;
  s = initial;
  c = ci;
  in.seconds = NAN;
  svc.calls = 0;
  assert(!bk_selection_ui_view(&s, &b, &in, &o, &f, e) && !svc.calls);
  in.seconds = .1f;
  s.row = 100;
  in.buttons = BK_PAUSE_DOWN;
  assert(!bk_selection_ui_control(&s, &b, &in, &o, e));
  in.buttons = BK_PAUSE_CONFIRM;
  in.voice_present = 0;
  s = initial;
  s.info = 100;
  s.pointer[0] = s.sprites[40].rect[0];
  s.pointer[1] = s.sprites[40].rect[1];
  assert(!bk_selection_ui_control(&s, &b, &in, &o, e) &&
         strstr(e, "voice replay"));
  puts("selection UI service-prefix, retained slots, pointer/draw order and "
       "transition tests passed");
  return 0;
}
