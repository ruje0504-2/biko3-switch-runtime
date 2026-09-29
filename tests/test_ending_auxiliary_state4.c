#include "game/ending_auxiliary_state4.h"
#include <stdio.h>
#include <string.h>

typedef struct {
  int present[2], playing[2], complete;
  unsigned calls[16], count;
  unsigned effects[4][3], effect_count;
  float target[3];
} Trace;

static int present(void *context, unsigned owner, int *out, char e[256]) {
  Trace *t = context;
  if (owner >= 2 || !out) { snprintf(e, 256, "bad present"); return 0; }
  t->calls[t->count++] = 10 + owner;
  *out = t->present[owner];
  return 1;
}
static int status(void *context, unsigned owner, int *out, char e[256]) {
  Trace *t = context;
  if (owner >= 2 || !out) { snprintf(e, 256, "bad status"); return 0; }
  t->calls[t->count++] = 20 + owner;
  *out = t->playing[owner];
  return 1;
}
static int prepare_actor(void *context, char e[256]) {
  Trace *t = context; (void)e; t->calls[t->count++] = 30; return 1;
}
static int expression(void *context, int32_t a, int32_t b, unsigned eye,
                      char e[256]) {
  Trace *t = context; (void)e; t->calls[t->count++] = 40;
  if (a != 7 || b != 3 || eye != 1) return 0;
  return 1;
}
static int target(void *context, float out[3], char e[256]) {
  Trace *t = context; (void)e; memcpy(out, t->target, sizeof(t->target)); return 1;
}
static int camera(void *context, BkEndingOpeningCamera kind, int32_t choice,
                  const uint32_t offset[3], uint32_t extra, uint32_t *result,
                  char e[256]) {
  Trace *t = context; (void)kind; (void)choice; (void)offset; (void)extra;
  (void)e; t->calls[t->count++] = 50; *result = (uint32_t)t->complete; return 1;
}
static int effect(void *context, unsigned a, unsigned b, unsigned c,
                  char e[256]) {
  Trace *t = context; (void)e;
  t->calls[t->count++] = 60;
  t->effects[t->effect_count][0] = a;
  t->effects[t->effect_count][1] = b;
  t->effects[t->effect_count++][2] = c;
  return 1;
}

int main(void) {
  Trace trace = {.complete = 1, .target = {1, 2, 3}};
  BkEndingFrameState frame = {.group = 2, .camera_clip = 0,
                              .camera_cached = -1};
  BkEndingControlState control = {.state_721eec = 4};
  BkEndingAuxiliaryState auxiliary = {0};
  BkMenuCamera camera_state = {0};
  uint8_t substate = 0;
  float fov = 1;
  int8_t previous = 0x18;
  BkEndingAuxiliaryState4Bindings bindings = {
      &frame, &control, &auxiliary, &camera_state, &substate, &fov, &previous};
  BkEndingAuxiliaryState4Ops ops = {
      &trace, present, status, prepare_actor, expression, target, camera, effect};
  char error[256] = {0};

  if (!bk_ending_auxiliary_state4_step(&bindings, 0, &ops, error) ||
      substate != 1 || auxiliary.index != 0x48 || trace.count != 3 ||
      camera_state.fov != .2f) {
    fprintf(stderr, "first: %s sub=%u index=%d count=%u fov=%g\n", error,
            substate, auxiliary.index, trace.count, camera_state.fov);
    return 1;
  }
  if (!bk_ending_auxiliary_state4_step(&bindings, 0, &ops, error) ||
      substate != 2 || auxiliary.pending != 1 || frame.camera_mode != 0) {
    fprintf(stderr, "second: %s sub=%u pending=%d mode=%d\n", error,
            substate, auxiliary.pending, frame.camera_mode);
    return 1;
  }

  trace.present[0] = 1; trace.playing[0] = 1;
  if (!bk_ending_auxiliary_state4_step(&bindings, 0, &ops, error) ||
      substate != 2) { fprintf(stderr, "busy: %s sub=%u\n", error, substate); return 1; }
  trace.playing[0] = 0;
  if (!bk_ending_auxiliary_state4_step(&bindings, 0, &ops, error) ||
      substate != 3 ||
      !bk_ending_auxiliary_state4_step(&bindings, 0, &ops, error) ||
      substate != 0 || control.state_721eec != 0) {
    fprintf(stderr, "done: %s sub=%u state=%d\n", error, substate,
            control.state_721eec); return 1;
  }
  BkEndingFrameState missing_frame = {.group = 2, .camera_clip = 0};
  BkEndingControlState missing_control = {.state_721eec = 4};
  BkEndingAuxiliaryState missing_auxiliary = {0};
  BkMenuCamera missing_camera = {0};
  uint8_t missing_substate = 0;
  float missing_fov = 1;
  BkEndingAuxiliaryState4Bindings missing_bindings = {
      &missing_frame, &missing_control, &missing_auxiliary, &missing_camera,
      &missing_substate, &missing_fov, &previous};
  BkEndingAuxiliaryState4Ops missing_ops = ops;
  missing_ops.prepare_actor = NULL;
  if (bk_ending_auxiliary_state4_step(&missing_bindings, 0, &missing_ops,
                                      error) || missing_auxiliary.index != 0) {
    fprintf(stderr, "missing actor service was not rejected: %s\n", error);
    return 1;
  }
  puts("PASS auxiliary state4: 47DC79 five-substate dispatch prefix");
  return 0;
}
