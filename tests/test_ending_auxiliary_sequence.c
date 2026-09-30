#include "game/ending_auxiliary_sequence.h"
#include <stdio.h>
#include <string.h>

typedef struct {
  BkEndingFrameState frame;
  BkEndingControlState control;
  BkEndingAuxiliaryState auxiliary;
  BkMenuCamera camera;
  BkEndingRecords records;
  uint8_t substate, latches[10], saved_toggle;
  int32_t pass;
  int32_t voice_latches[2], expression_override, face_mode, active;
  float timer, saved_orbit[4], timings[128][3];
  uint32_t saved_target[3];
  int8_t previous_flow;
  int32_t voice_volume, effect_volume;
  float seconds;
  uint8_t present[48], playing[48];
  unsigned events[256], count;
  unsigned requests[16], request_count;
  int special_calls;
} Fixture;

static int event(Fixture *f, unsigned value) {
  if (f->count >= sizeof(f->events) / sizeof(f->events[0])) return 0;
  f->events[f->count++] = value;
  return 1;
}
static int active(void *p, int32_t *slot, char e[256]) {
  Fixture *f = p; (void)e;
  *slot = f->active; return event(f, 1);
}
static int timing(void *p, unsigned slot, BkEndingClipTiming *out, char e[256]) {
  Fixture *f = p; (void)e;
  if (slot >= 128) return 0;
  *out = (BkEndingClipTiming){f->timings[slot][0], f->timings[slot][1],
                              f->timings[slot][2]};
  return event(f, 2);
}
static int request(void *p, unsigned slot, char e[256]) {
  Fixture *f = p; (void)e;
  if (f->request_count >= 16) return 0;
  f->requests[f->request_count++] = slot;
  f->active = (int32_t)slot;
  return event(f, 3);
}
static int restart(void *p, unsigned slot, char e[256]) {
  Fixture *f = p; (void)e; f->active = (int32_t)slot; return event(f, 4);
}
static int present(void *p, unsigned slot, int *out, char e[256]) {
  Fixture *f = p; (void)e;
  if (slot >= 48) return 0;
  *out = f->present[slot]; return event(f, 5);
}
static int audio(void *p, const BkEndingAudioCall *call, int *out,
                 char e[256]) {
  Fixture *f = p; (void)e;
  if (call->slot >= 48) return 0;
  if (!event(f, 10 + (unsigned)call->operation)) return 0;
  if (call->operation == BK_ENDING_AUDIO_STATUS)
    *out = f->playing[call->slot];
  else if (call->operation == BK_ENDING_AUDIO_RESTART)
    f->playing[call->slot] = 1;
  else if (call->operation == BK_ENDING_AUDIO_PAUSE)
    f->playing[call->slot] = 0;
  return 1;
}
static int voice(void *p, int32_t cue, unsigned slot, int32_t flags,
                 int32_t volume, char e[256]) {
  Fixture *f = p; (void)e; (void)volume;
  if (slot > 1 || flags != 0) return 0;
  return event(f, 40 + (unsigned)cue);
}
static int expression(void *p, int32_t a, int32_t b, unsigned eye,
                      char e[256]) {
  Fixture *f = p; (void)e;
  return event(f, 80 + (unsigned)(a + b + (int32_t)eye));
}
static int target(void *p, float out[3], char e[256]) {
  Fixture *f = p; (void)e;
  memcpy(out, f->timings[120], sizeof(float) * 3);
  return event(f, 90);
}
static int special(void *p, int32_t volume, char e[256]) {
  Fixture *f = p; (void)e; (void)volume;
  ++f->special_calls;
  f->playing[2 + 7] = 1;
  return 1;
}

static BkEndingAuxiliarySequenceBindings bindings(Fixture *f) {
  return (BkEndingAuxiliarySequenceBindings){
      .frame = &f->frame, .control = &f->control, .auxiliary = &f->auxiliary,
      .camera = &f->camera, .records = &f->records, .substate = &f->substate,
      .latches = f->latches, .saved_toggle = &f->saved_toggle,
      .voice_latches = f->voice_latches, .timer = &f->timer,
      .pass = &f->pass, .saved_orbit = f->saved_orbit,
      .saved_target = f->saved_target,
      .expression_override = &f->expression_override, .face_mode = &f->face_mode,
      .previous_flow = &f->previous_flow, .voice_volume = &f->voice_volume,
      .effect_volume = &f->effect_volume, .seconds = f->seconds};
}
static BkEndingAuxiliarySequenceOps ops(Fixture *f) {
  return (BkEndingAuxiliarySequenceOps){
      .context = f, .active = active, .timing = timing, .request = request,
      .restart = restart, .present = present, .audio = audio, .voice = voice,
      .expression = expression, .target = target, .special_audio = special};
}
static void init(Fixture *f, unsigned group, int state) {
  memset(f, 0, sizeof(*f));
  f->frame.group = (uint8_t)group;
  f->control.state_721eec = state;
  f->active = 6;
  f->voice_volume = f->effect_volume = -100;
  f->previous_flow = 0x18;
  f->camera = (BkMenuCamera){.yaw = 1, .pitch = 2, .radius = 3, .height = 4};
  f->timings[120][0] = 7; f->timings[120][1] = 8; f->timings[120][2] = 9;
  for (unsigned i = 0; i < 48; ++i) f->present[i] = 1;
  for (unsigned i = 0; i < 128; ++i)
    f->timings[i][1] = 1000;
}
static int run(Fixture *f, char e[256]) {
  BkEndingAuxiliarySequenceBindings b = bindings(f);
  BkEndingAuxiliarySequenceOps o = ops(f);
  return bk_ending_auxiliary_sequence_step(&b, &o, e);
}

