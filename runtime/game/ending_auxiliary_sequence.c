#include "game/ending_auxiliary_sequence.h"
#include "game/ending_sound.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

enum { BK_ENDING_SEQUENCE_CLIP_SLOTS = 128 };

static int fail(char e[256], const char *why) {
  if (e) snprintf(e, 256, "auxiliary sequence: %s", why);
  return 0;
}

/* Fixed EXE tables at54e2bc/54e2cc/54e2dc. Zero is a real table value; an
 * entry is used only on the same group/branch as the original instruction. */
static const unsigned initial[5] = {10, 29, 32, 18, 13};
static const unsigned middle[5][3] = {
    {11, 0, 0}, {30, 0, 0}, {34, 35, 0}, {21, 22, 23}, {14, 15, 16}};
static const unsigned final_effect[5][5] = {
    {12, 0, 0, 0, 0}, {31, 0, 0, 0, 0}, {36, 37, 0, 0, 0},
    {24, 25, 26, 27, 28}, {17, 0, 0, 0, 0}};
static const float ordinary[5][4] = {
    {140, 9, 120, -3.5f}, {0, 6, 108, -1}, {112, 21, 80, 2},
    {328, 26, 59, 1},    {345, 2, 120, -3}};
static const float orbit[5][3][4] = {
    {{77.83f, -33.61f, 85.07f, 2.24f},
     {99.09f, -41.8f, 63.99f, 2.79f},
     {270, 76, 49, 11}},
    {{328, 2, 28, 5}, {24, 12, 24, 6}, {180, 72, 37, 5}},
    {{138, 17, 22, 3}, {211, -6, 19, 3}, {185, 27, 23, 3}},
    {{320.82f, 18.7f, 80.52f, -3.05f},
     {359.8f, -5.88f, 39.55f, -1.98f},
     {313, 73, 56, 0}},
    {{17.22f, -28, 40.8f, .86f},
     {117.63f, 29.69f, 66.2f, 1.45f},
     {346, -11, 47, 1}}};

