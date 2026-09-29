#include "game/ending_auxiliary_presentation.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

typedef struct {
  unsigned clock, advances, plains, active, hides, materials, publishes;
  unsigned expressions, eyes, gazes, blinks, levels, mouths;
  BkEndingAuxiliaryPresentationActor actors[8];
  float seconds[8];
  BkClipPlainMode plain_mode;
  int32_t active_clip;
} Trace;

static int fail(char e[256], const char *why) {
  snprintf(e, 256, "test auxiliary presentation: %s", why);
  return 0;
}
#define CHECK(x) do { if (!(x)) return fail(error, #x); } while (0)
static int clock_read(void *p, uint32_t *out, char e[256]) {
  Trace *t = p; (void)e; t->clock++; *out = 1234; return 1;
}
static int advance(void *p, BkEndingAuxiliaryPresentationActor actor,
                   float seconds, char e[256]) {
  Trace *t = p; (void)e;
  if (t->advances >= 8) return 0;
  t->actors[t->advances] = actor; t->seconds[t->advances++] = seconds;
  return 1;
}
static int plain(void *p, BkEndingAuxiliaryPresentationActor actor,
                 float seconds, BkClipPlainMode mode, char e[256]) {
  Trace *t = p; (void)e;
  t->plains++; t->actors[7] = actor; t->seconds[7] = seconds;
  t->plain_mode = mode; return 1;
}
static int active(void *p, BkEndingAuxiliaryPresentationActor actor,
                  int32_t *out, char e[256]) {
  Trace *t = p; (void)actor; (void)e; t->active++; *out = t->active_clip; return 1;
}
static int hide(void *p, uint32_t node, uint32_t hidden, char e[256]) {
  Trace *t = p; (void)node; (void)hidden; (void)e; t->hides++; return 1;
}
static int material(void *p, const char *name, uint32_t hidden, float alpha,
                    char e[256]) {
  Trace *t = p; (void)name; (void)hidden; (void)alpha; (void)e;
  t->materials++; return 1;
}
static int publish(void *p, char e[256]) {
  Trace *t = p; (void)e; t->publishes++; return 1;
}
static int expression(void *p, int32_t value, char e[256]) {
  Trace *t = p; (void)value; (void)e; t->expressions++; return 1;
}
static int eye_range(void *p, float minimum, float maximum, char e[256]) {
  Trace *t = p; (void)minimum; (void)maximum; (void)e; t->eyes++; return 1;
}
static int gaze(void *p, float minimum, float maximum, char e[256]) {
  Trace *t = p; (void)minimum; (void)maximum; (void)e; t->gazes++; return 1;
}
static int blink(void *p, uint32_t timestamp, char e[256]) {
  Trace *t = p; (void)timestamp; (void)e; t->blinks++; return 1;
}
static int level(void *p, float *out, char e[256]) {
  Trace *t = p; (void)e; t->levels++; *out = 4; return 1;
}
static int mouth(void *p, float value, uint32_t timestamp, char e[256]) {
  Trace *t = p; (void)value; (void)timestamp; (void)e; t->mouths++; return 1;
}

int main(void) {
  char error[256] = {0};
  BkEndingFrameState frame = {.group = 0};
  BkEndingControlState control = {.state_721eec = 3};
  BkEndingAuxiliaryState auxiliary = {.expression_a = 6, .expression_b = 3};
  BkFaceState face;
  if (!bk_face_init(&face, 1, 1, error)) return 1;
  int32_t face_mode = 0, expression_override = 5, eye_lower = 0, latch = 0;
  uint8_t toggles[8] = {0};
  uint32_t roots[2] = {10, 20}, hidden[3] = {30, 0, 31}, secondary = 0;
  Trace trace = {.active_clip = 4};
  BkEndingAuxiliaryPresentationBindings b = {
      &frame, &control, &auxiliary, &face, &face_mode,
      &expression_override, &eye_lower, &latch, toggles, &roots[0], &roots[1],
      hidden, &secondary};
  BkEndingAuxiliaryPresentationOps ops = {
      &trace, clock_read, advance, plain, active, hide, material, publish,
      expression, eye_range, gaze, blink, level, mouth};
  if (!bk_ending_auxiliary_presentation_step(&b, 1.f, &ops, error)) {
    fprintf(stderr, "%s\n", error); return 1;
  }
  CHECK(trace.clock == 1 && trace.active == 1);
  CHECK(trace.advances == 2 && trace.actors[0] == BK_ENDING_AUX_PRESENT_BACKGROUND);
  CHECK(trace.actors[1] == BK_ENDING_AUX_PRESENT_PRIMARY &&
        fabsf(trace.seconds[1] - .3f) < 1e-6f);
  CHECK(trace.plains == 0 && trace.hides == 2 && trace.materials == 4);
  CHECK(trace.publishes == 1 && trace.expressions == 1 && trace.eyes == 1 &&
        trace.blinks == 1 && trace.levels == 1 && trace.mouths == 1);

  memset(&trace, 0, sizeof(trace)); trace.active_clip = 5;
  if (!bk_ending_auxiliary_presentation_step(&b, .5f, &ops, error)) {
    fprintf(stderr, "%s\n", error); return 1;
  }
  CHECK(trace.plains == 1 && trace.advances == 1 &&
        trace.plain_mode == BK_CLIP_PLAIN_SCHEDULED &&
        trace.seconds[7] == 0);

  memset(&trace, 0, sizeof(trace));
  frame.group = 2; control.state_721eec = 7; secondary = 77;
  trace.active_clip = 1;
  if (!bk_ending_auxiliary_presentation_step(&b, .5f, &ops, error)) {
    fprintf(stderr, "%s\n", error); return 1;
  }
  CHECK(trace.advances == 2 && fabsf(trace.seconds[1] - .15f) < 1e-6f &&
        trace.hides == 3);
  printf("PASS auxiliary presentation: exact 48181F rate/order branches\n");
  return 0;
}
