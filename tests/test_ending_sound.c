#include "game/ending_sound.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <math.h>
#include <stdio.h>
typedef struct {
  int32_t *transition;
  unsigned calls, reads, fail_at;
  int playing[2];
  int32_t volumes[2], written;
} Fixture;
static int tick(Fixture *f, char e[256]) {
  if (++f->calls != f->fail_at)
    return 1;
  snprintf(e, 256, "injected failure");
  return 0;
}
static int status(void *p, unsigned slot, int *playing, char e[256]) {
  Fixture *f = p;
  if (!tick(f, e))
    return 0;
  *playing = f->playing[slot];
  return 1;
}
static int volume(void *p, int32_t *out, char e[256]) {
  Fixture *f = p;
  if (!tick(f, e))
    return 0;
  assert(f->reads < 2);
  *out = f->volumes[f->reads++];
  return 1;
}
static int gain(void *p, int32_t value, char e[256]) {
  Fixture *f = p;
  assert(*f->transition == 1);
  if (!tick(f, e))
    return 0;
  f->written = value;
  return 1;
}
int main(void) {
  char e[256];
  int32_t transition = 0, result = -77;
  Fixture f = {
      .transition = &transition, .playing = {1, 1}, .volumes = {-500, -750}};
  BkEndingDuckOps ops = {&f, status, volume, gain};
  BkEndingDuckInput in = {1, -500, .125f, {1, 1}};
  assert(bk_ending_sound_duck(&transition, &in, &ops, &result, e));
  assert(f.calls == 5 && f.reads == 2 && f.written == -1000 &&
         transition == 1 && result == 0);
  for (unsigned fail = 1; fail <= 5; fail++) {
    transition = 0;
    result = -77;
    f = (Fixture){.transition = &transition,
                  .fail_at = fail,
                  .playing = {1, 1},
                  .volumes = {-500, -750}};
    assert(!bk_ending_sound_duck(&transition, &in, &ops, &result, e));
    assert(f.calls == fail && transition == (fail >= 4) && result == -77);
  }
  f = (Fixture){
      .transition = &transition, .playing = {1, 1}, .volumes = {-500, -750}};
  in.seconds = 2;
  transition = 0;
  assert(bk_ending_sound_duck(&transition, &in, &ops, &result, e));
  assert(f.written == -2500 && transition == 0 && result == 1);
  f.calls = 0;
  transition = 37;
  in.present[0] = 0;
  assert(bk_ending_sound_duck(&transition, &in, &ops, &result, e));
  assert(!f.calls && transition == 37 && result == 1);
  in.seconds = NAN;
  result = -77;
  assert(!bk_ending_sound_duck(&transition, &in, &ops, &result, e));
  assert(!f.calls && transition == 37 && result == -77);
  assert(!bk_ending_sound_music(5, 0) && !bk_ending_sound_music(0, 2));
  for (unsigned i = 0; i < BK_ENDING_EFFECTS; i++)
    assert((bk_ending_sound_effect(i) != NULL) == (i < 41));
  assert(!bk_ending_sound_effect(BK_ENDING_EFFECTS));
  puts("PASS ending sound: two live reads, retained latch, five failure "
       "prefixes, native completion and input bounds");
}
