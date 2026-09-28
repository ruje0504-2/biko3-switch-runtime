#include "scene/failure_hud.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <math.h>
#include <string.h>
typedef struct {
  unsigned calls, fail_at;
} Context;
static int result(Context *c) { return ++c->calls != c->fail_at; }
static int click(void *c, char *e) {
  (void)e;
  return result(c);
}
static int message(void *c, uint32_t label, char *e) {
  (void)e;
  assert(label == 30401);
  return result(c);
}
static int release(void *c, uint8_t flow, char *e) {
  (void)e;
  assert(flow == 0x40 || flow == 2);
  return result(c);
}
static int schedule(void *c, uint8_t target, uint8_t mode, char *e) {
  (void)e;
  assert(target == 0x68 && mode == 2);
  return result(c);
}
int main(void) {
  char e[256] = {0};
  BkFailureHudState s = {0, 1};
  BkCommonHudState common = {.curtain = {0, 2, 0}};
  BkFadeSprite panel = {0, 2, 0}, prompt = {0, 2, 0};
  BkTextFlow text = {12, 20, 30, 0, 1};
  uint32_t group = 2, duration = 4000;
  uint8_t outcome = 6, overlay = 1, visible = 1;
  int8_t script = 3, mode = 4, stimulus = 5;
  int32_t behavior = 9;
  BkFailureHudBindings b = {&common,   &panel,    &prompt,  &text,   &group,
                            &outcome,  &overlay,  &visible, &script, &mode,
                            &stimulus, &behavior, &duration};
  Context c = {0};
  BkFailureHudOps ops = {&c, click, message, release, schedule};
  BkFailureHudFrame f = {0};
  /* Message failure preserves already executed click, but cannot reset text
   * or increment the count. No later draws/releases are reached. */
  c.fail_at = 2;
  assert(!bk_failure_hud_step(&s, &b, &ops, 1, .1f, &f, e));
  assert(c.calls == 2 && s.advances == 0 && text.started == 1 && !text.enabled);
  assert(common.blocked == 0 && panel.stage == 0);
  c = (Context){0};
  assert(bk_failure_hud_step(&s, &b, &ops, 1, .1f, &f, e));
  assert(c.calls == 2 && s.advances == 1 && text.started == 0 &&
         text.enabled == 1);
  assert(text.scroll == 20 && text.target == 30);
  assert(f.panel_alpha == 0 && f.prompt_alpha > 0);
  assert(panel.stage == 1 && prompt.stage == 1 && !f.draw_text);
  c = (Context){0};
  assert(bk_failure_hud_step(&s, &b, &ops, 1, .1f, &f, e));
  assert(c.calls == 1 && s.advances == 2 && common.blocked == 1 && !overlay);
  /* Release failure occurs after the blackout clears blocked, before resets.
   * The caller terminates rather than retrying this partially released frame.
   */
  common.curtain = (BkFadeSprite){1, 2, 3};
  c = (Context){.fail_at = 2};
  assert(!bk_failure_hud_step(&s, &b, &ops, 0, .1f, &f, e));
  assert(c.calls == 2 && !common.blocked && outcome == 6 && s.advances == 2);
  assert(script == 3 && mode == 4 && behavior == 9 && duration == 4000);
  common.blocked = 1;
  c = (Context){0};
  assert(bk_failure_hud_step(&s, &b, &ops, 0, .1f, &f, e));
  assert(c.calls == 3 && !outcome && !s.advances && !s.visible);
  assert(!script && !mode && !visible && !stimulus && behavior == 1 &&
         duration == 3000);
  BkFailureHudState before = s;
  BkCommonHudState saved = common;
  c = (Context){0};
  assert(!bk_failure_hud_step(&s, &b, &ops, 1, NAN, &f, e));
  assert(!c.calls && !memcmp(&s, &before, sizeof(s)) &&
         !memcmp(&common, &saved, sizeof(common)));
  outcome = 6;
  group = 5;
  assert(!bk_failure_hud_step(&s, &b, &ops, 1, .1f, &f, e));
  assert(!c.calls && !s.advances);
  /* Other unknown native outcome bytes use the ordinary confirmation path. */
  outcome = 255;
  assert(bk_failure_hud_step(&s, &b, &ops, 1, 0, &f, e));
  assert(c.calls == 4 && outcome == 0);
  return 0;
}
