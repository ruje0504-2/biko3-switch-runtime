#include "game/ending_auxiliary.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
typedef struct {
  unsigned calls, fail_at;
  int32_t slot;
  int trace[64];
} Fixture;
static int call(Fixture *f, int code, char e[256]) {
  assert(f->calls < 64);
  f->trace[f->calls++] = code;
  if (f->calls != f->fail_at)
    return 1;
  snprintf(e, 256, "injected service failure");
  return 0;
}
static int active(void *p, int32_t *slot, char e[256]) {
  Fixture *f = p;
  if (!call(f, 1, e))
    return 0;
  *slot = f->slot;
  return 1;
}
static int write_clip(void *p, unsigned slot, BkEndingClipWrite kind,
                      int32_t value, char e[256]) {
  return call(p, 1000 + slot * 100 + kind * 20 + value, e);
}
static int request(void *p, unsigned slot, char e[256]) {
  Fixture *f = p;
  if (!call(f, 3000 + slot, e))
    return 0;
  f->slot = slot;
  return 1;
}
static int audio(void *p, const BkEndingAudioCall *c, int *playing,
                 char e[256]) {
  if (!call(p, 4000 + c->operation * 100 + c->slot, e))
    return 0;
  *playing = c->slot != 5; /* Stop live 2..4; restart idle 5. */
  return 1;
}
static int eyes(void *p, unsigned slot, char e[256]) {
  assert(slot == 1);
  return call(p, 5000 + slot, e);
}
int main(void) {
  char e[256] = {0};
  Fixture fixture = {.slot = 5};
  BkEndingAuxiliaryOps ops = {&fixture, active, write_clip,
                              request,  audio,  eyes};
  BkEndingAuxiliaryState state = {.gate = 1, .variant = 0, .progress = .7f};
  BkEndingFrameState frame = {.group = 4, .phase = 6, .auxiliary_mode = 77};
  int32_t result = 99;
  assert(!bk_ending_auxiliary_change(NULL, &frame, 0, -700, -500, &ops, &result,
                                     e));
  BkEndingAuxiliaryState old;
  /* Gates must not demand unused services or mutate state. */
  state.gate = 2;
  old = state;
  BkEndingAuxiliaryOps empty = {0};
  assert(
      bk_ending_auxiliary_change(&state, &frame, 0, 0, 0, &empty, &result, e));
  assert(result == 0 && !memcmp(&state, &old, sizeof(old)));
  state.gate = 1;
  assert(
      !bk_ending_auxiliary_change(&state, &frame, 0, 0, 0, &empty, &result, e));
  empty.active = active;
  empty.context = &fixture;
  for (fixture.slot = 1; fixture.slot <= 3; fixture.slot++) {
    assert(bk_ending_auxiliary_change(&state, &frame, 0, 0, 0, &empty, &result,
                                      e));
    assert(!result);
  }
  fixture.slot = 5;
  for (unsigned i = 0; i < 3; i++) {
    int32_t proposed = (int32_t[]){-1, 4, INT_MAX}[i];
    old = state;
    assert(bk_ending_auxiliary_change(&state, &frame, proposed, 0, 0, &empty,
                                      &result, e));
    assert(result == 1 && !memcmp(&state, &old, sizeof(old)));
  }
  assert(
      !bk_ending_auxiliary_change(&state, &frame, 0, 0, 0, &empty, &result, e));
  /* Every service failure terminates at its exact ordered prefix. */
  unsigned failures = 0;
  for (int proposed = 0; proposed < 4; proposed++) {
    Fixture reference = {.slot = 5};
    ops.context = &reference;
    state = (BkEndingAuxiliaryState){.gate = 1, .progress = .7f};
    assert(bk_ending_auxiliary_change(&state, &frame, proposed, -700, -500,
                                      &ops, &result, e));
    assert(result == 1 && frame.auxiliary_mode == 77);
    for (unsigned fail_at = 1; fail_at <= reference.calls; fail_at++) {
      fixture = (Fixture){.slot = 5, .fail_at = fail_at};
      ops.context = &fixture;
      state = (BkEndingAuxiliaryState){.gate = 1, .progress = .7f};
      result = 99;
      assert(!bk_ending_auxiliary_change(&state, &frame, proposed, -700, -500,
                                         &ops, &result, e));
      assert(!result && fixture.calls == fail_at && frame.auxiliary_mode == 77);
      assert(!memcmp(reference.trace, fixture.trace, fail_at * sizeof(int)));
      failures++;
    }
  }
  BkEndingAudioCall sound = {
      .operation = BK_ENDING_AUDIO_VOICE, .slot = 0, .cue = 5};
  char pack[16], name[32];
  assert(!bk_ending_audio_resource(0, 0, INT_MAX, &sound, pack, name, e));
  assert(!bk_ending_audio_resource(0, 0, INT_MIN, &sound, pack, name, e));
  assert(!bk_ending_audio_resource(5, 0, 0, &sound, pack, name, e));
  assert(!bk_ending_audio_resource(0, 2, 0, &sound, pack, name, e));
  sound.cue = 30;
  assert(bk_ending_audio_resource(0, 1, INT_MAX, &sound, pack, name, e));
  assert(!strcmp(pack, "bk3_02") && !strcmp(name, "se301.wav"));
  printf("PASS ending auxiliary: gates, required services, %u failure "
         "prefixes, sound bounds\n",
         failures);
  return 0;
}