int main(void) {
  char e[256] = {0};
  Fixture f;
  init(&f, 2, 6);
  f.timings[6][0] = 0; f.timings[6][1] = 200; f.timings[6][2] = 0;
  f.frame.camera_values[0] = 1; f.frame.camera_values[1] = 2;
  f.frame.camera_values[2] = 3;
  if (!run(&f, e) || f.control.state_721eec != 7 || f.request_count != 1 ||
      f.requests[0] != 6 || f.pass != 0 || f.saved_orbit[0] != 1 ||
      f.frame.camera_values[0] != 0x40e00000u) {
    fprintf(stderr, "state6 setup failed: %s state=%d req=%u first=%u pass=%d orbit=%g target=%u\n", e, f.control.state_721eec, f.request_count, f.request_count ? f.requests[0] : 999u, f.pass, f.saved_orbit[0], f.frame.camera_values[0]); return 1;
  }

  init(&f, 2, 6);
  f.substate = 1; f.active = 6; f.playing[0] = 0;
  f.previous_flow = 8;
  if (!run(&f, e) || f.control.state_721eec != 4 ||
      !f.frame.curtain_wanted || f.records.groups[2].count != 1 ||
      f.records.groups[2].actions[0] != 13) {
    fprintf(stderr, "state6 substate1 finish failed: %s state=%d count=%d\n",
            e, f.control.state_721eec, f.records.groups[2].count); return 1;
  }

  init(&f, 2, 6);
  f.timings[6][1] = 200;
  if (!run(&f, e) || f.control.state_721eec != 7) {
    fprintf(stderr, "state7 entry setup failed: %s state=%d\n",
            e, f.control.state_721eec); return 1;
  }
  f.timings[6][2] = 200;
  if (!run(&f, e) || f.pass != 1 || f.request_count != 1 ||
      f.camera.yaw != 211 || f.camera.pitch != -6) {
    fprintf(stderr, "state7 pass failed: %s pass=%d requests=%u\n", e,
            f.pass, f.request_count); return 1;
  }

  f.pass = 2; f.active = 7; f.timings[7][2] = f.timings[7][1] = 200;
  f.saved_orbit[0] = 50; f.saved_orbit[1] = 51; f.saved_orbit[2] = 52;
  f.saved_orbit[3] = 53;
  f.saved_target[0] = 11; f.saved_target[1] = 12; f.saved_target[2] = 13;
  if (!run(&f, e) || f.control.state_721eec != 5 || f.request_count != 2 ||
      f.requests[1] != 8 || f.frame.camera_mode != 0 ||
      f.camera.yaw != 50 || f.frame.camera_values[0] != 11) {
    fprintf(stderr, "state7 restore failed: %s state=%d\n", e,
            f.control.state_721eec); return 1;
  }

  init(&f, 2, 5);
  f.substate = 4; f.control.pause_selection = 0;
  f.seconds = 20.1f; f.previous_flow = 8;
  if (!run(&f, e) || f.control.state_721eec != 4 ||
      f.frame.transition_action != 7 || !f.frame.curtain_wanted ||
      f.records.groups[2].count != 1 || f.records.groups[2].actions[0] != 13) {
    fprintf(stderr, "state5 finish failed: %s state=%d count=%d\n", e,
            f.control.state_721eec, f.records.groups[2].count); return 1;
  }

  init(&f, 0, 5);
  f.substate = 3; f.active = 7; f.timings[7][2] = 220;
  if (!run(&f, e) || f.special_calls != 1 || f.auxiliary.pending != 100 ||
      !f.playing[2 + 7]) {
    fprintf(stderr, "special audio dispatch failed: %s calls=%d pending=%d\n",
            e, f.special_calls, f.auxiliary.pending); return 1;
  }
  BkEndingAuxiliarySequenceBindings b = bindings(&f);
  BkEndingAuxiliarySequenceOps o = ops(&f);
  o.special_audio = NULL;
  f.auxiliary.pending = 0;
  if (bk_ending_auxiliary_sequence_step(&b, &o, e) || f.auxiliary.pending == 100) {
    fprintf(stderr, "special audio missing-service boundary was not rejected\n"); return 1;
  }

  init(&f, 3, 5);
  f.substate = 3; f.active = 7; f.timings[7][2] = 152;
  if (!run(&f, e) || !f.playing[2 + 24]) {
    fprintf(stderr, "group3 effect threshold failed: %s\n", e); return 1;
  }
  puts("PASS auxiliary sequence: state5/6/7 camera, tail, pass, finish, effects");
  return 0;
}
