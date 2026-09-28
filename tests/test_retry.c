#include "scene/retry.h"
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
int main(void) {
  char e[256];
  Services svc = {0};
  BkPauseOps ops = {.context = &svc,
                    .sound = sound,
                    .warp = warp,
                    .pointer = pointer,
                    .release = release};
  BkRetryState s = {.selected = 1, .phase = 2};
  assert(bk_retry_initialize(&s, 640, &ops, e));
  assert(s.selected == 1 && s.phase == 2 && s.loaded);
  assert(svc.point[0] == 248 && svc.point[1] == 274);
  BkMenuCursor cursor = {0};
  assert(bk_menu_cursor_initialize(&cursor, 640, 480));
  BkCommonHudState c = {.curtain = {1, 2, 3}};
  BkFlowTransition flow = {0x68, 0x40, 0, 0};
  uint8_t overlay = 0, latch = 0;
  BkPauseBindings b = {&c, &flow, &cursor, &overlay, &latch};
  BkPauseInput in = {
      .scale = .5f, .seconds = .25f, .buttons = BK_PAUSE_CONFIRM};
  BkPauseFrame frame;
  BkRetryState start = s;
  /* Unlike pause, retry tests this frame's pointer before sounds/drawing.
   * Transition can fail after the prepared snapshot and hover side effects. */
  const unsigned expected[] = {300, 100, 103, 504};
  for (unsigned fail_at = 1; fail_at <= 4; ++fail_at) {
    s = start;
    flow = (BkFlowTransition){0x68, 0x40, 0, 0};
    c = (BkCommonHudState){.curtain = {1, 2, 3}};
    latch = 0;
    svc = (Services){.fail_at = fail_at, .point = {248, 274}};
    assert(!bk_retry_step(&s, &b, &in, &ops, &frame, e));
    assert(svc.calls == fail_at &&
           !memcmp(svc.events, expected, fail_at * sizeof(unsigned)));
    assert(s.loaded && flow.current == 0x68 && s.phase == 2);
    assert(c.action == (fail_at <= 2 ? 0 : 2));
    assert(c.blocked == (fail_at == 3));
    assert(latch == (fail_at == 4));
  }
  s = start;
  flow = (BkFlowTransition){0x68, 0x40, 0, 0};
  c = (BkCommonHudState){.curtain = {1, 2, 3}};
  latch = 0;
  svc = (Services){.point = {248, 274}};
  assert(bk_retry_step(&s, &b, &in, &ops, &frame, e));
  assert(frame.count == 6 && frame.draws[2].slot == 3 &&
         frame.draws[3].slot == 4);
  assert(!s.loaded && s.phase == 0 && s.selected == 0 && flow.current == 0x50 &&
         flow.target == 2 && flow.mode == 1);
  /* Rejected basic input must not change live state or call any service. */
  s = start;
  flow.current = 0x68;
  in.seconds = NAN;
  svc.calls = 0;
  assert(!bk_retry_step(&s, &b, &in, &ops, &frame, e));
  assert(!memcmp(&s, &start, sizeof(s)) && svc.calls == 0);
  /* Guard the original unchecked keyboard table index. */
  in.seconds = 0;
  in.buttons = BK_PAUSE_RIGHT;
  s.selected = INT32_MAX;
  s.phase = 0;
  assert(!bk_retry_step(&s, &b, &in, &ops, &frame, e));
  puts("PASS retry fresh-pointer hits, callback prefixes, last snapshot and "
       "invalid input");
}
