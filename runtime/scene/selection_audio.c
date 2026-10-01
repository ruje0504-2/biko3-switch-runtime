#include "scene/selection_audio.h"
#include <stdlib.h>
struct BkSelectionAudio {
  BkAudio *audio;
  BkAudioClip *music_clip, *speech_clip;
  unsigned music, speech;
  int speech_submitted;
};
static int fail(char error[256], const char *why) {
  snprintf(error, 256, "selection audio: %s", why);
  return 0;
}
void bk_selection_audio_destroy(BkSelectionAudio *a) {
  if (a) {
    bk_audio_clip_release(a->music_clip);
    free(a);
  }
}
BkSelectionAudio *bk_selection_audio_create(BkResourceStore *store,
                                            BkAudio *audio, unsigned music,
                                            unsigned speech, int32_t volume,
                                            char error[256]) {
  if (!store || !audio || music >= BK_AUDIO_VOICES ||
      speech >= BK_AUDIO_VOICES || music == speech || volume < -10000 ||
      volume > 0 || bk_audio_stats(audio).failed) {
    fail(error, "invalid services/voices/volume");
    return NULL;
  }
  BkSelectionAudio *a = calloc(1, sizeof(*a));
  if (!a) {
    fail(error, "allocation failed");
    return NULL;
  }
  a->audio = audio;
  a->music = music;
  a->speech = speech;
  a->music_clip = bk_audio_clip_load_music(store, "bk3_02", "bg002.wav", error);
  if (!a->music_clip)
    goto bad;
  if (!bk_audio_clear(audio, speech, error) ||
      !bk_audio_play(audio, music, a->music_clip, 1, volume, 0, error))
    goto bad;
  return a;
bad:
  bk_selection_audio_destroy(a);
  return NULL;
}
int bk_selection_audio_release_voice(BkSelectionAudio *a, char error[256]) {
  if (!a)
    return fail(error, "missing owner");
  if (!bk_audio_clear(a->audio, a->speech, error))
    return 0;
  a->speech_clip = NULL;
  a->speech_submitted = 0;
  return 1;
}
int bk_selection_audio_stop(BkSelectionAudio *a, char error[256]) {
  return a ? bk_selection_audio_release_voice(a, error) &&
                 bk_audio_clear(a->audio, a->music, error)
           : fail(error, "missing owner");
}
int bk_selection_audio_bind_voice(BkSelectionAudio *a, BkAudioClip *voice,
                                  int32_t volume, char error[256]) {
  if (!a || !voice)
    return fail(error, "missing owner/voice");
  if (volume < -10000 || volume > 0)
    return fail(error, "invalid voice gain");
  /* Never play then pause: the independent audio pump could submit speech
   * between those two commands. A newly loaded native buffer is stopped. */
  if (!bk_audio_clear(a->audio, a->speech, error))
    return 0;
  a->speech_clip = voice;
  a->speech_submitted = 0;
  return 1;
}
int bk_selection_audio_music_gain(BkSelectionAudio *a, int32_t volume,
                                  char error[256]) {
  return a ? bk_audio_gain(a->audio, a->music, volume, 0, error)
           : fail(error, "missing owner");
}
int bk_selection_audio_voice_play(BkSelectionAudio *a, int32_t volume,
                                  char error[256]) {
  if (!a || !a->speech_clip)
    return fail(error, "missing owner/bound voice");
  if (!bk_audio_play(a->audio, a->speech, a->speech_clip, 0, volume, 0, error))
    return 0;
  a->speech_submitted = 1;
  return 1;
}
int bk_selection_audio_voice_stop(BkSelectionAudio *a, char error[256]) {
  if (!a || !a->speech_clip)
    return fail(error, "missing owner/bound voice");
  return a->speech_submitted ? bk_audio_pause(a->audio, a->speech, error)
                             : bk_audio_clear(a->audio, a->speech, error);
}
int bk_selection_audio_voice_status(BkSelectionAudio *a, int *playing,
                                    char error[256]) {
  if (!a || !playing || !bk_audio_playing(a->audio, a->speech, playing))
    return fail(error, "cannot read speech status");
  return 1;
}
int bk_selection_audio_voice_level(BkSelectionAudio *a, BkVoiceEnvelope *shared,
                                   float seconds, float *level,
                                   char error[256]) {
  return a ? bk_scene_voice_envelope(a->audio, a->speech, shared, seconds,
                                     level, error)
           : fail(error, "missing owner");
}
