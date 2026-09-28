#include "scene/outcome_audio.h"
#include <stdlib.h>
struct BkOutcomeAudio {
  BkAudio *audio;
  BkAudioClip *clips[2];
  unsigned voices[2];
  int32_t volume;
  uint8_t submitted[2];
};
static int fail(char *e, const char *why) {
  snprintf(e, 256, "outcome audio: %s", why);
  return 0;
}
BkOutcomeAudio *bk_outcome_audio_create(BkResourceStore *store, BkAudio *audio,
                                        unsigned outcome, unsigned response,
                                        int32_t volume, char e[256]) {
  if (!store || !audio || outcome >= BK_AUDIO_VOICES ||
      response >= BK_AUDIO_VOICES || outcome == response || volume < -10000 ||
      volume > 0 || bk_audio_stats(audio).failed) {
    fail(e, "invalid services/voices/volume");
    return NULL;
  }
  BkOutcomeAudio *a = calloc(1, sizeof(*a));
  if (!a) {
    fail(e, "allocation failed");
    return NULL;
  }
  a->audio = audio;
  a->voices[0] = outcome;
  a->voices[1] = response;
  a->volume = volume;
  a->clips[0] = bk_audio_clip_load(store, "bk3_02", "se100.wav", e);
  if (a->clips[0])
    a->clips[1] = bk_audio_clip_load(store, "bk3_02", "se007.wav", e);
  if (!a->clips[1]) {
    bk_outcome_audio_destroy(a);
    return NULL;
  }
  return a;
}
void bk_outcome_audio_destroy(BkOutcomeAudio *a) {
  if (a) {
    bk_audio_clip_release(a->clips[0]);
    bk_audio_clip_release(a->clips[1]);
    free(a);
  }
}
int bk_outcome_audio_stop(BkOutcomeAudio *a, char e[256]) {
  if (!a)
    return fail(e, "missing instance");
  for (unsigned i = 0; i < 2; ++i) {
    if (!bk_audio_clear(a->audio, a->voices[i], e))
      return 0;
    a->submitted[i] = 0;
  }
  return 1;
}
static int play(BkOutcomeAudio *a, unsigned i, char e[256]) {
  if (!a)
    return fail(e, "missing instance");
  if (a->submitted[i])
    return bk_audio_resume(a->audio, a->voices[i], 0, e);
  if (!bk_audio_play(a->audio, a->voices[i], a->clips[i], 0, a->volume, 0, e))
    return 0;
  a->submitted[i] = 1;
  return 1;
}
int bk_outcome_audio_play_outcome(void *p, char e[256]) {
  return play(p, 0, e);
}
int bk_outcome_audio_play_response(void *p, char e[256]) {
  return play(p, 1, e);
}
