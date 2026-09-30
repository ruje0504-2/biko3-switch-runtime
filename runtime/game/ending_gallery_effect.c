#include "game/ending_gallery_effect.h"
#include "game/ending_selected_auxiliary.h"
#include <math.h>
#include <stdio.h>

static int fail(char e[256], const char *why) {
  if (e) snprintf(e, 256, "gallery effect: %s", why);
  return 0;
}
BkEndingGalleryEffectState bk_ending_gallery_effect_initial(void) {
  return (BkEndingGalleryEffectState){0};
}
static int audio(const BkEndingGalleryEffectOps *o, BkEndingAudioOperation op,
                 unsigned slot, int32_t cue, int32_t bank, int32_t volume,
                 int *playing, char e[256]) {
  BkEndingAudioCall call = {op, slot, cue, bank, 0, volume};
  return o->audio ? o->audio(o->context, &call, playing, e)
                  : fail(e, "missing audio service");
}
static int read_cue(const BkEndingGalleryEffectBindings *b, unsigned index,
                     int32_t *out, char e[256]) {
  /*54f8f4 and5546ac contain the same60 words in the fixed EXE.*/
  return bk_ending_selected_cue((unsigned)b->auxiliary->variant, index, out)
             ? 1 : fail(e, "voice table outside original two rows");
}
int bk_ending_gallery_effect_step(const BkEndingGalleryEffectBindings *b,
    float seconds, const BkEndingGalleryEffectOps *o, char e[256]) {
  if (!b || !b->frame || !b->auxiliary || !b->event || !b->state ||
      !b->voice_volume || !b->effect_volume || !o ||
      !isfinite(seconds) || seconds < 0)
    return fail(e, "invalid bindings/time");
#define CALL(member, ...) do { \
  if (!o->member) return fail(e, "missing " #member " service"); \
  if (!o->member(o->context, __VA_ARGS__)) return 0; \
} while (0)
  BkEndingGalleryEffectState *s = b->state;
  if (s->speech_blocked == 1) {
    s->speech_elapsed = (float)((double)s->speech_elapsed + (double)seconds);
    if (s->speech_elapsed > 6.f) {
      s->speech_blocked = 0;
      s->speech_elapsed = 0;
    }
  }
  int near = 0;
  int32_t active;
  BkEndingClipTiming t;
  unsigned group = b->frame->group, event = *b->event;
  if ((group == 0 && event == 2) || (group == 2 && event == 8)) {
    CALL(active, &active, e);
    if (active == 6 || active == 7 || active == 10 || active == 11) {
      const unsigned slots[] = {6, 10};
      for (unsigned i = 0; i < 2 && !near; ++i) {
        CALL(timing, slots[i], &t, e);
        near = group == 0 ? !((double)t.end - 1.0 > (double)t.source)
                          : t.source >= t.end;
      }
      if (!near) s->sampled = 0;
    } else if (active == 17 || active == 18) {
      CALL(timing, 17, &t, e);
      if (t.source >= 380.f && !s->alternating) {
        near = 1;
        s->alternating = 1;
      } else s->sampled = 0;
      /*Independent second comparison: it can clear sampled even after
       *the first one triggers. Do not merge this with the first branch.*/
      CALL(timing, 18, &t, e);
      if (t.source >= 400.f && s->alternating == 1) {
        near = 1;
        s->alternating = 0;
      } else s->sampled = 0;
    }
  } else if (group <= 4) {
    unsigned slots[] = {6, 10, 17, 18}, count = 3;
    if (group == 0 && event == 4) { slots[1] = 11; slots[2] = 18; }
    else if (group == 1) {
      if (event == 2) { slots[0] = 7; slots[1] = 11; slots[2] = 18; }
      else if (event == 3 || event == 4) { slots[1] = 11; slots[2] = 18; }
      else if (event == 7) slots[1] = 11;
    } else if (group == 2 && event == 9) count = 4;
    for (unsigned i = 0; i < count && !near; ++i) {
      CALL(timing, slots[i], &t, e);
      near = !((double)t.end - 1.0 > (double)t.source);
    }
    if (!near) s->sampled = 0;
  }
  if (!near || s->sampled) return 1;

  int playing = 0, have_cue = 0;
  int32_t cue = 0;
  if (!audio(o, BK_ENDING_AUDIO_RESTART, 4, 0, 0, *b->effect_volume,
               &playing, e)) return 0;
  CALL(active, &active, e);
  int family = active == 6 || active == 7 ? 0
               : active == 10 || active == 11 ? 1 : -1;
  if (family >= 0) {
    unsigned index;
    if (!b->auxiliary->variant) index = family ? 20 : 8;
    else {
      int32_t random;
      CALL(random, &random, e); /*49717c reads variant after RNG.*/
      index = (family ? 14u : 8u) + (random % 2 ? 12u : 0u);
    }
    if (!read_cue(b, index, &cue, e)) return 0;
    have_cue = 1;
  }
  if (!audio(o, BK_ENDING_AUDIO_STATUS, 0, 0, 0, 0, &playing, e)) return 0;
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
        if (!audio(o, BK_ENDING_AUDIO_CUE, 0, cue, bank,
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
      if (bank && !audio(o, BK_ENDING_AUDIO_CUE, 0, cue, bank,
                           *b->voice_volume, &playing, e)) return 0;
    }
    s->speech_blocked = 1;
  }
  s->sampled = 1;
#undef CALL
  return 1;
}