#define CALL(member, ...)                                                     \
  do {                                                                        \
    if (!o->member) return fail(e, "missing " #member " service");            \
    if (!o->member(o->context, __VA_ARGS__)) return 0;                        \
  } while (0)

static int media_busy(const BkEndingAuxiliarySequenceOps *o, unsigned owner,
                      int *busy_out, char e[256]) {
  int present = 0;
  CALL(present, owner, &present, e);
  *busy_out = 0;
  if (present) {
    BkEndingAudioCall call = {
        .operation = BK_ENDING_AUDIO_STATUS, .slot = owner};
    CALL(audio, &call, busy_out, e);
  }
  return 1;
}

static int effect_busy(const BkEndingAuxiliarySequenceOps *o, unsigned effect,
                       int *busy_out, char e[256]) {
  int present = 0;
  if (effect >= BK_ENDING_EFFECTS)
    return fail(e, "effect table index is outside the ending bank");
  CALL(present, effect + 2, &present, e);
  *busy_out = 0;
  if (present) {
    BkEndingAudioCall call = {
        .operation = BK_ENDING_AUDIO_STATUS, .slot = effect + 2};
    CALL(audio, &call, busy_out, e);
  }
  return 1;
}

static int effect_play(const BkEndingAuxiliarySequenceBindings *b,
                       const BkEndingAuxiliarySequenceOps *o, unsigned effect,
                       int loop, char e[256]) {
  if (effect >= BK_ENDING_EFFECTS || !b->effect_volume)
    return fail(e, "invalid ending effect request");
  BkEndingAudioCall call = {.operation = BK_ENDING_AUDIO_RESTART,
                            .slot = effect + 2,
                            .flags = loop ? 1 : 0,
                            .volume = *b->effect_volume};
  int ignored = 0;
  CALL(audio, &call, &ignored, e);
  return 1;
}

static int effect_stop(const BkEndingAuxiliarySequenceOps *o, unsigned effect,
                       char e[256]) {
  if (effect >= BK_ENDING_EFFECTS)
    return fail(e, "invalid ending effect stop");
  BkEndingAudioCall call = {
      .operation = BK_ENDING_AUDIO_PAUSE, .slot = effect + 2};
  int ignored = 0;
  CALL(audio, &call, &ignored, e);
  return 1;
}

static int voice(const BkEndingAuxiliarySequenceBindings *b,
                 const BkEndingAuxiliarySequenceOps *o, int cue, unsigned slot,
                 char e[256]) {
  if (!b->voice_volume)
    return fail(e, "missing voice volume");
  CALL(voice, cue, slot, 0, *b->voice_volume, e);
  return 1;
}

static int active(const BkEndingAuxiliarySequenceOps *o, int32_t *slot,
                 char e[256]) {
  CALL(active, slot, e);
  if (*slot < 0 || *slot >= BK_ENDING_SEQUENCE_CLIP_SLOTS)
    return fail(e, "active clip is outside native slots");
  return 1;
}

static int timing(const BkEndingAuxiliarySequenceOps *o, unsigned slot,
                  BkEndingClipTiming *out, char e[256]) {
  if (slot >= BK_ENDING_SEQUENCE_CLIP_SLOTS)
    return fail(e, "clip timing slot is outside native slots");
  CALL(timing, slot, out, e);
  return 1;
}

static int source_at(const BkEndingAuxiliarySequenceOps *o, unsigned slot,
                     float *source, char e[256]) {
  BkEndingClipTiming t;
  if (!timing(o, slot, &t, e)) return 0;
  *source = t.source;
  return 1;
}

static void set_orbit(BkMenuCamera *camera, const float values[4]) {
  camera->yaw = values[0];
  camera->pitch = values[1];
  camera->radius = values[2];
  camera->height = values[3];
}

static int threshold(const BkEndingAuxiliarySequenceBindings *b,
                     const BkEndingAuxiliarySequenceOps *o, unsigned slot,
                     float bound, unsigned latch, unsigned effect, int loop,
                     char e[256]) {
  float source;
  if (latch >= 10 || !source_at(o, slot, &source, e)) return 0;
  /* Native test ah,1 accepts equality and rejects unordered values. */
  if (source >= bound && !b->latches[latch]) {
    if (!effect_play(b, o, effect, loop, e)) return 0;
    b->latches[latch] = 1;
  }
  return 1;
}

static int state6_clip_effects(const BkEndingAuxiliarySequenceBindings *b,
                                const BkEndingAuxiliarySequenceOps *o,
                                unsigned slot, char e[256]) {
  unsigned g = b->frame->group;
  const unsigned *fx = middle[g];
  int playing = 0;
  if (!threshold(b, o, slot, 133, 1, fx[0], 0, e) ||
      !threshold(b, o, slot, 140, 2, fx[2], 0, e) ||
      !effect_busy(o, fx[2], &playing, e))
    return 0;
  if (!playing && b->latches[2] && !b->latches[8]) {
    if (!effect_play(b, o, fx[1], 0, e)) return 0;
    b->latches[8] = 1;
  }
  return 1;
}

/* The interlocked group-3 effect chain occurs in state5/6/7. */
static int group3_chain(const BkEndingAuxiliarySequenceBindings *b,
                        const BkEndingAuxiliarySequenceOps *o, unsigned slot,
                        char e[256]) {
  const unsigned *fx = final_effect[3];
  int playing = 0;
  if (!threshold(b, o, slot, 152, 0, fx[0], 0, e)) return 0;
  if (b->latches[0] == 1 && !b->latches[1]) {
    if (!effect_busy(o, fx[0], &playing, e)) return 0;
    if (!playing) {
      if (!effect_stop(o, middle[3][0], e) ||
          !effect_play(b, o, fx[1], 0, e))
        return 0;
      b->latches[1] = 1;
    }
  }
  if (!threshold(b, o, slot, 154, 3, fx[3], 0, e)) return 0;
  if (b->latches[1] == 1 && !b->latches[2]) {
    if (!effect_busy(o, fx[1], &playing, e)) return 0;
    if (!playing) {
      if (!effect_play(b, o, fx[2], 0, e)) return 0;
      b->latches[2] = 1;
    }
  }
  if (!threshold(b, o, slot, 160, 4, fx[0], 0, e)) return 0;
  if (b->latches[4] == 1 && !b->latches[5]) {
    if (!effect_busy(o, fx[0], &playing, e)) return 0;
    if (!playing) {
      if (!effect_stop(o, middle[3][1], e) ||
          !effect_play(b, o, fx[2], 0, e))
        return 0;
      b->latches[5] = 1;
    }
  }
  if (!threshold(b, o, slot, 162, 7, fx[3], 0, e)) return 0;
  if (b->latches[5] == 1 && !b->latches[6]) {
    if (!effect_busy(o, fx[2], &playing, e)) return 0;
    if (!playing) {
      if (!effect_play(b, o, fx[4], 0, e)) return 0;
      b->latches[6] = 1;
    }
  }
  return 1;
}

static int finish(const BkEndingAuxiliarySequenceBindings *b, char e[256]) {
  unsigned group = b->frame->group;
  if (*b->previous_flow == 0x18) {
    b->frame->transition_action = 0x31;
  } else if (*b->previous_flow == 8 && b->frame->transition_action != 7) {
    b->frame->transition_action = 7;
    b->auxiliary->selection = group == 2;
    if (!b->records)
      return fail(e, "story record owner is unavailable");
    if (!bk_ending_record_append_unique(
            &b->records->groups[group], group == 2 ? 13 : 12, e))
      return 0;
  }
  b->frame->curtain_wanted = 1;
  *b->substate = 0;
  memset(b->latches, 0, 10);
  memset(b->voice_latches, 0, 2 * sizeof(*b->voice_latches));
  *b->timer = 0;
  *b->pass = 30;
  b->frame->camera_cached = -1;
  b->control->state_721eec = 4;
  return 1;
}

static int state5_substate0(const BkEndingAuxiliarySequenceBindings *b,
                            const BkEndingAuxiliarySequenceOps *o,
                            char e[256]) {
  unsigned g = b->frame->group;
  int playing = 0;
  if (g < 2) {
    if (!effect_busy(o, initial[g], &playing, e)) return 0;
    if (!playing && !b->latches[0]) {
      if (!effect_play(b, o, middle[g][0], 1, e)) return 0;
      b->latches[0] = 1;
    }
  } else if (g == 2) {
    if (!threshold(b, o, 5, 105, 0, middle[g][1], 0, e)) return 0;
  } else if (g == 3) {
    if (!threshold(b, o, 5, 95, 0, middle[g][2], 0, e)) return 0;
    float source;
    if (!source_at(o, 5, &source, e)) return 0;
    if (source > 90) b->latches[0] = 0;
  }

  int32_t current;
  if (!active(o, &current, e)) return 0;
  if (current != 5 && current != 6) return 1;
  BkEndingClipTiming t;
  if (!timing(o, (unsigned)current, &t, e) || t.source < t.end)
    return 1;
  if (!media_busy(o, 0, &playing, e) || playing) return 1;
  if (!o->request || !o->request(o->context, 6, e)) return 0;
  if (g < 2 &&
      (!o->expression || !o->expression(o->context, 0, 4, 0, e)))
    return 0;
  if (!voice(b, o, 8, 0, e)) return 0;
  if (g == 3) {
    if (!effect_stop(o, initial[g] + 1, e) ||
        !effect_stop(o, initial[g] + 2, e) ||
        !effect_play(b, o, initial[g], 1, e) ||
        !effect_play(b, o, initial[g] + 1, 1, e))
      return 0;
  } else if (g < 2 && !effect_play(b, o, middle[g][0], 1, e)) {
    return 0;
  }
  *b->substate = 1;
  memset(b->latches, 0, 10);
  return 1;
}

static int state5_substate1(const BkEndingAuxiliarySequenceBindings *b,
                            const BkEndingAuxiliarySequenceOps *o,
                            char e[256]) {
  unsigned g = b->frame->group;
  if (g == 2) {
    if (!threshold(b, o, 6, 125, 0, middle[g][0], 0, e) ||
        !threshold(b, o, 6, 140, 1, middle[g][1], 0, e))
      return 0;
    float source;
    if (!source_at(o, 6, &source, e)) return 0;
    if (source > 120) b->latches[0] = b->latches[1] = 0;
  } else if (g == 3) {
    BkEndingClipTiming t;
    if (!timing(o, 6, &t, e)) return 0;
    if (t.end - 10.0f > t.source && !b->latches[0]) {
      if (!effect_play(b, o, middle[g][2], 0, e)) return 0;
      b->latches[0] = 1;
    }
    if (t.source > 120) b->latches[0] = 0;
  }
  int playing = 0;
  if (!media_busy(o, 0, &playing, e) || playing) return 1;
  if (b->control->toggles[7] && !voice(b, o, 9, 1, e)) return 0;
  memset(b->latches, 0, 10);
  *b->substate = 2;
  return 1;
}

static int state5_substate2(const BkEndingAuxiliarySequenceBindings *b,
                            const BkEndingAuxiliarySequenceOps *o,
                            char e[256]) {
  unsigned g = b->frame->group;
  if (g == 3) {
    BkEndingClipTiming t;
    if (!timing(o, 6, &t, e)) return 0;
    if (t.end - 10.0f > t.source && !b->latches[0]) {
      if (!effect_play(b, o, middle[g][2], 0, e)) return 0;
      b->latches[0] = 1;
    }
    if (t.source > 120) b->latches[0] = 0;
  }
  int playing = 0;
  if (!media_busy(o, 1, &playing, e) || playing) return 1;
  b->auxiliary->index = g == 2 ? 0x48 : 0x49;
  if (!o->expression || !o->expression(o->context, 5, 4, 0, e)) return 0;
  *b->expression_override = 5;
  *b->face_mode = 1;
  if (!voice(b, o, 10, 0, e) || !o->request ||
      !o->request(o->context, 7, e))
    return 0;
  *b->substate = 3;
  memset(b->latches, 0, 10);
  return 1;
}

static int state5_substate3(const BkEndingAuxiliarySequenceBindings *b,
                            const BkEndingAuxiliarySequenceOps *o,
                            char e[256]) {
  unsigned g = b->frame->group;
  int32_t current;
  if (!active(o, &current, e)) return 0;
  if (current == 7) {
    if (g == 0) {
      float target[3];
      if (!o->target || !o->target(o->context, target, e)) return 0;
      memcpy(b->frame->camera_values, target,
             sizeof(b->frame->camera_values));
      b->control->target_choice = 0;
      b->frame->camera_mode = 2;
      float source;
      if (!source_at(o, 7, &source, e)) return 0;
      if (source >= 220 && b->auxiliary->pending != 100) {
        if (!o->special_audio ||
            !o->special_audio(o->context, *b->effect_volume, e))
          return 0;
        b->auxiliary->pending = 100;
      }
    } else if (g == 2) {
      float source;
      if (!source_at(o, 7, &source, e)) return 0;
      if (source >= 160 && b->latches[0] < 2) {
        int playing = 0;
        if (!effect_busy(o, final_effect[g][1], &playing, e)) return 0;
        if (!playing) {
          if (!effect_play(b, o, final_effect[g][1], 0, e)) return 0;
          ++b->latches[0];
        }
      }
    } else if (g == 3 && !group3_chain(b, o, 7, e)) {
      return 0;
    }
  }

  BkEndingClipTiming t;
  if (!timing(o, (unsigned)current, &t, e) || t.source < t.end)
    return 1;
  if (!o->request) return fail(e, "missing state5 next clip request");
  if (!o->request(o->context, g == 0 ? 9 : 8, e)) return 0;
  if (!active(o, &current, e) || current != 9) return 1;
  if (!o->expression || !o->expression(o->context, 0, 4, 0, e)) return 0;
  *b->expression_override = 0;
  *b->face_mode = 1;
  int playing = 0;
  if (!media_busy(o, 0, &playing, e) || playing) return 1;
  if (!voice(b, o, 11, 0, e)) return 0;
  *b->substate = 4;
  return 1;
}

static int state5(const BkEndingAuxiliarySequenceBindings *b,
                  const BkEndingAuxiliarySequenceOps *o, char e[256]) {
  switch (*b->substate) {
  case 0:
    return state5_substate0(b, o, e);
  case 1:
    return state5_substate1(b, o, e);
  case 2:
    return state5_substate2(b, o, e);
  case 3:
    return state5_substate3(b, o, e);
  case 4: {
    if (b->frame->group == 2) {
      *b->timer = (float)((double)*b->timer + (double)b->seconds);
      if (*b->timer <= 20.0f) return 1;
    } else {
      int playing = 0;
      if (!media_busy(o, 0, &playing, e) || playing) return 1;
    }
    *b->timer = 0;
    return finish(b, e);
  }
  default:
    return 1;
  }
}

static int state6_substate0(const BkEndingAuxiliarySequenceBindings *b,
                            const BkEndingAuxiliarySequenceOps *o,
                            char e[256]) {
  int playing = 0;
  if (!media_busy(o, 0, &playing, e)) return 0;
  if (!playing) {
    if (!b->latches[0]) {
      *b->pass = 0;
      if (!o->request || !o->request(o->context, 6, e)) return 0;
      b->frame->camera_mode = 4;
      if (!voice(b, o, 8, 0, e)) return 0;
      b->control->state_721eec = 7;
      b->saved_orbit[0] = b->camera->yaw;
      b->saved_orbit[1] = b->camera->pitch;
      b->saved_orbit[2] = b->camera->radius;
      b->saved_orbit[3] = b->camera->height;
      *b->saved_toggle = b->control->toggles[1];
      b->control->toggles[1] = 1;
      set_orbit(b->camera, orbit[b->frame->group][0]);
      memcpy(b->saved_target, b->frame->camera_values,
             sizeof(b->frame->camera_values));
      float target[3];
      if (!o->target || !o->target(o->context, target, e)) return 0;
      memcpy(b->frame->camera_values, target, sizeof(target));
      return 1;
    }
    if (b->latches[0] == 1) {
      b->latches[0] = 3;
      if (b->control->toggles[7] && !voice(b, o, 9, 1, e)) return 0;
    } else if (b->latches[0] == 3) {
      if (!media_busy(o, 1, &playing, e)) return 0;
      if (!playing) {
        b->latches[0] = 4;
        if (!voice(b, o, 10, 0, e)) return 0;
      }
    } else if (b->latches[0] == 4) {
      *b->substate = 1;
      if (!voice(b, o, 11, 0, e)) return 0;
    }
  }

  int32_t current;
  if (!active(o, &current, e)) return 0;
  if (current == 6) {
    if (!state6_clip_effects(b, o, 6, e)) return 0;
    BkEndingClipTiming t;
    if (!timing(o, 6, &t, e)) return 0;
    if (t.source >= t.end) {
      if (!media_busy(o, 0, &playing, e)) return 0;
      if (!playing) {
        if (!o->request || !o->request(o->context, 7, e)) return 0;
      }
    }
  } else if (current == 7) {
    float source;
    if (!source_at(o, 7, &source, e)) return 0;
    if (source >= 160 && !b->latches[3]) {
      if (!o->expression || !o->expression(o->context, 0, 4, 0, e))
        return 0;
      *b->expression_override = 0;
      *b->face_mode = 1;
      if (!effect_play(b, o, final_effect[b->frame->group][0], 0, e)) return 0;
      b->latches[3] = 1;
    }
  } else if (current == 8) {
    if (!threshold(b, o, 8, 195, 4, final_effect[b->frame->group][0], 0, e) ||
        !threshold(b, o, 8, 225, 5, final_effect[b->frame->group][0], 0, e))
      return 0;
  } else if (current == 9) {
    if (!threshold(b, o, 9, 257, 6, final_effect[b->frame->group][0], 0, e) ||
        !threshold(b, o, 9, 264, 7, final_effect[b->frame->group][0], 0, e))
      return 0;
    BkEndingClipTiming t;
    if (!timing(o, 9, &t, e)) return 0;
    if (t.start + 6.0f >= t.source)
      b->latches[6] = b->latches[7] = 0;
  }
  return 1;
}

/* 48079D is the state6 substate-1 tail, not the state7 entry.  It keeps
 * servicing the last clip's two effect thresholds while speech0 is playing;
 * once that speech ends it performs the same ordered finish/curtain writes
 * as the state5 tail. */
static int state6_substate1(const BkEndingAuxiliarySequenceBindings *b,
                            const BkEndingAuxiliarySequenceOps *o,
                            char e[256]) {
  int playing = 0;
  if (!media_busy(o, 0, &playing, e)) return 0;
  if (!playing) return finish(b, e);
  int32_t current;
  if (!active(o, &current, e)) return 0;
  if (current != 9) return 1;
  if (!threshold(b, o, 9, 257, 6,
                 final_effect[b->frame->group][0], 0, e) ||
      !threshold(b, o, 9, 264, 7,
                 final_effect[b->frame->group][0], 0, e))
    return 0;
  BkEndingClipTiming t;
  if (!timing(o, 9, &t, e)) return 0;
  if (t.start + 6.0f >= t.source)
    b->latches[6] = b->latches[7] = 0;
  return 1;
}

static int state7_core(const BkEndingAuxiliarySequenceBindings *b,
                       const BkEndingAuxiliarySequenceOps *o, char e[256]) {
  int32_t current;
  if (!active(o, &current, e)) return 0;
    BkEndingClipTiming t;
  if (!timing(o, (unsigned)current, &t, e)) return 0;
  int playing = 1;
  if (t.source >= t.end && !media_busy(o, 0, &playing, e)) return 0;
  if (t.source >= t.end && !playing) {
    ++*b->pass;
    unsigned g = b->frame->group;
    if (*b->pass == 3) {
      if (g < 2) {
        if (!o->expression || !o->expression(o->context, 5, 6, 1, e) ||
            !voice(b, o, 8, 0, e))
          return 0;
        if (!effect_busy(o, middle[g][0], &playing, e)) return 0;
        if (!playing && !effect_play(b, o, middle[g][0], 1, e)) return 0;
        *b->substate = 1;
      }
      if (g < 4) {
        b->control->state_721eec = 5;
        if (!active(o, &current, e) ||
            current + 1 >= BK_ENDING_SEQUENCE_CLIP_SLOTS ||
            !o->request || !o->request(o->context, (unsigned)current + 1, e))
          return 0;
      } else {
        b->latches[0] = 1;
        b->control->state_721eec = 6;
      }
      b->frame->camera_mode = 0;
      b->control->toggles[1] = *b->saved_toggle;
      if (g == 0 || g == 1 || g == 4) {
        set_orbit(b->camera, ordinary[g]);
      } else {
        set_orbit(b->camera, b->saved_orbit);
        memcpy(b->frame->camera_values, b->saved_target,
               sizeof(b->frame->camera_values));
      }
    } else {
      if (!o->restart || !o->restart(o->context, (unsigned)current, e) ||
          !voice(b, o, g < 2 ? 7 : g < 4 ? 10 : 8, 0, e))
        return 0;
      if (g < 2) {
        if (!effect_play(b, o, initial[g], 0, e)) return 0;
      } else if (g == 3) {
        memset(b->latches, 0, 10);
        if (!effect_stop(o, initial[g] + 1, e) ||
            !effect_stop(o, initial[g] + 2, e) ||
            !effect_play(b, o, initial[g], 1, e) ||
            !effect_play(b, o, initial[g] + 1, 1, e))
          return 0;
      } else {
        memset(b->latches, 0, 10);
      }
      if (*b->pass < 0 || *b->pass >= 3)
        return fail(e, "camera pass is outside native table");
      set_orbit(b->camera, orbit[g][*b->pass]);
    }
  }

  unsigned g = b->frame->group;
  if (!active(o, &current, e)) return 0;
  if (g == 2 && current == 7) {
    float source;
    if (!source_at(o, 7, &source, e)) return 0;
    if (source >= 160 && b->latches[0] < 2) {
      if (!effect_busy(o, final_effect[g][1], &playing, e)) return 0;
      if (!playing) {
        if (!effect_play(b, o, final_effect[g][1], 0, e)) return 0;
        ++b->latches[0];
      }
    }
  } else if (g == 3 && current == 7) {
    if (!group3_chain(b, o, 7, e)) return 0;
  } else if (g == 4 && current == 6) {
    if (!state6_clip_effects(b, o, 6, e)) return 0;
  }
  return 1;
}

static int state6(const BkEndingAuxiliarySequenceBindings *b,
                  const BkEndingAuxiliarySequenceOps *o, char e[256]) {
  if (*b->substate == 1)
    return state6_substate1(b, o, e); /*4800ca jumps directly to48079d.*/
  if (*b->substate != 0) return 1;
  return state6_substate0(b, o, e);
}

int bk_ending_auxiliary_sequence_step(
    const BkEndingAuxiliarySequenceBindings *b,
    const BkEndingAuxiliarySequenceOps *o, char e[256]) {
  if (!b || !o || !b->frame || !b->control || !b->auxiliary || !b->camera ||
      !b->substate || !b->latches || !b->saved_toggle ||
      !b->voice_latches || !b->timer || !b->pass || !b->saved_orbit ||
      !b->saved_target || !b->expression_override || !b->face_mode ||
      !b->previous_flow || !b->voice_volume || !b->effect_volume ||
      !isfinite(b->seconds) || b->seconds < 0 || b->frame->group >= 5)
    return fail(e, "invalid state5/6/7 bindings");
  if (b->control->state_721eec == 5) return state5(b, o, e);
  if (b->control->state_721eec == 6) return state6(b, o, e);
  if (b->control->state_721eec == 7) return state7_core(b, o, e);
  return fail(e, "called outside state5/6/7");
}

#undef CALL
