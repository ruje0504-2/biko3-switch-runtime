#include "scene/dialogue_audio.h"
#include <stdlib.h>
struct BkDialogueAudio {
  BkResourceStore *store;
  BkAudio *audio;
  BkAudioClip *music_clip, *speech_clip;
  unsigned music, speech;
  int32_t speech_volume;
  int speech_submitted;
};
static int fail(char e[256], const char *why) {
  snprintf(e, 256, "dialogue audio: %s", why);
  return 0;
}
static int release_speech(BkDialogueAudio *a, char e[256]) {
  if (!bk_audio_clear(a->audio, a->speech, e))
    return 0;
  bk_audio_clip_release(a->speech_clip);
  a->speech_clip = NULL;
  a->speech_submitted = 0;
  return 1;
}
static int command(void *context, const BkDialogueMediaCommand *c,
                   char e[256]) {
  BkDialogueAudio *a = context;
  switch (c->kind) {
  case BK_DIALOGUE_SPEECH_RELEASE:
    return release_speech(a, e);
  case BK_DIALOGUE_SPEECH_LOAD: {
    BkAudioClip *next = bk_audio_clip_load(a->store, c->pack, c->name, e);
    if (!next)
      return 0;
    bk_audio_clip_release(a->speech_clip);
    a->speech_clip = next;
    a->speech_volume = c->volume;
    a->speech_submitted = 0;
    /* Loaded native buffer is stopped. Never play then pause while the
     * independent pump can run between those two commands. */
    return 1;
  }
  case BK_DIALOGUE_SPEECH_PLAY:
    if (!a->speech_clip)
      return fail(e, "speech play without a loaded buffer");
    if (!bk_audio_play(a->audio, a->speech, a->speech_clip, 0, a->speech_volume,
                       0, e))
      return 0;
    a->speech_submitted = 1;
    return 1;
  case BK_DIALOGUE_MUSIC_GAIN:
    return bk_audio_gain(a->audio, a->music, c->volume, 0, e);
  case BK_DIALOGUE_MUSIC_PAUSE:
    return bk_audio_pause(a->audio, a->music, e);
  case BK_DIALOGUE_MUSIC_LOAD: {
    BkAudioClip *next = bk_audio_clip_load_music(a->store, c->pack, c->name, e);
    if (!next)
      return 0;
    if (!bk_audio_play(a->audio, a->music, next, c->loop, c->volume, 0, e)) {
      bk_audio_clip_release(next);
      return 0;
    }
    bk_audio_clip_release(a->music_clip);
    a->music_clip = next;
    return 1;
  }
  }
  return fail(e, "unknown media command");
}
BkDialogueAudio *bk_dialogue_audio_create(BkResourceStore *store,
                                          BkAudio *audio, unsigned music,
                                          unsigned speech, char e[256]) {
  if (!store || !audio || music >= BK_AUDIO_VOICES ||
      speech >= BK_AUDIO_VOICES || music == speech ||
      bk_audio_stats(audio).failed) {
    fail(e, "invalid services/voice slots");
    return NULL;
  }
  BkDialogueAudio *a = calloc(1, sizeof(*a));
  if (!a) {
    fail(e, "allocation failed");
    return NULL;
  }
  a->store = store;
  a->audio = audio;
  a->music = music;
  a->speech = speech;
  if (!bk_audio_clear(audio, music, e) || !bk_audio_clear(audio, speech, e)) {
    free(a);
    return NULL;
  }
  return a;
}
void bk_dialogue_audio_destroy(BkDialogueAudio *a) {
  if (!a)
    return;
  bk_audio_clip_release(a->music_clip);
  bk_audio_clip_release(a->speech_clip);
  free(a);
}
int bk_dialogue_audio_stop(BkDialogueAudio *a, char e[256]) {
  if (!a)
    return fail(e, "missing owner");
  if (!release_speech(a, e) || !bk_audio_clear(a->audio, a->music, e))
    return 0;
  bk_audio_clip_release(a->music_clip);
  a->music_clip = NULL;
  return 1;
}
int bk_dialogue_audio_music_open(BkDialogueAudio *a, BkDialogueMusic *s,
                                 BkDialogue *d, char e[256]) {
  if (!a || !d)
    return fail(e, "missing owner/dialogue");
  return bk_dialogue_music_open(s, &d->music_pending, d->music,
                                &(BkDialogueMediaOps){a, command}, e);
}
int bk_dialogue_audio_music_step(BkDialogueAudio *a, BkDialogueMusic *s,
                                 BkDialogue *d, float dt, int32_t master,
                                 char e[256]) {
  if (!a || !d)
    return fail(e, "missing owner/dialogue");
  return bk_dialogue_music_step(s, a->music_clip != NULL, &d->music_pending,
                                d->music, dt, master,
                                &(BkDialogueMediaOps){a, command}, e);
}
int bk_dialogue_audio_speech_step(BkDialogueAudio *a, BkDialogue *d,
                                  int32_t master, char e[256]) {
  if (!a || !d)
    return fail(e, "missing owner/dialogue");
  return bk_dialogue_speech_step(&d->sound_pending, d->sound, master,
                                 &(BkDialogueMediaOps){a, command}, e);
}
int bk_dialogue_audio_speech_pause(BkDialogueAudio *a, char e[256]) {
  if (!a)
    return fail(e, "missing owner");
  return a->speech_submitted ? bk_audio_pause(a->audio, a->speech, e) : 1;
}
int bk_dialogue_audio_voice_level(BkDialogueAudio *a, BkVoiceEnvelope *shared,
                                  float dt, float *level, char e[256]) {
  return a ? bk_scene_voice_envelope(a->audio, a->speech, shared, dt, level, e)
           : fail(e, "missing owner");
}

int bk_dialogue_audio_speech_present(const BkDialogueAudio *a) {
  return a && a->speech_clip;
}
