#include "game/ending_selected_auxiliary.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static int fail(char e[256], const char *why) {
  if (e) snprintf(e, 256, "selected auxiliary: %s", why);
  return 0;
}
int bk_ending_selected_cue(unsigned variant, unsigned index, int32_t *out) {
  static const int32_t values[2][30] = {
      {1,2,3,1,2,3,1,2,3,4,5,6,1,2,3,4,5,6,1,2,3,4,5,6,1,2,3,4,5,6},
      {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30}};
  if (!out || variant >= 2 || index >= 30) return 0;
  *out = values[variant][index];
  return 1;
}
BkEndingSelectedCycle bk_ending_selected_cycle_initial(void) {
  return (BkEndingSelectedCycle){.countdown = 8};
}
static int audio(const BkEndingSelectedCycleOps *o, BkEndingAudioOperation op,
                 unsigned slot, int32_t cue, int32_t bank, int32_t flags,
                 int32_t volume, int *playing, char e[256]) {
  BkEndingAudioCall call = {op, slot, cue, bank, flags, volume};
  return o->audio ? o->audio(o->context, &call, playing, e)
                  : fail(e, "missing audio service");
}
static int read_cue(const BkEndingSelectedCycleBindings *b, unsigned index,
                     int32_t *out, char e[256]) {
  return bk_ending_selected_cue((unsigned)b->auxiliary->variant, index, out)
             ? 1 : fail(e, "voice table outside original two rows");
}
int bk_ending_selected_cycle_step(const BkEndingSelectedCycleBindings *b,
                                   float seconds, const BkEndingSelectedCycleOps *o,
                                   char e[256]) {
  if (!b || !b->frame || !b->auxiliary || !b->event || !b->cycle || !b->scale ||
      !b->previous_sound || !b->voice_volume || !b->effect_volume || !o ||
      !isfinite(seconds) || seconds < 0)
    return fail(e, "invalid cycle bindings/time");
#define CALL(member, ...) do { \
  if (!o->member) return fail(e, "missing " #member " service"); \
  if (!o->member(o->context, __VA_ARGS__)) return 0; \
} while (0)
  BkEndingSelectedCycle *s = b->cycle;
  if (s->speech_blocked == 1) {
    s->speech_elapsed = (float)((double)s->speech_elapsed + (double)seconds);
    if (s->speech_elapsed > 6.f) {
      s->speech_blocked = 0;
      s->speech_elapsed = 0;
    }
  }
  b->frame->camera_cached = -1;
  b->frame->camera_event = 0;
  int32_t active;
  CALL(active, &active, e);
  if (active == 11 || active == 7) {
    BkEndingAuxiliaryPrediction p;
    CALL(prediction, (unsigned)active, &p, e);
    double ahead = (double)seconds * (double)p.rate;
    ahead *= p.duration;
    ahead *= (double)*b->scale;
    ahead *= 60.0;
    if (!((double)p.end - ahead > (double)p.source)) {
      uint8_t byte = (uint8_t)((uint8_t)s->countdown - 1u);
      memcpy(&s->countdown, &byte, 1);
      if (s->countdown == 0) {
        if (s->slow) { *b->scale = .02f; s->slow = 0; }
        else { *b->scale = .01f; s->slow = 1; }
        int32_t random;
        CALL(random, &random, e);
        byte = (uint8_t)(random % 8 + 8);
        memcpy(&s->countdown, &byte, 1);
      }
    }
  }
  unsigned first = 6, second = 10;
  if (b->frame->group == 0) {
    if (*b->event == 4) second = 11;
  } else if (b->frame->group == 1) {
    if (*b->event == 2) { first = 7; second = 11; }
    else if (*b->event == 7 || *b->event == 3 || *b->event == 4) second = 11;
  } else if (b->frame->group > 4) {
    return fail(e, "original comparison clips undefined for group");
  }
  BkEndingAuxiliaryPrediction p;
  CALL(prediction, first, &p, e);
  int near = !((double)p.end - 1.0 > (double)p.source);
  if (!near) {
    CALL(prediction, second, &p, e);
    near = !((double)p.end - 1.0 > (double)p.source);
  }
  if (!near) s->sampled = 0;
  if (!near || s->sampled) return 1;

  int band = -1;
  if (!(b->auxiliary->progress >= .6f)) band = 0;
  else if (b->auxiliary->progress >= .59f && !(b->auxiliary->progress >= .8f)) band = 1;
  else if (b->auxiliary->progress >= .79f) band = 2;
  int32_t cue = 0;
  int have_cue = 0, playing = 0;
  if (band >= 0) {
    /*5546a0=-1 legitimately refers to speech1 immediately before effect0.
     *Limit only reads outside this actual six-buffer owner.*/
    int64_t previous = band == 0 ? 2 : (int64_t)*b->previous_sound + 2;
    if (previous < 0 || previous >= 6) return fail(e, "previous sound outside six-slot owner");
    if (!audio(o, BK_ENDING_AUDIO_PAUSE, (unsigned)previous, 0, 0, 0, 0, &playing, e) ||
        !audio(o, BK_ENDING_AUDIO_RESTART, (unsigned)band + 2, 0, 0, 0,
                *b->effect_volume, &playing, e)) return 0;
    CALL(active, &active, e);
    unsigned family = active == 6 || active == 7 ? 0
                       : active == 10 || active == 11 ? 1 : 2;
    if (family < 2) {
      unsigned index;
      if (!b->auxiliary->variant) {
        index = (family ? 18u : 6u) + (unsigned)band;
      } else {
        int32_t random;
        CALL(random, &random, e); /*49717c reads variant AFTER this call.*/
        index = (family ? 12u : 6u) + (unsigned)band + (random % 2 ? 12u : 0u);
      }
      if (!read_cue(b, index, &cue, e)) return 0;
      have_cue = 1;
    }
    *b->previous_sound = band;
  }
  if (!audio(o, BK_ENDING_AUDIO_STATUS, 0, 0, 0, 0, 0, &playing, e)) return 0;
  if (!playing && !s->speech_blocked) {
    if (!b->auxiliary->variant) {
      CALL(active, &active, e);
      int32_t bank = active == 6 || active == 7 ? 3
                     : active == 10 || active == 11 ? 5 : 0;
      if (bank) {
        int32_t random;
        CALL(random, &random, e);
        bank += random % 2;
        if (!have_cue) return fail(e, "original cue is uninitialized for active clip");
        if (!audio(o, BK_ENDING_AUDIO_CUE, 0, cue, bank, 0,
                    *b->voice_volume, &playing, e)) return 0;
      }
    } else {
      if (!have_cue) return fail(e, "original cue is uninitialized for active clip");
      const unsigned offsets[] = {6, 18, 12, 24};
      const int32_t banks[] = {3, 5, 4, 6};
      int32_t bank = 0;
      for (unsigned row = 0; row < 4 && !bank; ++row)
        for (unsigned column = 0; column < 3; ++column) {
          int32_t value;
          if (!read_cue(b, offsets[row] + column, &value, e)) return 0;
          if (value == cue) { bank = banks[row]; break; }
        }
      if (bank && !audio(o, BK_ENDING_AUDIO_CUE, 0, cue, bank, 0,
                           *b->voice_volume, &playing, e)) return 0;
    }
    s->speech_blocked = 1;
  }
  s->sampled = 1;
#undef CALL
  return 1;
}
int bk_ending_selected_manual(BkEndingAuxiliaryState *s, int32_t proposed,
                                const BkEndingSelectedManualOps *o,
                                int32_t *accepted, char e[256]) {
  if (!s || !o || !accepted) return fail(e, "invalid manual bindings");
  *accepted = 0;
  if (s->gate != 1) return 1;
  if (!o->active) return fail(e, "missing active service");
  int32_t active;
  if (!o->active(o->context, &active, e)) return 0;
  if (active >= 1 && active <= 3) return 1;
  if ((uint32_t)proposed > 3) { *accepted = 1; return 1; }
#define CALL(member, ...) do { \
  if (!o->member) return fail(e, "missing " #member " service"); \
  if (!o->member(o->context, __VA_ARGS__)) return 0; \
} while (0)
  unsigned delta;
  if (!proposed) {
    const unsigned slots[] = {6, 7, 10, 11, 13, 14, 17, 18};
    for (unsigned i = 0; i < 8; ++i)
      CALL(write, slots[i], BK_ENDING_CLIP_CHAIN, 0, e);
    CALL(request_ten, 4, e);
    if (!o->audio) return fail(e, "missing audio service");
    for (unsigned i = 2; i < 5; ++i) {
      int playing = 0;
      BkEndingAudioCall call = {BK_ENDING_AUDIO_STATUS, i, 0, 0, 0, 0};
      if (!o->audio(o->context, &call, &playing, e)) return 0;
      if (playing) {
        call.operation = BK_ENDING_AUDIO_PAUSE;
        if (!o->audio(o->context, &call, &playing, e)) return 0;
      }
    }
    delta = 1;
  } else {
    const unsigned first[] = {0, 6, 10, 17};
    unsigned slot = first[proposed];
    CALL(write, slot, BK_ENDING_CLIP_CHAIN, 1, e);
    CALL(write, slot + 1, BK_ENDING_CLIP_CHAIN, 1, e);
    CALL(request_ten, slot, e);
    delta = (unsigned)proposed + 1;
  }
  uint32_t index = (uint32_t)s->base + (uint32_t)s->selection * 6u + delta;
  memcpy(&s->index, &index, 4);
  *accepted = 1;
#undef CALL
  return 1;
}
