#include "game/ending_auxiliary.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
static int fail(char e[256], const char *why) {
  snprintf(e, 256, "ending auxiliary: %s", why);
  return 0;
}
static int expression(BkEndingAuxiliaryState *s, const BkEndingAuxiliaryOps *o,
                      int32_t a, int32_t b, char e[256]) {
  s->expression_a = a;
  s->expression_b = b;
  return o->eyes ? o->eyes(o->context, 1, e)
                 : fail(e, "missing eye-selection service");
}
static int alternate_expression(BkEndingAuxiliaryState *s,
                                const BkEndingFrameState *f,
                                const BkEndingAuxiliaryOps *o, char e[256]) {
  return f->group == 4 && f->phase == 6 ? expression(s, o, 5, 11, e)
                                        : expression(s, o, 1, 4, e);
}
static void index_set(BkEndingAuxiliaryState *s, unsigned delta) {
  uint32_t bits = (uint32_t)s->base + (uint32_t)s->selection * 6u + delta;
  memcpy(&s->index, &bits, sizeof(bits));
}
static int audio(const BkEndingAuxiliaryOps *o, BkEndingAudioOperation kind,
                 unsigned slot, int32_t cue, int32_t bank, int32_t flags,
                 int32_t volume, int *playing, char e[256]) {
  BkEndingAudioCall c = {kind, slot, cue, bank, flags, volume};
  return o->audio ? o->audio(o->context, &c, playing, e)
                  : fail(e, "missing audio service");
}
static int start_loop(const BkEndingAuxiliaryOps *o, int32_t volume,
                      char e[256]) {
  int playing = 0;
  return audio(o, BK_ENDING_AUDIO_STATUS, 5, 0, 0, 0, 0, &playing, e) &&
         (playing ||
          audio(o, BK_ENDING_AUDIO_RESTART, 5, 0, 0, 1, volume, &playing, e));
}
int bk_ending_auxiliary_change(BkEndingAuxiliaryState *s, BkEndingFrameState *f,
                               int32_t proposed, int32_t voice_volume,
                               int32_t effect_volume,
                               const BkEndingAuxiliaryOps *o, int32_t *result,
                               char e[256]) {
  if (!s || !f || !o || !result)
    return fail(e, "missing live state/services");
  *result = 0;
  if (s->gate != 1)
    return 1;
  int32_t slot;
  if (!o->active)
    return fail(e, "missing active-clip service");
  if (!o->active(o->context, &slot, e))
    return 0;
  if (slot >= 1 && slot <= 3)
    return 1;
  if ((uint32_t)proposed > 3) {
    *result = 1;
    return 1;
  }
  if (!o->write || !o->request || !o->audio || !o->eyes || s->variant < 0 ||
      s->variant > 1 || f->group >= 5)
    return fail(e, "invalid transition services/profile");
  int playing = 0;
#define WRITE(i, k, v)                                                         \
  do {                                                                         \
    if (!o->write(o->context, i, k, v, e))                                     \
      return 0;                                                                \
  } while (0)
#define REQUEST(i)                                                             \
  do {                                                                         \
    if (!o->request(o->context, i, e))                                         \
      return 0;                                                                \
  } while (0)
#define VOICE(c)                                                               \
  do {                                                                         \
    if (!audio(o, BK_ENDING_AUDIO_VOICE, 0, c, 0, 0, voice_volume, &playing,   \
               e))                                                             \
      return 0;                                                                \
  } while (0)
  if (proposed == 0) {
    const unsigned slots[] = {5, 6, 7, 9, 10, 11, 13, 14};
    for (unsigned i = 0; i < 8; i++)
      WRITE(slots[i], BK_ENDING_CLIP_CHAIN, 0);
    REQUEST(4);
    for (unsigned i = 2; i < 5; i++) {
      if (!audio(o, BK_ENDING_AUDIO_STATUS, i, 0, 0, 0, 0, &playing, e) ||
          (playing &&
           !audio(o, BK_ENDING_AUDIO_PAUSE, i, 0, 0, 0, 0, &playing, e)))
        return 0;
    }
    if (!audio(o, BK_ENDING_AUDIO_STATUS, 5, 0, 0, 0, 0, &playing, e) ||
        (playing &&
         !audio(o, BK_ENDING_AUDIO_PAUSE, 5, 0, 0, 0, 0, &playing, e)) ||
        !expression(s, o, 6, 3, e))
      return 0;
    int band = -1;
    /* x87 C0 is also set on unordered (NaN), hence negated >=. */
    if (!(s->progress >= .6f))
      band = 0;
    else if (s->progress >= .59f && !(s->progress >= .8f))
      band = 1;
    else if (s->progress >= .79f)
      band = 2;
    if (band >= 0 &&
        !audio(o, BK_ENDING_AUDIO_CUE, 0, 1 + 3 * s->variant + band, 2, 3,
               voice_volume, &playing, e))
      return 0;
    index_set(s, 1);
    s->pending = 1;
  } else if (proposed == 1) {
    WRITE(10, BK_ENDING_CLIP_REWIND, 0);
    WRITE(11, BK_ENDING_CLIP_REWIND, 0);
    WRITE(5, BK_ENDING_CLIP_CHAIN, 1);
    WRITE(6, BK_ENDING_CLIP_CHAIN, 1);
    WRITE(6, BK_ENDING_CLIP_NEXT, 7);
    WRITE(7, BK_ENDING_CLIP_CHAIN, 1);
    WRITE(7, BK_ENDING_CLIP_NEXT, 6);
    REQUEST(5);
    VOICE(5);
    index_set(s, 2);
    if (!expression(s, o, 3, 3, e) || !audio(o, BK_ENDING_AUDIO_RESTART, 5, 0,
                                             0, 1, effect_volume, &playing, e))
      return 0;
    f->camera_cached = -1;
  } else if (proposed == 2) {
    WRITE(6, BK_ENDING_CLIP_REWIND, 0);
    WRITE(7, BK_ENDING_CLIP_REWIND, 0);
    WRITE(5, BK_ENDING_CLIP_CHAIN, 0);
    WRITE(6, BK_ENDING_CLIP_CHAIN, 0);
    WRITE(7, BK_ENDING_CLIP_CHAIN, 0);
    WRITE(9, BK_ENDING_CLIP_CHAIN, 1);
    WRITE(10, BK_ENDING_CLIP_CHAIN, 1);
    WRITE(10, BK_ENDING_CLIP_NEXT, 11);
    WRITE(11, BK_ENDING_CLIP_CHAIN, 1);
    WRITE(11, BK_ENDING_CLIP_NEXT, 10);
    REQUEST(9);
    VOICE(7);
    index_set(s, 3);
    if (!alternate_expression(s, f, o, e) || !start_loop(o, effect_volume, e))
      return 0;
    f->camera_cached = -1;
  } else {
    WRITE(6, BK_ENDING_CLIP_REWIND, 0);
    WRITE(7, BK_ENDING_CLIP_REWIND, 0);
    WRITE(10, BK_ENDING_CLIP_REWIND, 0);
    WRITE(11, BK_ENDING_CLIP_REWIND, 0);
    WRITE(6, BK_ENDING_CLIP_CHAIN, 1);
    WRITE(6, BK_ENDING_CLIP_NEXT, 7);
    WRITE(7, BK_ENDING_CLIP_CHAIN, 1);
    WRITE(7, BK_ENDING_CLIP_NEXT, 6);
    WRITE(9, BK_ENDING_CLIP_CHAIN, 0);
    WRITE(10, BK_ENDING_CLIP_CHAIN, 1);
    WRITE(10, BK_ENDING_CLIP_NEXT, 11);
    WRITE(11, BK_ENDING_CLIP_CHAIN, 1);
    WRITE(11, BK_ENDING_CLIP_NEXT, 10);
    if (slot >= 5 && slot <= 7) {
      REQUEST(13);
      VOICE(9);
      if (!expression(s, o, 3, 3, e))
        return 0;
      index_set(s, 3);
      s->direction = 1;
    } else if (slot >= 9 && slot <= 11) {
      REQUEST(14);
      index_set(s, 2);
      s->direction = 0;
      VOICE(10);
      if (!alternate_expression(s, f, o, e))
        return 0;
    }
    WRITE(13, BK_ENDING_CLIP_CHAIN, 1);
    WRITE(13, BK_ENDING_CLIP_NEXT, 10);
    WRITE(14, BK_ENDING_CLIP_CHAIN, 1);
    WRITE(14, BK_ENDING_CLIP_NEXT, 6);
    if (!start_loop(o, effect_volume, e))
      return 0;
    f->camera_cached = -1;
  }
#undef WRITE
#undef REQUEST
#undef VOICE
  *result = 1;
  return 1;
}
BkEndingAuxiliaryCycle bk_ending_auxiliary_cycle_initial(void) {
  return (BkEndingAuxiliaryCycle){.scale = .02f, .countdown = 10};
}
int bk_ending_auxiliary_tick(BkEndingAuxiliaryState *s, BkEndingFrameState *f,
                             BkEndingAuxiliaryCycle *cycle, float seconds,
                             const int32_t *voice_volume,
                             const BkEndingAuxiliaryTickOps *ops, char e[256]) {
  if (!s || !f || !cycle || !voice_volume || !ops || !isfinite(seconds) ||
      seconds < 0)
    return fail(e, "invalid automatic-cycle bindings/time");
  const BkEndingAuxiliaryOps *o = &ops->auxiliary;
  f->camera_cached = -1;
  f->camera_event = 0;
  int reverse = s->direction != 0;
  int32_t slot;
  if (!o->active)
    return fail(e, "missing active-clip service");
  if (!o->active(o->context, &slot, e))
    return 0;
  if (slot != (reverse ? 11 : 7))
    return 1;
  BkEndingAuxiliaryPrediction prediction;
  if (!ops->prediction)
    return fail(e, "missing clip look-ahead service");
  if (!ops->prediction(o->context, (unsigned)slot, &prediction, e))
    return 0;
  double ahead = reverse ? (double)seconds : 60.0 * (double)seconds;
  ahead *= (double)prediction.rate;
  ahead *= prediction.duration;
  ahead *= (double)cycle->scale;
  if (reverse)
    ahead *= 60.0;
  /* Original test accepts equality and unordered, not just source>=end. */
  if ((double)prediction.end - ahead > (double)prediction.source)
    return 1;
  uint8_t byte = (uint8_t)((uint8_t)cycle->countdown - 1u);
  memcpy(&cycle->countdown, &byte, 1);
  if (cycle->countdown > 0)
    return 1;
  if (!o->request)
    return fail(e, "missing configured clip request");
  if (!o->request(o->context, reverse ? 14 : 13, e))
    return 0;
  s->direction = reverse ? 0 : 1;
  index_set(s, reverse ? 2 : 3);
  int32_t random;
  if (!ops->random)
    return fail(e, "missing shared random service");
  if (!ops->random(o->context, &random, e))
    return 0;
  byte = (uint8_t)(random % 11 + 10);
  memcpy(&cycle->countdown, &byte, 1);
  int playing = 0;
  if (!audio(o, BK_ENDING_AUDIO_VOICE, 0, reverse ? 10 : 9, 0, 0,
             *voice_volume, &playing, e))
    return 0;
  if (!reverse && !alternate_expression(s, f, o, e))
    return 0;
  if (!o->write)
    return fail(e, "missing source-rewind service");
  unsigned first = reverse ? 10 : 6;
  if (!o->write(o->context, first, BK_ENDING_CLIP_REWIND, 0, e) ||
      !o->write(o->context, first + 1, BK_ENDING_CLIP_REWIND, 0, e))
    return 0;
  return !reverse || expression(s, o, 3, 3, e);
}
int bk_ending_audio_resource(unsigned group, int32_t variant, int32_t selection,
                             const BkEndingAudioCall *c, char pack[16],
                             char name[32], char e[256]) {
  if (!c || !pack || !name || group >= 5 || variant < 0 || variant > 1 ||
      c->slot >= 6 || c->cue < 0 || c->cue > 99 ||
      (c->operation != BK_ENDING_AUDIO_VOICE &&
       c->operation != BK_ENDING_AUDIO_CUE))
    return fail(e, "invalid sound identity");
  if (c->operation == BK_ENDING_AUDIO_VOICE && group == 0 && c->cue == 30 &&
      variant) {
    strcpy(pack, "bk3_02");
    strcpy(name, "se301.wav");
    return 1;
  }
  if (c->operation == BK_ENDING_AUDIO_VOICE && (selection < 0 || selection > 9))
    return fail(e, "invalid voice selection");
  int bank = c->operation == BK_ENDING_AUDIO_VOICE ? selection + 1 : c->bank;
  if (bank < 0 || bank > 10)
    return fail(e, "invalid sound bank");
  strcpy(pack, "bk3_06");
  snprintf(name, 32, "PH%u%d%d%02d.wav", group + 1,
           (c->operation == BK_ENDING_AUDIO_VOICE ? 2 : 1) + variant * 3, bank,
           c->cue);
  return 1;
}
