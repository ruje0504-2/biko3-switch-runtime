#include "game/ending_reload.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct {
  BkEndingFrameState frame;
  BkEndingControlState control;
  BkEndingAuxiliaryState aux;
  int8_t previous;
  int32_t selected, next;
  uint8_t saved[4];
} State;
typedef struct {
  State state, before[160];
  unsigned calls, fail_at, bad_query;
  int voice;
} Fixture;
static int tick(Fixture *f, char e[256]) {
  assert(f->calls < 160);
  f->before[f->calls++] = f->state;
  if (f->calls != f->fail_at)
    return 1;
  snprintf(e, 256, "injected service failure");
  return 0;
}
static int load(void *p, BkEndingLoader kind, int32_t arg, char e[256]) {
  (void)arg;
  assert(kind <= BK_ENDING_LOAD_4D39E6);
  Fixture *f = p;
  if (!tick(f, e))
    return 0;
  memset(f->state.control.toggles, 0x35, 8);
  f->state.frame.state_721ee0 = -101;
  return 1;
}
static int release(void *p, BkEndingLoader kind, char e[256]) {
  assert(kind <= BK_ENDING_LOAD_4D39E6);
  return tick(p, e);
}
static int image(void *p, int create, unsigned group, char e[256]) {
  assert(!create || group < 5);
  return tick(p, e);
}
static int leave(void *p, BkEndingLeave kind, char e[256]) {
  assert(kind <= BK_ENDING_LEAVE_48D7F2);
  return tick(p, e);
}
static int light(void *p, BkEndingReloadLight kind, char e[256]) {
  assert(kind <= BK_ENDING_LIGHT_ENABLE);
  return tick(p, e);
}
static int schedule(void *p, uint8_t target, uint8_t mode, char e[256]) {
  assert(mode == 0 &&
         (target == 1 || target == 8 || target == 24 || target == 88));
  return tick(p, e);
}
static int present(void *p, unsigned slot, int *out, char e[256]) {
  Fixture *f = p;
  assert(slot < 47);
  if (!tick(f, e))
    return 0;
  *out = f->bad_query == 1 ? 2 : 1;
  return 1;
}
static int status(void *p, unsigned slot, int *out, char e[256]) {
  Fixture *f = p;
  assert(slot < 47);
  if (!tick(f, e))
    return 0;
  *out = f->bad_query == 2 ? -1 : (slot ? slot % 3 == 0 : f->voice);
  return 1;
}
static int pause(void *p, unsigned slot, char e[256]) {
  assert(slot >= 2 && slot < 47 && slot % 3 == 0);
  return tick(p, e);
}
static BkEndingReloadBindings bind(State *s) {
  return (BkEndingReloadBindings){&s->frame,
                                  &s->control,
                                  &s->aux,
                                  &s->previous,
                                  &s->frame.transition_action,
                                  &s->frame.curtain_wanted,
                                  &s->selected,
                                  &s->next,
                                  s->saved};
}
int main(void) {
  Fixture *f = calloc(1, sizeof(*f)), *ref = calloc(1, sizeof(*ref));
  assert(f && ref);
  BkEndingReloadOps ops = {f,     load,     release, image,  leave,
                           light, schedule, present, status, pause};
  char e[256];
  unsigned failures = 0, cases = 0;
  const unsigned actions[] = {7, 6, 63, 8, 74, 49, 45, 47, 255};
  for (unsigned k = 0; k < sizeof(actions) / sizeof(*actions); k++)
    for (unsigned variant = 0; variant < 2; variant++)
      for (unsigned gallery = 0; gallery < 2; gallery++) {
        memset(ref, 0, sizeof(*ref));
        ref->state.frame =
            (BkEndingFrameState){.phase = gallery ? 8 : 1,
                                 .group = 2,
                                 .state_721ee4 = 1000,
                                 .curtain_wanted = 1,
                                 .transition_action = actions[k]};
        ref->state.aux.variant = variant;
        ref->state.aux.selection = 3;
        ref->state.previous = gallery ? 24 : 8;
        ref->state.selected = -1;
        ref->state.next = variant;
        for (unsigned i = 0; i < 8; i++)
          ref->state.control.toggles[i] = (uint8_t)(20 + i);
        for (unsigned i = 0; i < 4; i++)
          ref->state.saved[i] = (uint8_t)(80 + i);
        State initial = ref->state;
        BkEndingReloadBindings b = bind(&ref->state);
        ops.context = ref;
        int early = -1;
        assert(bk_ending_reload_transition(&b, 3, &ops, &early, e));
        assert(early == (actions[k] == 49));
        assert(ref->state.frame.curtain_wanted == 0);
        for (unsigned j = 1; j <= ref->calls; j++) {
          memset(f, 0, sizeof(*f));
          f->state = initial;
          f->fail_at = j;
          b = bind(&f->state);
          ops.context = f;
          early = -1;
          assert(!bk_ending_reload_transition(&b, 3, &ops, &early, e));
          assert(f->calls == j && early == 0 &&
                 !memcmp(&f->state, &ref->before[j - 1], sizeof(State)));
          for (unsigned m = 0; m < j; m++)
            assert(!memcmp(&f->before[m], &ref->before[m], sizeof(State)));
          failures++;
        }
        cases++;
      }
  memset(f, 0, sizeof(*f));
  ops.context = f;
  BkEndingReloadBindings b = bind(&f->state);
  f->state.frame.transition_action = 63;
  f->state.frame.curtain_wanted = 1;
  f->voice = 1;
  int early = -1;
  assert(bk_ending_reload_transition(&b, 3, &ops, &early, e));
  assert(f->calls == 2 && f->state.frame.transition_action == 63 &&
         f->state.frame.curtain_wanted == 1);
  for (unsigned bad = 1; bad <= 2; bad++) {
    f->bad_query = bad;
    f->calls = 0;
    assert(!bk_ending_reload_release(&b, &ops, e));
    assert(f->calls == bad);
  }
  f->bad_query = 0;
  f->state.frame.phase = 7;
  f->state.frame.group = 5;
  f->calls = 0;
  assert(!bk_ending_reload_load(&b, &ops, e) && !f->calls);
  BkEndingReloadOps missing = {0};
  f->state.frame.phase = 1;
  assert(!bk_ending_reload_load(&b, &missing, e));
  assert(!bk_ending_reload_release(&b, &missing, e));
  f->state.frame.curtain_wanted = 0;
  assert(bk_ending_reload_transition(&b, 3, &missing, &early, e));
  f->state.frame.curtain_wanted = 1;
  assert(bk_ending_reload_transition(&b, 2, &missing, &early, e));
  assert(!bk_ending_reload_transition(&b, 3, &missing, &early, e));
  assert(!bk_ending_reload_transition(NULL, 3, &ops, &early, e));
  for (unsigned g = 0; g < 5; g++)
    assert(bk_ending_final_image(g));
  assert(!bk_ending_final_image(5));
  printf(
      "PASS ending reload: %u transition profiles, %u ordered service failure "
      "prefixes; voice wait, absent services, invalid queries/groups\n",
      cases, failures);
  free(ref);
  free(f);
  return 0;
}
