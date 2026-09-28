#include "scene/dialogue_ui.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
typedef struct {
  unsigned calls, fail_at;
  int fail;
  BkDialogue *d;
} Service;
static int op(void *p, char e[256]) {
  Service *s = p;
  s->calls++;
  int failed = s->fail || s->calls == s->fail_at;
  if (failed)
    snprintf(e, 256, "injected service failure");
  return !failed;
}
static int next(void *p, int *done, char e[256]) {
  Service *s = p;
  if (!op(p, e))
    return 0;
  *done = 1;
  s->d->image_kind = 2;
  s->d->code_f = 34;
  return 1;
}
static int text(void *p, float dt, char e[256]) {
  (void)dt;
  return op(p, e);
}
static int unlock(void *p, unsigned group, char e[256]) {
  (void)group;
  return op(p, e);
}
static int schedule(void *p, uint8_t flow, uint8_t mode, char e[256]) {
  (void)flow;
  (void)mode;
  return op(p, e);
}
int main(void) {
  char e[256];
  BkDialogueUi s = {0};
  s.prompt.direction = 255;
  bk_dialogue_ui_initialize(&s);
  assert(s.columns == 27 && s.rows == 4 && s.step_y == 16 &&
         s.prompt.direction == 255);
  BkDialogue d = {0};
  BkTextFlow flow = {0, 12, 34, 0, 1};
  BkDialogueBackdrop bg = {0};
  uint8_t phase = 0, wanted = 0;
  BkFadeSprite curtain = {0, 2, 0};
  BkDialogueResult result = {17, 19, 23};
  BkDialogueUiBindings b = {&d, &flow, &bg, &phase, &curtain, &wanted, &result};
  BkDialogueUiInput in = {
      .seconds = .1f, .advance = 1, .previous = 0x38, .group = 0};
  Service service = {.fail = 1, .d = &d};
  BkDialogueUiOps ops = {&service, op, next,   op,      text,
                         op,       op, unlock, schedule};
  BkDialogueUiFrame out = {7, 8, 9};
  BkDialogueUi saved = s;
  BkTextFlow old = flow;
  in.seconds = NAN;
  assert(!bk_dialogue_ui_step(&s, &b, &in, &ops, &out, e));
  assert(!memcmp(&saved, &s, sizeof(s)) && !memcmp(&old, &flow, sizeof(flow)) &&
         service.calls == 0 && out.panel_alpha == 7);
  in.seconds = .1f;
  /* Fast-forward commits before text service failure; no parser advance. */
  assert(!bk_dialogue_ui_step(&s, &b, &in, &ops, &out, e));
  assert(flow.scroll == 34 && flow.enabled == 1 && flow.started == 0 &&
         d.image_kind == 0 && service.calls == 1);
  service.fail = 0;
  assert(bk_dialogue_ui_step(&s, &b, &in, &ops, &out, e));
  assert(wanted == 1 && phase == 1 && bg.image_kind == 1 &&
         bg.saved_expression == 34 && d.image_kind == 0 && flow.delay == 2 &&
         flow.scroll == 0);
  /* Unlock service is required before transition scheduling, after the
   * original close-latch clear. It cannot silently succeed on failure. */
  service = (Service){.fail = 1, .d = &d};
  in.advance = 0;
  in.previous = 0x10;
  curtain = (BkFadeSprite){1, 2, 3};
  wanted = 1;
  BkDialogueUiOps no_unlock = ops;
  no_unlock.unlock = NULL;
  saved = s;
  old = flow;
  assert(!bk_dialogue_ui_step(&s, &b, &in, &no_unlock, &out, e));
  assert(!memcmp(&saved, &s, sizeof(s)) && !memcmp(&old, &flow, sizeof(flow)) &&
         wanted == 1 && service.calls == 0);
  /* Hold first three normal services successful, fail specifically unlock. */
  service.fail = 0;
  service.fail_at = 4;
  assert(!bk_dialogue_ui_step(&s, &b, &in, &ops, &out, e));
  assert(service.calls == 4 && wanted == 0 && result.group == 17 &&
         result.kind == 19 && result.choice == 23);
  float target = 19;
  d.text.length = 1025;
  assert(!bk_dialogue_text_target(&d.text, 27, 4, 16, 0, &target, e) &&
         target == 19);
  d.text.length = 3;
  memcpy(d.text.bytes, "\r\nA", 3);
  assert(!bk_dialogue_text_target(&d.text, 0, 4, 16, 0, &target, e) &&
         target == 19);
  puts("PASS dialogue UI: input atomicity, fast-forward/failure order, real "
       "service requirements");
}
