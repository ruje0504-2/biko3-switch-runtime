#include "scene/background_audio.h"
#include <stdlib.h>
#include <string.h>
struct BkBackgroundAudio {
  BkAudio *audio;
  const BkBackgroundConfig *config;
  BkAudioClip *clips[9];
  unsigned first;
  int32_t volume[9], pan[9];
  int submitted[9];
};
static int fail(char *error, const char *message) {
  snprintf(error, 256, "background audio: %s", message);
  return 0;
}
void bk_background_audio_destroy(BkBackgroundAudio *a) {
  if (!a)
    return;
  for (unsigned i = 0; i < 9; ++i)
    bk_audio_clip_release(a->clips[i]);
  free(a);
}
int bk_background_audio_stop(BkBackgroundAudio *a, char error[256]) {
  if (!a)
    return fail(error, "missing instance");
  for (unsigned i = 0; i < 9; ++i) {
    if (!bk_audio_clear(a->audio, a->first + i, error))
      return 0;
    a->submitted[i] = 0;
  }
  return 1;
}
BkBackgroundAudio *bk_background_audio_create(BkResourceStore *store,
                                              BkAudio *audio, unsigned first,
                                              const BkBackgroundConfig *config,
                                              BkBackgroundState *state,
                                              int32_t effect_volume,
                                              char error[256]) {
  if (!store || !audio || !config || !state || first > BK_AUDIO_VOICES - 9 ||
      effect_volume < -10000 || effect_volume > 0 ||
      bk_audio_stats(audio).failed) {
    fail(error, "invalid resources/mixer/config/voice range");
    return NULL;
  }
  BkBackgroundAudio *a = calloc(1, sizeof(*a));
  if (!a) {
    fail(error, "allocation failed");
    return NULL;
  }
  a->audio = audio;
  a->config = config;
  a->first = first;
  for (unsigned i = 0; i < 9; ++i) {
    const char *name = i ? config->ambient[i - 1].file : config->music;
    if (!name)
      continue;
    a->clips[i] = bk_audio_clip_load(store, "bk3_02", name, error);
    if (!a->clips[i]) {
      bk_background_audio_destroy(a);
      return NULL;
    }
    a->volume[i] =
        i && config->ambient[i - 1].uses_effect_volume ? effect_volume : -6000;
  }
  for (unsigned i = 0; i < 9; ++i) {
    if (!bk_audio_clear(audio, first + i, error))
      goto bad;
    if (a->clips[i] && (!i || config->ambient[i - 1].loop_mode == 1)) {
      if (!bk_audio_play(audio, first + i, a->clips[i], 1, a->volume[i], 0,
                         error))
        goto bad;
      a->submitted[i] = 1;
    }
  }
  state->music_volume = -6000;
  return a;
bad:
  {
    char ignored[256];
    bk_background_audio_stop(a, ignored);
  }
  bk_background_audio_destroy(a);
  return NULL;
}
static int consume(void *context, const BkBackgroundCommands *commands,
                   char error[256]) {
  BkBackgroundAudio *a = context;
  if (commands->music_update &&
      !bk_audio_gain(a->audio, a->first, commands->music_volume, 0, error))
    return 0;
  for (unsigned i = 0; i < 8; ++i) {
    unsigned v = i + 1;
    const BkAmbientCommand *c = &commands->ambient[i];
    if (!a->clips[v])
      continue;
    if (c->spatial) {
      a->volume[v] = c->gain.volume;
      a->pan[v] = c->gain.pan;
    }
    if (c->action == 2) {
      if (!bk_audio_clear(a->audio, a->first + v, error))
        return 0;
      a->submitted[v] = 0;
    } else if (c->action == 1) {
      /*46435e always rewinds to0. Its explicit loop flag can differ from
       * the context flag (101 door edges request a one-shot). */
      if (!bk_audio_play(a->audio, a->first + v, a->clips[v], c->loop,
                         a->volume[v], a->pan[v], error))
        return 0;
      a->submitted[v] = 1;
    } else if (c->spatial && a->submitted[v] &&
               !bk_audio_gain(a->audio, a->first + v, a->volume[v], a->pan[v],
                              error))
      return 0;
  }
  return 1;
}
int bk_background_audio_step(BkBackgroundAudio *a, BkBackgroundAssets *assets,
                             BkBackgroundState *state, uint32_t *random,
                             const BkBackgroundInput *input,
                             BkBackgroundCommands *out, char error[256]) {
  if (!a || !assets || !input ||
      a->config != bk_background_assets_config(assets))
    return fail(error, "missing/mismatched instances");
  BkBackgroundInput in = *input;
  for (unsigned i = 0; i < 8; ++i) {
    const BkAmbientConfig *c = &a->config->ambient[i];
    in.ambient[i].present = a->clips[i + 1] != NULL;
    if (!bk_audio_playing(a->audio, a->first + i + 1, &in.ambient[i].playing))
      return fail(error, "unavailable playback status");
    in.ambient[i].loop = c->loop_mode != 0;
    in.ambient[i].trigger = c->trigger;
    memcpy(in.ambient[i].position, c->position, 12);
  }
  return bk_background_assets_step(assets, state, random, &in, out, consume, a,
                                   error);
}

int bk_background_audio_pause_step(BkBackgroundAudio *a,
                                   BkBackgroundState *state,
                                   const BkBackgroundInput *input,
                                   char error[256]) {
  if (!a || !state || !input)
    return fail(error, "missing pause inputs");
  BkBackgroundInput in = *input;
  for (unsigned i = 0; i < 8; ++i) {
    const BkAmbientConfig *c = &a->config->ambient[i];
    in.ambient[i].present = a->clips[i + 1] != NULL;
    in.ambient[i].trigger = c->trigger;
    memcpy(in.ambient[i].position, c->position, 12);
  }
  BkBackgroundState next = *state;
  BkBackgroundCommands commands;
  if (!bk_background_pause_step(&next, &in, &commands))
    return fail(error, "invalid pause policy");
  if (!consume(a, &commands, error))
    return 0;
  *state = next;
  return 1;
}
