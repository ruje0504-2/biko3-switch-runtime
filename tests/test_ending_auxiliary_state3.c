#include "game/ending_auxiliary_state3.h"
#include <stdio.h>
#include <string.h>

typedef struct {
  unsigned calls[32], count;
  int child_ok, key_value, hit_value;
  int32_t active_slot;
  int32_t random_value;
  int32_t a, b;
  unsigned eye, slot;
  int32_t cue, flags, volume;
  float center[2], radius, pointer[2];
  BkEndingFrameInput child_input;
} Trace;

static int child(void *context, const BkEndingFrameInput *input, char e[256]) {
  Trace *t = context;
  (void)e;
  t->calls[t->count++] = 10;
  memcpy(&t->child_input, input, sizeof(*input));
  return t->child_ok;
}
static int key(void *context, unsigned code, unsigned mode, uint32_t *out,
              char e[256]) {
  Trace *t = context;
  (void)e;
  t->calls[t->count++] = 20;
  if (code != 0 || mode != 3) return 0;
  *out = (uint32_t)t->key_value;
  return 1;
}
static int hit(void *context, const float center[2], float radius,
               const float pointer[2], int *out, char e[256]) {
  Trace *t = context;
  (void)e;
  t->calls[t->count++] = 30;
  memcpy(t->center, center, sizeof(t->center));
  t->radius = radius;
  memcpy(t->pointer, pointer, sizeof(t->pointer));
  *out = t->hit_value;
  return 1;
}
static int expression(void *context, int32_t a, int32_t b, unsigned eye,
                      char e[256]) {
  Trace *t = context;
  (void)e;
  t->calls[t->count++] = 40;
  t->a = a; t->b = b; t->eye = eye;
  return a == 7 && b == 3 && eye == 1;
}
static int request(void *context, unsigned slot, char e[256]) {
  Trace *t = context;
  (void)e;
  t->calls[t->count++] = 50;
  t->slot = slot;
  return slot == 1;
}
static int voice(void *context, int32_t cue, unsigned slot, int32_t flags,
                 int32_t volume, char e[256]) {
  Trace *t = context;
  (void)e;
  t->calls[t->count++] = 60;
  t->cue = cue; t->slot = slot; t->flags = flags; t->volume = volume;
  return cue == 1 && slot == 0 && flags == 0;
}
static int active_child(void *context, int32_t *out, char e[256]) {
  Trace *t = context;
  (void)e;
  t->calls[t->count++] = 70;
  *out = t->active_slot;
  return 1;
}
static int child_source(void *context, unsigned slot, float source,
                        char e[256]) {
  Trace *t = context;
  (void)e;
  t->calls[t->count++] = 170;
  t->slot = slot;
  t->radius = source;
  return slot == 4 && source > 229.9f && source < 230.1f;
}
static int present_child(void *context, unsigned owner, int *out, char e[256]) {
  Trace *t = context;
  (void)e;
  t->calls[t->count++] = 80 + owner;
  *out = 0;
  return 1;
}
static int status_child(void *context, unsigned owner, int *out, char e[256]) {
  Trace *t = context;
  (void)e;
  t->calls[t->count++] = 90 + owner;
  *out = 0;
  return 1;
}
static int random_child(void *context, int32_t *out, char e[256]) {
  Trace *t = context;
  (void)e;
  t->calls[t->count++] = 100;
  *out = t->random_value;
  return 1;
}
static int child_expression(void *context, int32_t a, int32_t b,
                            unsigned eye, char e[256]) {
  Trace *t = context;
  (void)e;
  t->calls[t->count++] = 110;
  return a == 0 && b == 4 && eye == 0;
}
static int child_request(void *context, unsigned slot, char e[256]) {
  Trace *t = context;
  (void)e;
  t->calls[t->count++] = 120;
  return slot == (t->active_slot == 4 ? 3u : 4u);
}
static int child_voice(void *context, int32_t cue, unsigned slot,
                      int32_t flags, int32_t volume, char e[256]) {
  Trace *t = context;
  (void)e;
  t->calls[t->count++] = 130;
  return ((cue == 5 && slot == 0) || (cue == 6 && slot == 1)) &&
         flags == 0 && volume == -333;
}
static int child_timing(void *context, unsigned slot, BkClipTiming *out,
                        char e[256]) {
  Trace *t = context;
  (void)e;
  t->calls[t->count++] = 140 + slot;
  if (slot == 2)
    *out = (BkClipTiming){0, 100, 40};
  else if (slot == 3)
    *out = (BkClipTiming){0, 100, 55};
  else
    return 0;
  return 1;
}
static int child_effect_present(void *context, unsigned effect, int *out,
                                char e[256]) {
  Trace *t = context;
  (void)e;
  t->calls[t->count++] = 150;
  *out = effect == 19;
  return 1;
}
static int child_effect_status(void *context, unsigned effect, int *out,
                               char e[256]) {
  Trace *t = context;
  (void)e;
  t->calls[t->count++] = 155;
  *out = 0;
  return effect == 19;
}
static int child_effect(void *context, unsigned effect, unsigned flags,
                        int32_t volume, char e[256]) {
  Trace *t = context;
  (void)e;
  t->calls[t->count++] = 160;
  return (effect == 18 || effect == 32 || effect == 33 ||
          (effect == 19 && flags == 1)) && volume == -444 &&
         (effect == 19 ? flags == 1 : flags == 0);
}

