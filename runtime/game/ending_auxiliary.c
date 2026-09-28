#include "game/ending_auxiliary.h"
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
  return o->eyes(o->context, 1, e);
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
  return o->audio(o->context, &c, playing, e);
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
