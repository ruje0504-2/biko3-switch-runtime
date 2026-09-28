#include "scene/system_audio.h"
#include <stdlib.h>
struct BkSystemAudio {
  BkAudio *audio;
  BkAudioClip *clip;
  unsigned voice;
  int32_t volume;
};
BkSystemAudio *bk_system_audio_create(BkResourceStore *store, BkAudio *audio,
                                      unsigned voice, int32_t volume,
                                      char error[256]) {
  return bk_system_audio_create_slot(store, audio, voice, 2, volume, error);
}
BkSystemAudio *bk_system_audio_create_slot(BkResourceStore *store,
                                           BkAudio *audio, unsigned voice,
                                           unsigned slot, int32_t volume,
                                           char error[256]) {
  static const char *const names[] = {"se000.wav", "se001.wav", "se002.wav",
                                      "se003.wav", "se004.wav", "se005.wav",
                                      "se006.wav", "se099.wav"};
  if (!store || !audio || voice >= BK_AUDIO_VOICES || volume < -10000 ||
      volume > 0 || slot >= 8 || bk_audio_stats(audio).failed) {
    snprintf(error, 256, "system audio: invalid services/voice/volume");
    return NULL;
  }
  BkSystemAudio *a = calloc(1, sizeof(*a));
  if (!a) {
    snprintf(error, 256, "system audio: allocation failed");
    return NULL;
  }
  a->audio = audio;
  a->voice = voice;
  a->volume = volume;
  a->clip = bk_audio_clip_load(store, "bk3_02", names[slot], error);
  if (!a->clip) {
    free(a);
    return NULL;
  }
  return a;
}
void bk_system_audio_destroy(BkSystemAudio *a) {
  if (a) {
    bk_audio_clip_release(a->clip);
    free(a);
  }
}
int bk_system_audio_wait(BkSystemAudio *a, char error[256]) {
  return bk_system_audio_restart(a, error);
}
int bk_system_audio_restart(BkSystemAudio *a, char error[256]) {
  if (!a) {
    snprintf(error, 256, "system audio: missing instance");
    return 0;
  }
  return bk_audio_play(a->audio, a->voice, a->clip, 0, a->volume, 0, error);
}
int bk_system_audio_stop(BkSystemAudio *a, char error[256]) {
  if (!a) {
    snprintf(error, 256, "system audio: missing instance");
    return 0;
  }
  return bk_audio_clear(a->audio, a->voice, error);
}
