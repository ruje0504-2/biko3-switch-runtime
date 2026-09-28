#include "scene/pause.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
typedef struct {
  unsigned calls, fail_at, events[16];
  float point[2];
} Services;
static int event(Services *s, unsigned code) {
  assert(s->calls < 16);
  s->events[s->calls++] = code;
  return s->calls != s->fail_at;
}
static int sound(void *p, unsigned slot, char e[256]) {
  (void)e;
  return event(p, 100 + slot);
}
static int warp(void *p, float x, float y, char e[256]) {
  (void)e;
  Services *s = p;
  s->point[0] = x;
  s->point[1] = y;
  return event(s, 200);
}
static int pointer(void *p, float pos[2], float motion[2], char e[256]) {
  (void)e;
  memcpy(pos, ((Services *)p)->point, sizeof(float) * 2);
  motion[0] = motion[1] = 0;
  return event(p, 300);
}
static int release(void *p, uint8_t flow, char e[256]) {
  (void)e;
  return event(p, 400 + flow);
}
static int remove_capture(void *p, char e[256]) {
  (void)e;
  return event(p, 500);
}
int main(void) {
  char e[256];
  Services svc = {0};
  BkPauseOps ops = {&svc, sound, warp, pointer, release, remove_capture};
  BkPauseState s = {.page = 3, .row = 4, .action = 14, .cursor = {7, 9}};
  assert(bk_pause_initialize(&s, 640, &ops, e));
  assert(s.page == 3 && s.row == 4 && s.action == 14 && s.cursor[0] == 7);
  assert(s.loaded && svc.point[0] == 320 && svc.point[1] == 240);
  BkMenuCursor cursor = {.idle = {500, 123, 1}};
  assert(bk_menu_cursor_initialize(&cursor, 640, 480));
  assert(cursor.idle.duration == 10000 && cursor.idle.deadline == 123 &&
         cursor.idle.armed == 1);
  BkCommonHudState c = {.curtain = {1, 2, 3}, .blocked = 1};
  BkFlowTransition flow = {4, 2, 0, 0};
  uint8_t overlay = 0, latch = 0;
  BkPauseBindings b = {&c, &flow, &cursor, &overlay, &latch};
  BkPauseInput in = {.scale = .5f, .seconds = .25f};
  BkPauseFrame frame;
  BkPauseState start = s;
  /* Exit confirmation has two explicit releases, then schedule releases4
   * again, then removes the screenshot. Failure retains the exact prefix. */
  for (unsigned fail_at = 1; fail_at <= 5; ++fail_at) {
    s = start;
    flow = (BkFlowTransition){4, 2, 0, 0};
    c.blocked = 1;
    c.curtain = (BkFadeSprite){1, 2, 3};
    svc = (Services){.fail_at = fail_at};
    assert(!bk_pause_step(&s, &b, &in, &ops, &frame, e));
    assert(svc.calls == fail_at);
    const unsigned expected[] = {404, 402, 404, 500, 300};
    assert(!memcmp(svc.events, expected, fail_at * sizeof(unsigned)));
    assert(s.loaded == (fail_at == 1));
    assert(flow.current == (fail_at <= 3 ? 4 : 0x50));
    assert(s.action == (fail_at <= 4 ? 14 : 0));
  }
  /* Hit tests consume the prior sensor sample, not this frame's position. */
  s = (BkPauseState){0};
  svc = (Services){0};
  assert(bk_pause_initialize(&s, 640, &ops, e));
  s.sprites[0].fade = (BkFadeSprite){1, 2, 3};
  c = (BkCommonHudState){.curtain = {0, 2, 0}};
  flow.current = 4;
  svc = (Services){.point = {320, 240}};
  in.buttons = BK_PAUSE_CONFIRM;
  assert(bk_pause_step(&s, &b, &in, &ops, &frame, e));
  assert(s.action == 0 && s.hover == 99 && s.cursor[0] == 320);
  svc.calls = 0;
  assert(bk_pause_step(&s, &b, &in, &ops, &frame, e));
  assert(s.action == 4 && overlay == 1 && !s.background_show);
  /* CPU suppression for absent screenshots is defined, unlike the native
   * immediate-alpha resource setter's null-handle dereference. */
  s.loaded = 0;
  assert(bk_pause_backdrop(&s, &frame, e) && frame.count == 0);
  s.page = 4;
  assert(!bk_pause_step(&s, &b, &in, &ops, &frame, e));
  s.page = 1;
  s.row = INT32_MAX;
  in.buttons = BK_PAUSE_UP;
  svc.calls = 0;
  assert(!bk_pause_step(&s, &b, &in, &ops, &frame, e));
  s.cursor[0] = NAN;
  assert(!bk_pause_step(&s, &b, &in, &ops, &frame, e));
  puts("PASS pause retained state, service failure prefixes, old-pointer hits");
}
