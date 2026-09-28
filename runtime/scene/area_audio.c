#include "scene/area_audio.h"
#include <stdlib.h>
#include <string.h>
static const char *const names[] = {"se304.wav", "se155.wav", "se156.wav",
                                    "se151.wav", "se152.wav"};
struct BkAreaAudio {
  BkAudio *audio;
  BkPlayerAudio *player;
  unsigned npc_voice;
  BkAudioClip *clips[5];
};
static int fail(char *error, const char *why) {
  snprintf(error, 256, "area audio: %s", why);
  return 0;
}
void bk_area_audio_destroy(BkAreaAudio *a) {
  if (!a)
    return;
  for (unsigned i = 0; i < 5; ++i)
    bk_audio_clip_release(a->clips[i]);
  free(a);
}
BkAreaAudio *bk_area_audio_create(BkResourceStore *store, BkAudio *audio,
                                  unsigned voice, BkPlayerAudio *player,
                                  char error[256]) {
  if (!store || !audio || !player || voice >= BK_AUDIO_VOICES ||
      bk_audio_stats(audio).failed) {
    fail(error, "invalid services/voice");
    return NULL;
  }
  BkAreaAudio *a = calloc(1, sizeof(*a));
  if (!a) {
    fail(error, "allocation failed");
    return NULL;
  }
  a->audio = audio;
  a->player = player;
  a->npc_voice = voice;
  for (unsigned i = 0; i < 5; ++i)
    if (!(a->clips[i] = bk_audio_clip_load(store, "bk3_02", names[i], error))) {
      bk_area_audio_destroy(a);
      return NULL;
    }
  return a;
}
int bk_area_audio_stop(BkAreaAudio *a, char error[256]) {
  if (!a)
    return fail(error, "missing instance");
  return bk_audio_clear(a->audio, a->npc_voice, error);
}
int bk_area_audio_apply(BkAreaAudio *a, const BkAreaBoundaryCommands *commands,
                        char error[256]) {
  if (!a || !commands || commands->count > 2)
    return fail(error, "invalid commands");
  unsigned clips[2];
  for (unsigned i = 0; i < commands->count; ++i) {
    const BkAreaSoundCommand *c = &commands->commands[i];
    if ((unsigned)c->recipient > BK_AREA_SOUND_PLAYER || !c->file ||
        c->gain.volume < -10000 || c->gain.volume > 0 || c->gain.pan < -10000 ||
        c->gain.pan > 10000 ||
        (i && c->recipient <= commands->commands[i - 1].recipient))
      return fail(error, "invalid sound/order/gain");
    clips[i] = 0;
    while (clips[i] < 3 && strcmp(c->file, names[clips[i]]))
      ++clips[i];
    if (clips[i] == 3)
      return fail(error, "unknown boundary cue");
  }
  for (unsigned i = 0; i < commands->count; ++i) {
    const BkAreaSoundCommand *c = &commands->commands[i];
    int ok = c->recipient == BK_AREA_SOUND_NPC
                 ? bk_audio_play(a->audio, a->npc_voice, a->clips[clips[i]], 0,
                                 c->gain.volume, c->gain.pan, error)
                 : bk_player_audio_effect(a->player, a->clips[clips[i]],
                                          c->gain.volume, c->gain.pan, error);
    if (!ok)
      return 0;
  }
  return 1;
}

int bk_area_audio_route(BkAreaAudio *a, const BkNpcRouteSound *cue,
                        char error[256]) {
  if (!a || !cue)
    return fail(error, "missing route cue");
  if (!cue->file)
    return 1;
  unsigned clip = 0;
  while (clip < 5 && strcmp(cue->file, names[clip]))
    ++clip;
  if (clip == 5 || cue->gain.volume < -10000 || cue->gain.volume > 0 ||
      cue->gain.pan < -10000 || cue->gain.pan > 10000)
    return fail(error, "invalid route cue/gain");
  return bk_audio_play(a->audio, a->npc_voice, a->clips[clip], 0,
                       cue->gain.volume, cue->gain.pan, error);
}
