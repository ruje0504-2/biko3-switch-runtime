#include "game/ending_auxiliary_controller.h"
#include <stdio.h>
#include <string.h>

typedef struct {
  unsigned calls[16], count;
  int playing[2];
  unsigned group, effect_group, camera_group;
  int camera_preset;
  int32_t expression[2];
  unsigned eye, request, voice_cue, voice_slot;
  float camera[4], target[3];
} Trace;

static int status(void *p, unsigned owner, int *playing, char e[256]) {
  Trace *t = p;
  if (owner >= 2 || !playing) { snprintf(e, 256, "bad status"); return 0; }
  t->calls[t->count++] = 10 + owner;
  *playing = t->playing[owner];
  return 1;
}
static int request(void *p, unsigned slot, char e[256]) {
  (void)e; Trace *t = p; t->calls[t->count++] = 20 + slot; t->request = slot;
  return 1;
}
static int expression(void *p, int32_t a, int32_t b, unsigned eye,
                      char e[256]) {
  (void)e; Trace *t = p; t->calls[t->count++] = 30;
  t->expression[0] = a; t->expression[1] = b; t->eye = eye; return 1;
}
static int voice(void *p, int32_t cue, unsigned slot, int32_t flags,
                 int32_t volume, char e[256]) {
  (void)flags; (void)volume; (void)e; Trace *t = p; t->calls[t->count++] = 35;
  t->voice_cue = (unsigned)cue; t->voice_slot = slot; return 1;
}
static int group_sound(void *p, unsigned group, char e[256]) {
  (void)e; Trace *t = p; t->calls[t->count++] = 40; t->effect_group = group;
  return 1;
}
static int camera_setup(void *p, unsigned group, const float values[4],
                        int preset, char e[256]) {
  (void)e; Trace *t = p; t->calls[t->count++] = 50; t->camera_group = group;
  t->camera_preset = preset; memcpy(t->camera, values, sizeof(t->camera));
  return 1;
}
static int target(void *p, float position[3], char e[256]) {
  (void)e; Trace *t = p; t->calls[t->count++] = 60;
  memcpy(position, t->target, sizeof(t->target)); return 1;
}
static int check(int condition, const char *message) {
  if (!condition) fprintf(stderr, "FAIL: %s\n", message);
  return condition;
}

static int run(unsigned group, int expected_state, int expected_mode,
               int32_t a, int32_t b, unsigned eye, unsigned count) {
  Trace t = {.target = {1, 2, 3}};
  BkEndingFrameState frame = {
      .group = (uint8_t)group, .camera_cached = 17, .camera_mode = 5};
  BkEndingControlState control = {.state_721eec = 8, .target_choice = 9};
  BkEndingAuxiliaryState auxiliary = {.index = 23};
  int32_t delay = 42;
  uint8_t saved_toggle = 7;
  BkEndingAuxiliaryControllerBindings bindings = {
      &frame, &control, &auxiliary, &delay, &saved_toggle};
  BkEndingAuxiliaryControllerOps ops = {
      &t, status, request, expression, voice, group_sound, camera_setup,
      target, -600};
  char error[256] = {0};
  if (!bk_ending_auxiliary_controller_begin(&bindings, &ops, error)) {
    fprintf(stderr, "FAIL controller: %s\n", error); return 0;
  }
  int ok = check(control.state_721eec == expected_state, "state") &&
           check(frame.camera_mode == expected_mode, "camera mode") &&
           check(frame.camera_cached == -1, "camera cache") &&
           check(auxiliary.index == -1, "auxiliary index") &&
           check(delay == 0, "delay") && check(t.request == 5, "clip") &&
           check(group >= 2 || saved_toggle == 0, "saved toggle") &&
           check(t.expression[0] == a && t.expression[1] == b, "expression") &&
           check(t.eye == eye, "eye") && check(t.voice_cue == 7, "voice cue") &&
           check(t.voice_slot == 0, "voice slot") && check(t.count == count, "call order");
  if (group < 4) {
    ok = ok && check(t.camera_group == group, "camera group") &&
         check(t.camera_preset == (group >= 2), "camera preset");
  }
  if (group < 2) {
    ok = ok && check(t.effect_group == group, "effect group") &&
         check(control.target_choice == 1, "target choice") &&
         check(frame.camera_clip == 0, "camera clip") &&
         check(frame.camera_table[group][0] == 0x40000000u, "camera table") &&
         check(!memcmp(frame.camera_values, t.target, sizeof(t.target)), "target");
  } else if (group < 4) {
    ok = ok && check(control.target_choice == 1, "target choice") &&
         check(frame.camera_clip == 0, "camera clip") &&
         check(frame.camera_table[group][0] == 0x40000000u, "camera table") &&
         check(!memcmp(frame.camera_values, t.target, sizeof(t.target)), "target");
  } else {
    ok = ok && check(control.target_choice == 9, "group4 target untouched") &&
         check(frame.camera_clip == 0, "group4 clip untouched") &&
         check(frame.camera_table[group][0] == 0, "group4 table untouched");
  }
  return ok;
}

int main(void) {
  if (!run(0, 7, 4, 6, 3, 1, 8) ||
      !run(1, 7, 4, 6, 3, 1, 8) ||
      !run(2, 5, 2, 0, 13, 1, 7) ||
      !run(3, 5, 2, 3, 4, 0, 7) ||
      !run(4, 6, 5, 3, 4, 0, 5))
    return 1;

  Trace busy = {.playing = {1, 0}};
  BkEndingFrameState frame = {.group = 2, .camera_cached = 17};
  BkEndingControlState control = {.state_721eec = 8};
  BkEndingAuxiliaryState auxiliary = {.index = 23};
  int32_t delay = 42;
  uint8_t saved_toggle = 7;
  BkEndingAuxiliaryControllerBindings bindings = {
      &frame, &control, &auxiliary, &delay, &saved_toggle};
  BkEndingAuxiliaryControllerOps ops = {
      &busy, status, request, expression, voice, group_sound, camera_setup,
      target, -600};
  char error[256] = {0};
  if (!bk_ending_auxiliary_controller_begin(&bindings, &ops, error) ||
      !check(busy.count == 1, "busy no-op") ||
      !check(control.state_721eec == 8, "busy state") ||
      !check(auxiliary.index == 23, "busy prefix") || !check(delay == 42, "busy delay"))
    return 1;
  puts("PASS auxiliary controller: 47DC79 state-8 entry branches");
  return 0;
}
