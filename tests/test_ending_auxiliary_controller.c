#include "game/ending_auxiliary_controller.h"
#include <stdio.h>
#include <string.h>

typedef struct {
  unsigned calls[8];
  unsigned count;
  int playing[2];
  unsigned group, branch;
  int32_t expression[2];
  unsigned eye, request;
} Trace;

static int status(void *p, unsigned owner, int *playing, char e[256]) {
  Trace *t = p;
  if (owner >= 2 || !playing) {
    snprintf(e, 256, "bad status");
    return 0;
  }
  t->calls[t->count++] = 10 + owner;
  *playing = t->playing[owner];
  return 1;
}
static int request(void *p, unsigned slot, char e[256]) {
  (void)e;
  Trace *t = p;
  t->calls[t->count++] = 20 + slot;
  t->request = slot;
  return 1;
}
static int expression(void *p, int32_t a, int32_t b, unsigned eye,
                      char e[256]) {
  (void)e;
  Trace *t = p;
  t->calls[t->count++] = 30;
  t->expression[0] = a;
  t->expression[1] = b;
  t->eye = eye;
  return 1;
}
static int group_sound(void *p, unsigned group, char e[256]) {
  (void)e;
  Trace *t = p;
  t->calls[t->count++] = 40;
  t->group = group;
  return 1;
}
static int prepare_group(void *p, unsigned group, unsigned branch,
                         char e[256]) {
  (void)e;
  Trace *t = p;
  t->calls[t->count++] = 50;
  t->group = group;
  t->branch = branch;
  return 1;
}
static int check(int condition, const char *message) {
  if (!condition) fprintf(stderr, "FAIL: %s\n", message);
  return condition;
}
static int run(unsigned group, int expected_state, int expected_camera,
               int32_t a, int32_t b, unsigned eye) {
  Trace t = {0};
  BkEndingFrameState frame = {
      .group = (uint8_t)group, .camera_cached = 17, .camera_mode = 5};
  BkEndingControlState control = {.state_721eec = 4};
  BkEndingAuxiliaryState auxiliary = {.index = 23};
  BkEndingAuxiliaryControllerBindings bindings = {
      &frame, &control, &auxiliary};
  BkEndingAuxiliaryControllerOps ops = {
      &t, status, request, expression, group_sound, prepare_group};
  char error[256] = {0};
  if (!bk_ending_auxiliary_controller_begin(&bindings, &ops, error)) {
    fprintf(stderr, "FAIL controller: %s\n", error);
    return 0;
  }
  return check(control.state_721eec == expected_state, "state") &&
         check(frame.camera_mode == expected_camera, "camera mode") &&
         check(frame.camera_cached == -1, "camera cache") &&
         check(auxiliary.index == -1, "auxiliary index") &&
         check(t.request == 5, "clip") &&
         check(t.expression[0] == a && t.expression[1] == b, "expression") &&
         check(t.eye == eye, "eye") && check(t.group == group, "group") &&
         check(t.branch == group, "group branch") &&
         check(t.count == (group < 2 ? 6u : 5u), "call order count");
}
int main(void) {
  if (!run(0, 7, 4, 6, 3, 1) || !run(1, 7, 4, 6, 3, 1) ||
      !run(2, 5, 2, 0, 13, 1) || !run(3, 5, 2, 3, 4, 0) ||
      !run(4, 6, 2, 3, 4, 0))
    return 1;
  Trace busy = {.playing = {1, 0}};
  BkEndingFrameState frame = {
      .group = 2, .camera_cached = 17, .camera_mode = 5};
  BkEndingControlState control = {.state_721eec = 4};
  BkEndingAuxiliaryState auxiliary = {.index = 23};
  BkEndingAuxiliaryControllerBindings bindings = {
      &frame, &control, &auxiliary};
  BkEndingAuxiliaryControllerOps ops = {
      &busy, status, request, expression, group_sound, prepare_group};
  char error[256] = {0};
  if (!bk_ending_auxiliary_controller_begin(&bindings, &ops, error) ||
      !check(busy.count == 1, "busy no-op") ||
      !check(control.state_721eec == 4, "busy state") ||
      !check(auxiliary.index == 23, "busy prefix"))
    return 1;
  puts("PASS auxiliary controller: 47DC79 state-4 entry branches");
  return 0;
}
