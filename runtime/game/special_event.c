#include "game/special_event.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static int fail(char e[256], const char *why) {
  if (e) snprintf(e, 256, "special event: %s", why);
  return 0;
}
#define CALL(member, ...) do { \
  if (!o->member) return fail(e, "missing " #member " service"); \
  if (!o->member(o->context, __VA_ARGS__)) return 0; \
} while (0)
BkSpecialEventState bk_special_event_initial(void) {
  return (BkSpecialEventState){0};
}
static int valid_group(int32_t group, char e[256]) {
  return group >= 0 && group < 5 ? 1 : fail(e, "undefined group configuration");
}
static int opening(const BkSpecialEventBindings *b,
                   const BkSpecialEventOps *o, uint8_t *done, char e[256]) {
  if (!valid_group(*b->group, e)) return 0;
  const float zero[3] = {0};
  uint8_t ignored;
  CALL(camera, BK_SPECIAL_CAMERA_OPEN, 0, zero,
       *b->paused ? 0 : *b->seconds, &ignored, e);
  float source, end;
  CALL(timing, 1, &source, &end, e);
  if (source >= end) { *done = 1; return 1; }
  const uint32_t keys[] = {0, 0x5a, 0x33450};
  for (unsigned i = 0; i < 3; ++i) {
    CALL(key, keys[i], done, e);
    if (*done) return 1;
  }
  return 1;
}
static int placement(int32_t group, const BkSpecialEventOps *o, char e[256]) {
  int present;
  CALL(present, BK_SPECIAL_CAMERA_PRIMARY, &present, e);
  if (present && group >= 0 && group < 5) {
    const float p[3] = {group == 3 ? -20 : 0, 0, group == 3 ? 40 : 0};
    CALL(place, 0, p, 0, e);
  }
  CALL(present, BK_SPECIAL_CAMERA_SECONDARY, &present, e);
  if (present && group >= 0 && group < 5) {
    const float axis[3] = {0, 1, 0};
    CALL(place, 1, axis, group == 2 ? 160 : 0, e);
  }
  return 1;
}
static int audio_step(const BkSpecialEventBindings *b,
                      const BkSpecialEventOps *o, char e[256]) {
  static const int32_t ticks[5] = {70, 36, 120, 70, 60};
  static const char *names[5] = {"se400.wav", "se410.wav", "se420.wav",
                                "se430.wav", "se440.wav"};
  static const char *paths[5] = {"\\wav\\se400.wav", "\\wav\\se410.wav",
      "\\wav\\se420.wav", "\\wav\\se430.wav", "\\wav\\se440.wav"};
  int32_t group = *b->group;
  int32_t tick = group >= 0 && group < 5 ? ticks[group] : 0;
  /*Filename is captured BEFORE fade/cue, pack is reread AFTER cue.*/
  const char *name = tick ? (*b->packed == 1 ? names[group] : paths[group]) : "";
  if (*b->music_wanted == 0 || *b->music_wanted == 1) {
    BkSpecialEventAudioCall call = {.operation = BK_SPECIAL_MUSIC_FADE,
        .flags = (uint8_t)*b->music_wanted,
        .amount = (float)((double)*b->seconds * (*b->music_wanted ? 600 : 100))};
    CALL(audio, &call, e);
  }
  float listener[4];
  memcpy(listener, b->listener, sizeof listener);
  if (tick) {
    uint8_t triggered;
    CALL(cue, tick, &triggered, e);
    if (triggered) {
      BkSpecialEventAudioCall call = {.operation = BK_SPECIAL_EFFECT_LOAD,
          .slot = 1, .pack = *b->packed == 1 ? "bk3_02.pp" : NULL,
          .name = name, .volume = *b->effect_volume};
      CALL(audio, &call, e);
      memset(b->effect_positions[1], 0, 4 * sizeof(float));
      call = (BkSpecialEventAudioCall){.operation = BK_SPECIAL_EFFECT_PLAY, .slot = 1};
      CALL(audio, &call, e);
    }
  }
  for (unsigned i = 0; i < 4; ++i) {
    int present;
    CALL(present, (BkSpecialEventObject)(BK_SPECIAL_EFFECT0 + i), &present, e);
    if (present) {
      BkSpecialEventAudioCall call = {.operation = BK_SPECIAL_EFFECT_SPATIAL,
          .slot = i, .flags = 2, .amount = 4};
      memcpy(call.source, b->effect_positions[i], sizeof call.source);
      memcpy(call.listener, listener, sizeof listener);
      CALL(audio, &call, e);
    }
  }
  if (*b->group == 1 || *b->group == 4) {
    int present;
    CALL(present, BK_SPECIAL_MOVIE, &present, e);
    if (present) CALL(movie, e);
  }
  return 1;
}
typedef struct { int32_t track, mode; float rate, level; } Face;
static int face_parameters(const BkSpecialEventBindings *b,
    const BkSpecialEventOps *o, Face *out, char e[256]) {
  int32_t group = *b->group;
  if (!valid_group(group, e)) return 0;
  static const int32_t tracks[5] = {3, 1, 4, 4, 0};
  static const float rates[5] = {9, 7, 7, 9, 0};
  out->track = tracks[group]; out->rate = rates[group];
  if (!group) {
    b->state->face_timer.duration = 500;
    uint32_t now;
    CALL(clock, 1, &now, e);
    if (bk_timer_poll(&b->state->face_timer, now)) {
      int32_t random;
      CALL(random, &random, e);
      b->state->face_target = (float)(random % 5);
    }
    if (!bk_voice_envelope_target(b->envelope, b->state->face_target,
                                 *b->seconds, &out->level, e)) return 0;
  } else {
    if (group == 2) {
      float source, end;
      CALL(timing, 0, &source, &end, e);
      if (source >= 45 && source <= 80) out->rate = 0;
    }
    CALL(level, 1, *b->seconds, &out->level, e);
  }
  out->mode = group == 2 ? 0 : 1;
  return 1;
}
static int sequence(const BkSpecialEventBindings *b,
    const BkSpecialEventOps *o, int32_t phase, int32_t *clip, char e[256]) {
  if (*b->group != 1) return 1;
  BkSpecialEventState *s = b->state;
  s->sequence_timer.duration = 30000;
  if (phase == 2 && !s->sequence) {
    *b->effect_loop = 1;
    BkSpecialEventAudioCall call = {.operation = BK_SPECIAL_EFFECT_DIRECT_PLAY,
                                   .flags = 1};
    CALL(audio, &call, e);
    s->sequence = 1;
    s->sequence_timer.armed = 0;
  }
  if (s->sequence == 1) {
    uint32_t now;
    CALL(clock, 1, &now, e);
    if (bk_timer_poll(&s->sequence_timer, now)) {
      BkSpecialEventAudioCall call = {.operation = BK_SPECIAL_EFFECT_DIRECT_PLAY};
      CALL(audio, &call, e);
      s->sequence = 2;
    }
  }
  *clip = s->sequence == 2 ? 2 : 0;
  return 1;
}
int bk_special_event_step(const BkSpecialEventBindings *b,
    const BkSpecialEventOps *o, char e[256]) {
  if (!b || !o || !b->state || !b->group || !b->phase || !b->camera_clip ||
      !b->camera_mode || !b->seconds || !b->paused || !b->music_wanted ||
      !b->packed || !b->visibility || !b->effect_loop || !b->effect_volume ||
      !b->listener || !b->envelope || !isfinite(*b->seconds) || *b->seconds < 0)
    return fail(e, "invalid bindings/time");
  for (unsigned i = 0; i < 4; ++i)
    if (!b->effect_positions[i]) return fail(e, "missing effect position alias");
  int32_t group = *b->group;
  if (!valid_group(group, e)) return 0;
  float center[3] = {group == 3 ? -20 : 0, 0,
                    group == 0 ? -6.5f : group == 3 ? -6.7f : 0};
  uint32_t pitch_bits = 0x3db2b55f, yaw_bits = 0x3e32b7fe;
  float pitch, yaw;
  memcpy(&pitch, &pitch_bits, 4); memcpy(&yaw, &yaw_bits, 4);
  if (group == 1) yaw = 3;
  int32_t clip = group == 1 ? 1 : 0;
  uint8_t done = 0;
  switch (*b->phase) {
  case 0:
    if (!opening(b, o, &done, e)) return 0;
    if (done) *b->phase = 1;
    break;
  case 1:
    CALL(camera, BK_SPECIAL_CAMERA_TRANSITION, 0, center, *b->seconds, &done, e);
    if (done) *b->phase = 2;
    break;
  case 2:
    if (*b->camera_mode == 0) {
      CALL(camera, BK_SPECIAL_CAMERA_ORBIT, 0, center, *b->seconds, &done, e);
    } else if (*b->camera_mode == 1) {
      const float zero[3] = {0};
      CALL(camera, BK_SPECIAL_CAMERA_TRACK, 0, zero, *b->seconds, &done, e);
    } else if (*b->camera_mode == 2) {
      CALL(camera, BK_SPECIAL_CAMERA_TRANSITION, *b->camera_clip, center, *b->seconds, &done, e);
      if (done) *b->camera_mode = 0;
    }
    break;
  }
  if (!placement(*b->group, o, e) || !audio_step(b, o, e)) return 0;
  Face face;
  if (!face_parameters(b, o, &face, e) ||
      !sequence(b, o, *b->phase, &clip, e)) return 0;
  uint32_t now;
  CALL(clock, 0, &now, e);
  CALL(request, clip, e);
  CALL(advance, (float)(.5 * (double)*b->seconds), e);
  int present;
  CALL(present, BK_SPECIAL_PRIMARY_FACE, &present, e);
  if (present) {
    CALL(gaze, pitch, yaw, e);
    CALL(face, BK_SPECIAL_FACE_MODE, (uint8_t)face.mode, 0, 0, e);
    CALL(face, BK_SPECIAL_FACE_RANGE, 0, face.rate, 0, e);
    CALL(face, BK_SPECIAL_FACE_EXPRESSION, face.track, 0, 0, e);
    CALL(face, BK_SPECIAL_FACE_MOUTH, 0, face.level, now, e);
    CALL(face, BK_SPECIAL_FACE_BLINK, 0, 0, now, e);
  }
  CALL(hide, *b->visibility, e);
  return 1;
}
