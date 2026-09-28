#include "scene/player_audio.h"
#include <stdlib.h>
#include <string.h>
static const char *const names[] = {"se104.wav", "se105.wav", "se106.wav",
                                    "se107.wav", "se109.wav", "se110.wav",
                                    "se119.wav", "se144.wav", "se148.wav"};
struct BkPlayerAudio {
  BkAudio *audio;
  BkAudioClip *clips[9];
  unsigned voice;
  int present;
  int32_t volume;
};
static int fail(char *error, const char *message) {
  snprintf(error, 256, "player audio: %s", message);
  return 0;
}
BkPlayerAudio *bk_player_audio_create(BkResourceStore *resources,
                                      BkAudio *audio, unsigned voice,
                                      char error[256]) {
  if (!resources || !audio || voice >= BK_AUDIO_VOICES ||
      bk_audio_stats(audio).failed) {
    fail(error, "invalid resources/mixer/voice");
    return NULL;
  }
  BkPlayerAudio *p = calloc(1, sizeof(*p));
  if (!p) {
    fail(error, "allocation failed");
    return NULL;
  }
  p->audio = audio;
  p->voice = voice;
  for (unsigned i = 0; i < 9; ++i) {
    p->clips[i] = bk_audio_clip_load(resources, "bk3_02", names[i], error);
    if (!p->clips[i]) {
      bk_player_audio_destroy(p);
      return NULL;
    }
  }
  return p;
}
void bk_player_audio_destroy(BkPlayerAudio *p) {
  if (!p)
    return;
  for (unsigned i = 0; i < 9; ++i)
    bk_audio_clip_release(p->clips[i]);
  free(p);
}
int bk_player_audio_stop(BkPlayerAudio *p, char error[256]) {
  if (!p)
    return fail(error, "missing instance");
  return bk_audio_clear(p->audio, p->voice, error);
}
int bk_player_audio_effect(BkPlayerAudio *p, BkAudioClip *clip, int32_t volume,
                           int32_t pan, char error[256]) {
  if (!p || !clip || volume < -10000 || volume > 0 || pan < -10000 ||
      pan > 10000)
    return fail(error, "invalid external effect");
  if (!bk_audio_play(p->audio, p->voice, clip, 0, volume, pan, error))
    return 0;
  p->present = 1;
  return 1;
}
static int submit(void *context, const BkPlayerEvents *events,
                  char error[256]) {
  BkPlayerAudio *p = context;
  for (unsigned i = 0; i < events->count; ++i) {
    const BkPlayerSoundCommand *c = &events->commands[i];
    if (!c->file) {
      if (!bk_audio_clear(p->audio, p->voice, error))
        return 0;
      continue;
    }
    unsigned index = 0;
    while (index < 9 && strcmp(c->file, names[index]))
      ++index;
    if (index == 9)
      return fail(error, "unknown effect file");
    if (!bk_audio_play(p->audio, p->voice, p->clips[index], c->loop, p->volume,
                       0, error))
      return 0;
    p->present = 1;
  }
  return 1;
}
int bk_player_audio_presentation(BkPlayerAudio *p, BkEntryAssets *entry,
                                 BkPlayerEventState *state, uint8_t *steps,
                                 size_t step_count, uint8_t *shared,
                                 size_t shared_count,
                                 const BkEntryPlayerPresentation *input,
                                 int32_t volume, char error[256]) {
  if (!p || !input || volume < -10000 || volume > 0)
    return fail(error, "invalid presentation/volume");
  BkEntryPlayerPresentation in = *input;
  in.voice_present = p->present;
  if (!bk_audio_playing(p->audio, p->voice, &in.voice_playing))
    return fail(error, "unavailable playback status");
  p->volume = volume;
  return bk_entry_assets_step_player_presentation(
      entry, state, steps, step_count, shared, shared_count, &in, submit, p,
      error);
}

int bk_player_audio_release_buffer(BkPlayerAudio *p, char error[256]) {
  if (!bk_player_audio_stop(p, error))
    return 0;
  p->present = 0;
  return 1;
}
