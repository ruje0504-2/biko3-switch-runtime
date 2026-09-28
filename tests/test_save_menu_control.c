#include "scene/save_menu_control.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
typedef struct {
  unsigned calls, fail_at, events[32];
  float point[2];
} Service;
static int event(void *p, unsigned code) {
  Service *s = p;
  assert(s->calls < 32);
  s->events[s->calls++] = code;
  return s->calls != s->fail_at;
}
static int sound(void *p, unsigned slot, char *e) {
  (void)e;
  return event(p, 100 + slot);
}
static int warp(void *p, float x, float y, char *e) {
  (void)e;
  Service *s = p;
  s->point[0] = x;
  s->point[1] = y;
  return event(p, 200);
}
static int pointer(void *p, float out[2], float motion[2], char *e) {
  (void)e;
  memcpy(out, ((Service *)p)->point, 8);
  motion[0] = motion[1] = 0;
  return event(p, 300);
}
static int release(void *p, uint8_t flow, char *e) {
  (void)e;
  return event(p, 400 + flow);
}
static int load(void *p, unsigned g, unsigned k, char *e) {
  (void)e;
  return event(p, 500 + g * 10 + k);
}
static int store(void *p, unsigned g, unsigned k, char *e) {
  (void)e;
  return event(p, 600 + g * 10 + k);
}
static int refresh(void *p, unsigned g, char *e) {
  (void)e;
  return event(p, 700 + g);
}
static int clear(void *p, char *e) {
  (void)e;
  return event(p, 800);
}
static int details(void *p, unsigned g, char *e) {
  (void)e;
  return event(p, 810 + g);
}
static int reset(void *p, char *e) {
  (void)e;
  return event(p, 820);
}
static int resume(void *p, char *e) {
  (void)e;
  return event(p, 830);
}
int main(void) {
  char e[256];
  Service svc = {0};
  BkSaveMenuOps o = {.menu = {.context = &svc,
                              .sound = sound,
                              .warp = warp,
                              .pointer = pointer,
                              .release = release},
                     .load = load,
                     .store = store,
                     .refresh_bank = refresh,
                     .clear_details = clear,
                     .load_details = details,
                     .reset_text = reset,
                     .resume_pause = resume};
  BkSaveMenuControl base = {.tab = 3,
                            .page = 1,
                            .hover = 21,
                            .back = {976, 880, 256, 56},
                            .yes = {368, 520, 256, 56},
                            .no = {656, 520, 256, 56}},
                    s;
  BkCommonHudState c = {0};
  BkFlowTransition flow = {0};
  uint8_t latch = 0;
  BkPauseBindings b = {.common = &c, .flow = &flow, .hover_latched = &latch};
  BkSaveMenuInput in = {.ui = {.buttons = BK_PAUSE_CONFIRM, .scale = 1}};
  const unsigned save_events[] = {300, 100, 200, 600, 700,
                                  701, 702, 703, 704, 103};
  const unsigned load_events[] = {300, 100, 500, 103, 402, 440};
  for (unsigned mode = 0; mode < 2; ++mode) {
    const unsigned *expect = mode ? save_events : load_events;
    unsigned count = mode ? 10 : 6;
    for (unsigned failure = 0; failure <= count; ++failure) {
      s = base;
      c = (BkCommonHudState){.curtain = {1, 2, 3}};
      flow = (BkFlowTransition){0x28, mode ? 0x20 : 4, 0, 0};
      latch = 0;
      svc = (Service){.fail_at = failure, .point = {496, 548}};
      assert(bk_save_menu_control(&s, &b, &in, &o, e) == !failure);
      assert(svc.calls == (failure ? failure : count));
      assert(!memcmp(svc.events, expect, svc.calls * sizeof(unsigned)));
      if (failure)
        assert(flow.current == 0x28);
      else if (mode) {
        assert(s.page == 0 && flow.current == 0x28 && !c.blocked);
      } else {
        assert(flow.current == 0x50 && flow.previous == 0x28 &&
               flow.target == 2 && flow.mode == 1 && s.skip_draw);
      }
      if (mode && failure && failure <= 9)
        assert(s.page == 1);
    }
  }
  /* Preflight errors have no service or state side effects. */
  s = base;
  flow = (BkFlowTransition){0x28, 1, 0, 0};
  svc = (Service){0};
  BkSaveMenuControl bad = s;
  bad.hover = 99;
  assert(!bk_save_menu_control(&bad, &b, &in, &o, e));
  assert(!svc.calls);
  bad = s;
  bad.column = -1;
  assert(!bk_save_menu_control(&bad, &b, &in, &o, e));
  bad = s;
  bad.no[2] = NAN;
  assert(!bk_save_menu_control(&bad, &b, &in, &o, e));
  BkSaveMenuOps missing = o;
  missing.store = NULL;
  assert(!bk_save_menu_control(&s, &b, &in, &missing, e));
  assert(!svc.calls && !memcmp(&s, &base, sizeof(s)));
  /* Native confirmation page defaults to no; only click/warp, not row index,
   * determines this frame's selection. */
  c = (BkCommonHudState){0};
  latch = 0;
  s = base;
  svc = (Service){.point = {788, 548}};
  assert(bk_save_menu_control(&s, &b, &in, &o, e));
  assert(s.page == 0 && flow.current == 0x28);
  assert(svc.events[1] == 102 && svc.events[2] == 200);
  puts("PASS save-menu control:18 service success/failure prefixes,invalid "
       "state rejection,cancel");
}
