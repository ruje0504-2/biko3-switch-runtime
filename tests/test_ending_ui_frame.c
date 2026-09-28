#include "scene/ending_ui_frame.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
typedef struct {
  BkEndingUi ui;
  BkEndingStageUi stage;
  BkEndingUiController controller;
  BkEndingFrameState f;
  BkEndingControlState c;
  BkEndingAuxiliaryState a;
  BkCommonHudState common;
  BkEndingUiNoticeState notices;
  BkEndingUiFrameBindings bindings;
  BkEndingUiFrameOps ops;
  int32_t open, selected, next_mode;
  int8_t previous, final;
  uint8_t saved[4], flash;
  float gauge;
  unsigned calls, fail_at, key_calls;
  unsigned events[16];
} Fixture;
static int event(Fixture *s, unsigned code) {
  assert(s->calls < 16);
  s->events[s->calls++] = code;
  return s->calls != s->fail_at;
}
static int position(void *p, float out[2], char e[256]) {
  (void)e;
  Fixture *s = p;
  assert(s->controller.hints.once_flags & 1);
  out[0] = 1200;
  out[1] = 100;
  return event(s, 1);
}
static int motion(void *p, float out[2], char e[256]) {
  (void)e;
  out[0] = out[1] = 0;
  return event(p, 2);
}
static int key(void *p, unsigned code, unsigned mode, uint32_t *out,
               char e[256]) {
  (void)e;
  Fixture *s = p;
  assert(code < 2 && mode == 2);
  s->key_calls++;
  *out = 0;
  return event(s, 3 + code);
}
static int leave(void *p, BkEndingLeave k, char e[256]) {
  (void)e;
  return event(p, 10 + k);
}
static int schedule(void *p, uint8_t target, uint8_t mode, char e[256]) {
  (void)e;
  assert(target == 8 && mode == 0);
  return event(p, 20);
}
static void init(Fixture *s) {
  memset(s, 0, sizeof(*s));
  char e[256];
  s->f.phase = 9;
  s->f.curtain_wanted = 255; /* deliberately stale diagnostic alias */
  s->final = 9;
  s->flash = 1;
  s->previous = 8;
  assert(bk_ending_ui_initialize(&s->ui, 1280, s->c.pause_flags, &s->gauge, e));
  bk_common_hud_initialize(&s->common);
  s->common.curtain = (BkFadeSprite){0, 2, 0};
  s->bindings = (BkEndingUiFrameBindings){
      .common = &s->common,
      .toolbar = {&s->f, &s->c, &s->a, &s->open},
      .hints = {.frame = &s->f,
                .control = &s->c,
                .auxiliary = &s->a,
                .gauge_y = &s->gauge},
      .select = {.frame = &s->f,
                 .control = &s->c,
                 .auxiliary = &s->a,
                 .open = &s->open},
      .cursor = {.frame = &s->f, .auxiliary = &s->a, .notices = &s->notices},
      .reload = {&s->f, &s->c, &s->a, &s->previous, &s->common.action,
                 &s->common.blocked, &s->selected, &s->next_mode, s->saved},
      .tail = {.frame = &s->f,
               .control = &s->c,
               .auxiliary = &s->a,
               .notices = &s->notices,
               .flash_wanted = &s->flash,
               .final_state = &s->final,
               .gauge_y = &s->gauge}};
  s->ops = (BkEndingUiFrameOps){
      .context = s,
      .position = position,
      .motion = motion,
      .key = key,
      .reload = {.context = s, .leave = leave, .schedule = schedule}};
}
static int step(Fixture *s, BkEndingUiCompositeFrame *out, char e[256]) {
  return bk_ending_ui_frame(&s->ui, &s->stage, &s->controller, &s->bindings,
                            &s->ops, 1, .1f, out, e);
}
int main(void) {
  Fixture s;
  BkEndingUiCompositeFrame out;
  char e[256];
  init(&s);
  assert(step(&s, &out, e) && out.complete && !out.early_return);
  assert(s.calls == 2 && out.curtain_after == out.sprites.count);
  /* action49 exits before tail, even when required tail fields are absent. */
  unsigned total = 0;
  for (unsigned failure = 0; !failure || failure <= total; failure++) {
    init(&s);
    s.f.phase = 8;
    s.common.action = 49;
    s.common.blocked = 1;
    s.common.curtain = (BkFadeSprite){1, 2, 3};
    s.bindings.tail.final_state = NULL;
    s.fail_at = failure;
    int ok = step(&s, &out, e);
    assert(ok == !failure && out.complete == !failure);
    if (!failure)
      total = s.calls;
    assert(s.calls == (failure ? failure : total));
    if (!failure || failure > total - 7)
      assert(out.curtain_after == out.sprites.count);
    if (!failure)
      assert(out.early_return && s.c.variant == 255);
    if (failure > total - 7)
      assert(!s.common.blocked); /* retained reload prefix */
  }
  init(&s);
  s.ops.motion = NULL;
  assert(!step(&s, &out, e) && !out.complete && s.calls == 1);
  init(&s);
  BkEndingFrameState copied = s.f;
  s.bindings.select.frame = &copied;
  assert(!step(&s, &out, e) && !out.complete && !s.calls);
  init(&s);
  s.bindings.reload.action = &s.f.transition_action;
  assert(!step(&s, &out, e) && !s.calls);
  /* Shared black-curtain owner controls toolbar, not the stale frame byte. */
  init(&s);
  s.f.phase = 7;
  s.f.camera_manual = 1;
  assert(bk_ending_stage_ui_initialize(&s.ui, &s.stage, BK_ENDING_UI_FINAL, 0,
                                       0, 1280, e));
  assert(step(&s, &out, e) && s.key_calls > 0);
  assert(out.sprites.draws[0].slot == 74);
  init(&s);
  s.f.phase = 7;
  assert(bk_ending_stage_ui_initialize(&s.ui, &s.stage, BK_ENDING_UI_FINAL, 0,
                                       0, 1280, e));
  assert(step(&s, &out, e));
  for (unsigned i = 0; i < 9; i++)
    assert(s.ui.sprites[i].transform.fade.stage != 1);
  init(&s);
  s.f.phase = 0;
  assert(!step(&s, &out, e) && !out.complete);
  for (unsigned blocked = 0; blocked < 2; blocked++) {
    init(&s);
    s.f.phase = 0; /* rejected later by selector; observe earlier hover gate */
    s.common.blocked = blocked;
    s.f.curtain_wanted = !blocked;
    assert(!step(&s, &out, e));
    assert((s.key_calls > 0) == !blocked);
  }
  init(&s);
  s.f.phase = 7;
  s.fail_at = 1;
  assert(bk_ending_stage_ui_initialize(&s.ui, &s.stage, BK_ENDING_UI_FINAL, 0,
                                       0, 1280, e));
  assert(!step(&s, &out, e) && !out.complete && out.sprites.count == 1);
  assert(out.sprites.draws[0].slot == 74 && s.controller.hints.once_flags == 1);
  init(&s);
  s.common.curtain.alpha = NAN;
  assert(!step(&s, &out, e) && !out.complete && out.sprites.count > 0);
  puts("ending UI whole-frame ownership/failure/phase7 policy PASS");
}
