#include "game/ending_control.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

typedef struct {
  uint32_t keys[3];
  unsigned calls, fail_at, sounds[4], key_calls;
} Fixture;

static int key(void *p, unsigned code, unsigned mode, uint32_t *out,
               char error[256]) {
  Fixture *f = p;
  assert(mode == 1);
  unsigned index = code == 0 ? 0 : code == 1 ? 1 : 2;
  assert(code == 0 || code == 1 || code == 0x5a || code == 0x33450);
  ++f->key_calls;
  if (++f->calls == f->fail_at) {
    snprintf(error, 256, "injected key failure");
    return 0;
  }
  *out = f->keys[index];
  return 1;
}

static int sound(void *p, unsigned slot, int32_t volume, char error[256]) {
  Fixture *f = p;
  assert(slot == 0 || slot == 2 || slot == 3);
  assert(volume == -700);
  if (++f->calls == f->fail_at) {
    snprintf(error, 256, "injected sound failure");
    return 0;
  }
  ++f->sounds[slot];
  return 1;
}

int main(void) {
  char error[256] = {0};
  BkEndingControlState s = {.hover = 77, .hover_armed = 1,
                             .previous_hover = 59, .previous_phase = 3,
                             .pause_selection = 45,
                             .pause_flags = {1, 2, 3, 4, 5, 6}};
  BkEndingFrameState frame = {.phase = 9};
  BkEndingControlRect rects[2] = {{0, 0, 10, 10}, {20, 0, 10, 10}};
  uint8_t action = 99, wanted = 1;
  int32_t volume = -700;
  BkEndingConfirmBindings b = {&frame, &action, &wanted, rects, &volume};
  BkEndingFrameInput input = {0};
  input.words[9] = input.words[10] = 10;
  Fixture f = {.keys = {1, 0, 0}};
  BkEndingControlOps ops = {&f, key, sound, NULL, NULL};
  assert(bk_ending_confirm_step(&s, &b, &input, &ops, error));
  assert(s.hover == 0 && s.hover_armed == 1 && s.previous_hover == 59 &&
         frame.phase == 9 && f.calls == 0 && action == 99);

  wanted = 0;
  assert(bk_ending_confirm_step(&s, &b, &input, &ops, error));
  assert(action == 45 && wanted == 1 && s.pause_selection == 0 &&
         frame.phase == 9 && f.sounds[0] == 1 && !f.sounds[3]);

  wanted = 0;
  s.pause_selection = 47;
  f = (Fixture){.keys = {256, 0, 0}};
  assert(bk_ending_confirm_step(&s, &b, &input, &ops, error));
  assert(wanted == 0 && s.pause_selection == 47 && f.key_calls == 6);

  /* Both regions are independently evaluated even after confirm requests
   * the curtain. Cancel retains the two unused flag bytes. */
  rects[1] = rects[0];
  f = (Fixture){.keys = {1, 1, 0}};
  assert(bk_ending_confirm_step(&s, &b, &input, &ops, error));
  assert(action == 47 && wanted == 1 && frame.phase == 3 &&
         !s.pause_selection && f.sounds[0] == 1 && f.sounds[2] == 1 &&
         f.sounds[3] == 1);
  assert(!memcmp(s.pause_flags, (uint8_t[6]){0, 0, 0, 4, 0, 6}, 6));

  wanted = 0;
  frame.phase = 9;
  input.words[9] = 100;
  f = (Fixture){.keys = {0, 1, 0}};
  assert(bk_ending_confirm_step(&s, &b, &input, &ops, error));
  assert(frame.phase == 3 && !s.hover && !s.hover_armed &&
         !s.previous_hover && f.key_calls == 1 && f.sounds[2] == 1);

  /* A failed required sound keeps selection and action, with only the
   * preceding hover write applied; it cannot report a completed cancel. */
  frame.phase = 9;
  s.pause_selection = 47;
  f = (Fixture){.keys = {0, 1, 0}, .fail_at = 2};
  assert(!bk_ending_confirm_step(&s, &b, &input, &ops, error));
  assert(frame.phase == 9 && s.pause_selection == 47 && !wanted &&
         f.sounds[2] == 0 && strstr(error, "injected sound"));

  rects[0].x = NAN;
  f = (Fixture){0};
  assert(!bk_ending_confirm_step(&s, &b, &input, &ops, error));
  assert(!f.calls && frame.phase == 9);
  rects[0] = (BkEndingControlRect){16777216, 0, 1, 10};
  rects[1].x = -100;
  input.words[9] = 16777217;
  f = (Fixture){.keys = {1, 0, 0}};
  assert(bk_ending_confirm_step(&s, &b, &input, &ops, error));
  assert(s.hover == 59 && wanted == 1 && action == 47);
  puts("ending confirm: blocked/confirm/cancel/overlap/AL/bounds/failure PASS");
  return 0;
}
