#include "game/ending_entry.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <math.h>
#include <stdio.h>
typedef struct {
  BkEndingEntryBindings *b;
  unsigned calls, fail_at;
} Fixture;
static int tick(Fixture *f, char e[256]) {
  if (++f->calls != f->fail_at)
    return 1;
  snprintf(e, 256, "injected failure");
  return 0;
}
static int load(void *p, BkEndingLoader loader, int32_t arg, char e[256]) {
  Fixture *f = p;
  assert(loader <= BK_ENDING_LOAD_4D39E6);
  assert(arg == -1 ||
         (loader == BK_ENDING_LOAD_4D1025 && arg >= 0 && arg <= 2));
  if (!tick(f, e))
    return 0;
  f->b->frame->phase = 97;
  f->b->frame->group = 3;
  return 1;
}
static int clear(void *p, unsigned group, char e[256]) {
  Fixture *f = p;
  assert(group == 3 && f->b->frame->phase == 97);
  return tick(f, e);
}
static int prepare(void *p, char e[256]) {
  Fixture *f = p;
  if (!tick(f, e))
    return 0;
  f->b->auxiliary->variant = 1;
  return 1;
}
int main(void) {
  BkEndingFrameState frame = {.phase = 41, .group = 0};
  BkEndingControlState control = {.variant = 21};
  BkEndingAuxiliaryState auxiliary = {0};
  uint8_t a = 7, b = 8;
  int32_t group = -1;
  float gauge = 400;
  BkEndingEntryBindings bindings = {&frame, &control, &auxiliary, &a,
                                    &b,     &group,   &gauge};
  Fixture f = {.b = &bindings};
  BkEndingEntryOps ops = {&f, load, clear, prepare};
  char e[256];
  assert(!bk_ending_entry_dispatch(NULL, 8, 0, 1, &ops, e));
  BkEndingEntryOps empty = {0};
  assert(bk_ending_entry_dispatch(&bindings, 1, 0, 1, &empty, e));
  assert(frame.phase == 41 && control.variant == 21 && a == 7 && b == 8);
  assert(bk_ending_entry_dispatch(&bindings, 0x18, UINT32_MAX, 1, &empty, e));
  assert(a == 1 && b == 1 && group == 0 && frame.phase == 41);
  assert(!bk_ending_entry_dispatch(&bindings, 8, 0, 1, &empty, e));
  assert(frame.phase == 1 && control.variant == 0);
  assert(bk_ending_entry_dispatch(&bindings, 8, 0, 1, &ops, e));
  assert(f.calls == 2 && frame.phase == 97 && frame.group == 3);
  frame.phase = 41;
  frame.group = 0;
  auxiliary.variant = 0;
  f.calls = 0;
  f.fail_at = 2;
  assert(!bk_ending_entry_dispatch(&bindings, 0x18, 6, 1, &ops, e));
  assert(f.calls == 2 && control.variant == 5 && auxiliary.variant == 1 &&
         frame.phase == 41);
  f.calls = f.fail_at = 0;
  assert(bk_ending_entry_dispatch(&bindings, 0x18, 6, 1, &ops, e));
  assert(frame.phase == 8 && frame.state_721ee0 == 3);
  frame.group = 2;
  gauge = 400;
  f.calls = 0;
  assert(!bk_ending_entry_dispatch(&bindings, 0x18, 2, NAN, &ops, e));
  assert(!f.calls && gauge == 400);
  assert(bk_ending_entry_dispatch(&bindings, 0x18, 5, .75f, &ops, e));
  assert(gauge == 340 && auxiliary.progress == .4f && control.variant == 8);
  puts("PASS ending entry: retained state, loader order, live reload, failure "
       "prefixes and input bounds");
  return 0;
}
