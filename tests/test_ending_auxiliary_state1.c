#include "game/ending_auxiliary_state1.h"
#include <stdio.h>
#include <string.h>

typedef struct {
  int confirm_pressed, idle_a, idle_b, picked, zone;
  unsigned calls[32], count;
  int32_t random_value;
} Trace;

static int key(void *context, unsigned code, unsigned mode, uint32_t *out,
               char e[256]) {
  Trace *t = context;
  (void)e;
  t->calls[t->count++] = 100 + code + mode * 16;
  *out = mode == 1 && code == 0 ? (uint32_t)t->confirm_pressed
                                : mode == 0 && code == 0 ? (uint32_t)t->idle_a
                                : mode == 0 && code == 1 ? (uint32_t)t->idle_b
                                : 0;
  return 1;
}
static int present(void *context, unsigned owner, int *out, char e[256]) {
  Trace *t = context; (void)e;
  t->calls[t->count++] = 10 + owner; *out = 0; return 1;
}
static int status(void *context, unsigned owner, int *out, char e[256]) {
  Trace *t = context; (void)e; t->calls[t->count++] = 20 + owner;
  *out = 0; return 1;
}
static int audio(void *context, const BkEndingAudioCall *call, int *out,
                 char e[256]) {
  Trace *t = context; (void)e;
  t->calls[t->count++] = call->operation == BK_ENDING_AUDIO_RESTART ? 30 : 31;
  *out = 0; return 1;
}
static int random_value(void *context, int32_t *out, char e[256]) {
  Trace *t = context; (void)e; t->calls[t->count++] = 40;
  *out = t->random_value; return 1;
}
static int pick(void *context, const float pointer[2], int32_t preferred,
                int32_t *out, char e[256]) {
  Trace *t = context; (void)pointer; (void)preferred; (void)e;
  t->calls[t->count++] = 50; *out = t->picked; return 1;
}
static int menu(void *context, int32_t selected, int32_t *out, char e[256]) {
  Trace *t = context; (void)selected; (void)e;
  t->calls[t->count++] = 60; *out = t->zone; return 1;
}
static int voice(void *context, int32_t cue, unsigned slot, int32_t flags,
                 int32_t volume, char e[256]) {
  Trace *t = context; (void)e;
  t->calls[t->count++] = 70 + (unsigned)cue + slot + (unsigned)flags +
                         (unsigned)volume * 0;
  return 1;
}
static int request(void *context, unsigned slot, char e[256]) {
  Trace *t = context; (void)e; t->calls[t->count++] = 80 + slot; return 1;
}
static int expression(void *context, int32_t a, int32_t b, unsigned eye,
                      char e[256]) {
  Trace *t = context; (void)e;
  if (a != 5 || b != 4 || eye != 0) return 0;
  t->calls[t->count++] = 90; return 1;
}

static int check(int condition, const char *what, const char *error) {
  if (condition) return 1;
  fprintf(stderr, "FAIL %s: %s\n", what, error);
  return 0;
}

int main(void) {
  Trace trace = {.confirm_pressed = 1, .picked = 1, .zone = 3,
                 .random_value = 7};
  BkEndingFrameState frame = {.camera_mode = 0, .camera_cached = 2};
  BkEndingControlState control = {.state_721eec = 1};
  BkEndingAuxiliaryState auxiliary = {0};
  float timer = 0;
  int32_t delay = 30;
  BkEndingAuxiliaryState1Bindings bindings = {
      &frame, &control, &auxiliary, &timer, &delay};
  BkEndingAuxiliaryState1Ops ops = {
      &trace, key, present, status, audio, random_value, pick, menu, voice,
      request, expression};
  float pointer[2] = {100, 200};
  char error[256] = {0};

  if (!check(bk_ending_auxiliary_state1_step(&bindings, pointer, 0, -200,
                                              &ops, error),
             "selection", error) ||
      !check(control.state_721eec == 3 && auxiliary.pending == 2 &&
                 frame.camera_mode == 4 && frame.camera_event == 1,
             "selection state", error) ||
      !check(trace.count == 7, "selection order", error))
    return 1;

  control.state_721eec = 1;
  auxiliary.pending = 1;
  timer = 29;
  delay = 30;
  trace.count = 0;
  trace.confirm_pressed = 0;
  if (!check(bk_ending_auxiliary_state1_step(&bindings, pointer, 1, -200,
                                              &ops, error),
             "timer", error) ||
      !check(timer == 0 && delay == 37, "timer random delay", error) ||
      !check(trace.calls[0] == 30 && trace.calls[1] == 40,
             "timer service order", error))
    return 1;

  control.state_721eec = 2;
  trace.idle_a = trace.idle_b = 1;
  trace.count = 0;
  if (!check(bk_ending_auxiliary_state2_step(&control, &ops, error),
             "state2", error) || !check(control.state_721eec == 1,
                                          "state2 transition", error))
    return 1;
  puts("PASS auxiliary state1: 47DC79 pointer/menu and state2 wait");
  return 0;
}
