#include "scene/checkpoint_prompt.h"
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
  BkCheckpointPrompt s = {.selected = 1};
  assert(bk_checkpoint_prompt_initialize(&s, 640, &ops, e));
  assert(s.selected == 1 && s.loaded);
  assert(svc.point[0] == 248 && svc.point[1] == 274);
  BkMenuCursor cursor = {0};
  assert(bk_menu_cursor_initialize(&cursor, 640, 480));
  BkCommonHudState c = {.curtain = {1, 2, 3}};
  BkFlowTransition flow = {0x20, 2, 0, 0};
  uint8_t overlay = 0, latch = 0;
  BkPauseBindings b = {&c, &flow, &cursor, &overlay, &latch};
  BkPauseInput in = {
      .scale = .5f, .seconds = .25f, .buttons = BK_PAUSE_CONFIRM};
  BkPauseFrame frame;
  uint8_t phase = 99;
  float reserve = .3f;
  BkCheckpointPrompt start = s;
  /* Unlike pause, checkpoint prompt tests this frame's pointer before
   * sounds/drawing. Transition can fail after the prepared snapshot and hover
   * side effects. */
  const unsigned expected[] = {300, 100, 103, 432};
  for (unsigned fail_at = 1; fail_at <= 4; ++fail_at) {
    s = start;
    flow = (BkFlowTransition){0x20, 2, 0, 0};
    c = (BkCommonHudState){.curtain = {1, 2, 3}};
    latch = 0;
    svc = (Services){.fail_at = fail_at, .point = {248, 274}};
    assert(!bk_checkpoint_prompt_step(&s, &b, &phase, &reserve, &in, &ops,
                                      &frame, e));
    assert(svc.calls == fail_at &&
           !memcmp(svc.events, expected, fail_at * sizeof(unsigned)));
    assert(s.loaded && flow.current == 0x20);
    assert(c.action == (fail_at <= 2 ? 0 : 1));
    assert(c.blocked == (fail_at == 3));
    assert(latch == (fail_at == 4));
    assert(reserve == .3f && phase == (fail_at == 1 ? 99 : 2));
    phase = 99;
  }
  s = start;
  flow = (BkFlowTransition){0x20, 2, 0, 0};
  c = (BkCommonHudState){.curtain = {1, 2, 3}};
  latch = 0;
  svc = (Services){.point = {248, 274}};
  assert(bk_checkpoint_prompt_step(&s, &b, &phase, &reserve, &in, &ops, &frame,
                                   e));
  assert(frame.count == 5 && frame.draws[1].slot == 2 &&
         frame.draws[2].slot == 3);
  assert(!s.loaded && s.selected == 0 && flow.current == 0x50 &&
         flow.target == 0x28 && flow.mode == 0 && reserve == 1);
  /* Rejected basic input must not change live state or call any service. */
  s = start;
  flow.current = 0x20;
  in.seconds = NAN;
  svc.calls = 0;
  assert(!bk_checkpoint_prompt_step(&s, &b, &phase, &reserve, &in, &ops, &frame,
                                    e));
  assert(!memcmp(&s, &start, sizeof(s)) && svc.calls == 0);
  /* Guard the original unchecked keyboard table index. */
  in.seconds = 0;
  in.buttons = BK_PAUSE_RIGHT;
  s.selected = INT32_MAX;
  assert(!bk_checkpoint_prompt_step(&s, &b, &phase, &reserve, &in, &ops, &frame,
                                    e));
  puts("PASS checkpoint prompt fresh-pointer hits, callback prefixes, last "
       "snapshot and "
       "invalid input");
}