static int check(int ok, const char *what, const char *error) {
  if (ok) return 1;
  fprintf(stderr, "FAIL %s: %s\n", what, error);
  return 0;
}

int main(void) {
  BkEndingFrameInput input = {0};
  int32_t x = 123, y = -45, point[2] = {640, 360};
  float width = 24.0f;
  memcpy(&input.words[9], &x, sizeof(x));
  memcpy(&input.words[10], &y, sizeof(y));
  Trace t = {.child_ok = 1, .key_value = 1, .hit_value = 1, .active_slot = 2};
  BkEndingFrameState frame = {.group = 0, .camera_mode = 4,
                              .camera_cached = 9};
  BkEndingControlState control = {.state_721eec = 3};
  BkEndingAuxiliaryState auxiliary = {.index = 12, .pending = 0};
  BkEndingAuxiliaryState3Bindings bindings = {
      &frame, &control, &auxiliary, point, &width};
  BkEndingAuxiliaryState3Ops ops = {
      &t, child, key, hit, expression, request, voice, -777};
  char error[256] = {0};

  if (!check(bk_ending_auxiliary_state3_step(&bindings, &input, &ops, error),
             "hit", error) ||
      !check(t.count == 3 && t.calls[0] == 10 && t.calls[1] == 20 &&
                 t.calls[2] == 30, "hit order", error) ||
      !check(control.state_721eec == 8 && auxiliary.index == -1 &&
                 frame.camera_mode == 0 && frame.camera_cached == 9,
             "hit state", error) ||
      !check(t.center[0] == 640 && t.center[1] == 360 && t.radius == 12 &&
                 t.pointer[0] == 123 && t.pointer[1] == -45,
             "hit geometry", error))
    return 1;

  frame.group = 0;
  control.state_721eec = 3;
  control.toggles[7] = 1;
  auxiliary.pending = 0;
  auxiliary.index = 0;
  t.count = 0;
  t.hit_value = 1;
  t.random_value = 17;
  int32_t latch = 0;
  uint8_t effect_latches[4] = {0};
  BkEndingAuxiliaryChildBindings child_bindings = {
      &frame, &control, &auxiliary, point, NULL, &width, NULL, &latch,
      effect_latches};
  BkEndingAuxiliaryChildOps child_ops = {
      &t, active_child, child_source, present_child, status_child, hit,
      child_request,
      child_expression, child_voice, random_child, NULL, NULL,
      child_effect_status, child_effect, -333, -444};
  if (!check(bk_ending_auxiliary_state3_child_step(
                 &child_bindings, &input, &child_ops, error),
             "481EA5 hit prefix", error) ||
      !check(auxiliary.pending == 4 && auxiliary.index == 0 && latch == 1,
             "481EA5 hit state", error) ||
      !check(t.calls[0] == 30 && t.calls[1] == 120 && t.calls[2] == 110 &&
                 t.calls[3] == 80 && t.calls[4] == 130 && t.calls[5] == 100 &&
                 t.calls[6] == 130,
             "481EA5 hit order", error))
    return 1;

  frame.group = 2;
  t.count = 0;
  t.hit_value = 0;
  effect_latches[0] = effect_latches[1] = 0;
  child_ops.timing = child_timing;
  child_ops.effect_present = child_effect_present;
  child_ops.effect = child_effect;
  if (!check(bk_ending_auxiliary_state3_child_step(
                 &child_bindings, &input, &child_ops, error),
             "481EA5 group2 effects", error) ||
      !check(effect_latches[0] == 1 && effect_latches[1] == 1,
             "481EA5 group2 latches", error) ||
      !check(t.calls[0] == 30 && t.calls[1] == 70 && t.calls[2] == 143 &&
                 t.calls[3] == 160 && t.calls[4] == 160,
             "481EA5 group2 effect order", error))
    return 1;

  frame.group = 3;
  t.count = 0;
  effect_latches[0] = effect_latches[1] = effect_latches[2] =
      effect_latches[3] = 0;
  if (!check(bk_ending_auxiliary_state3_child_step(
                 &child_bindings, &input, &child_ops, error),
             "481EA5 group3 thresholds", error) ||
      !check(effect_latches[0] == 0 && effect_latches[1] == 0 &&
                 effect_latches[2] == 1 && effect_latches[3] == 1,
             "481EA5 group3 threshold latches", error) ||
      !check(t.count == 8 && t.calls[0] == 30 && t.calls[1] == 70 &&
                 t.calls[2] == 142 && t.calls[3] == 143 &&
                 t.calls[4] == 160 && t.calls[5] == 150 &&
                 t.calls[6] == 155 && t.calls[7] == 160,
             "481EA5 group3 threshold order", error))
    return 1;

  {
    int32_t targets[39][2] = {{700, 400}};
    int32_t offset_mode = -1;
    int32_t x0 = 640, y0 = 360;
    memcpy(&input.words[9], &x0, sizeof(x0));
    memcpy(&input.words[10], &y0, sizeof(y0));
    child_bindings.targets_721f90 = targets;
    child_bindings.offset_mode_54e2f8 = &offset_mode;
    frame.camera_cached = 0;
    frame.group = 0;
    t.active_slot = 4;
    t.count = 0;
    if (!check(bk_ending_auxiliary_state3_child_step(
                   &child_bindings, &input, &child_ops, error),
               "481EA5 clip3 interpolation", error) ||
        !check(t.count == 6 && t.calls[0] == 30 && t.calls[1] == 70 &&
                   t.calls[2] == 120 && t.calls[3] == 142 &&
                   t.calls[4] == 143 && t.calls[5] == 170 && t.slot == 4,
               "481EA5 clip3 interpolation order", error))
      return 1;
  }

  control.state_721eec = 3;
  frame.camera_mode = 4;
  frame.camera_cached = 9;
  auxiliary.index = 12;
  auxiliary.pending = 0;
  frame.group = 2;
  t.count = 0;
  t.key_value = 1;
  t.hit_value = 0;
  if (!check(bk_ending_auxiliary_state3_step(&bindings, &input, &ops, error),
             "miss", error) ||
      !check(t.count == 6 && t.calls[0] == 10 && t.calls[1] == 20 &&
                 t.calls[2] == 30 && t.calls[3] == 40 && t.calls[4] == 50 &&
                 t.calls[5] == 60, "miss order", error) ||
      !check(control.state_721eec == 1 && auxiliary.index == 0x48 &&
                 auxiliary.pending == 1 && frame.camera_mode == 0 &&
                 frame.camera_cached == -1 && t.a == 7 && t.b == 3 &&
                 t.eye == 1 && t.cue == 1 && t.slot == 0 && t.volume == -777,
             "miss state", error))
    return 1;

  control.state_721eec = 3;
  frame.group = 0;
  t.count = 0;
  t.key_value = 0;
  if (!check(bk_ending_auxiliary_state3_step(&bindings, &input, &ops, error),
             "key no-op", error) ||
      !check(t.count == 2 && t.calls[0] == 10 && t.calls[1] == 20 &&
                 control.state_721eec == 3 && auxiliary.pending == 1,
             "key no-op state", error))
    return 1;

  t.count = 0;
  t.child_ok = 0;
  control.state_721eec = 3;
  if (!check(!bk_ending_auxiliary_state3_step(&bindings, &input, &ops, error),
             "missing child", error) ||
      !check(t.count == 1 && control.state_721eec == 3,
             "missing child prefix", error))
    return 1;

  puts("PASS auxiliary state3: 47DC79 parent prefix and 481EA5 boundary");
  return 0;
}
