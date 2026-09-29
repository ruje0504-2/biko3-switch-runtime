#include "scene/ending_audio.h"
#include <stdlib.h>
#include <string.h>
struct BkEndingAudio {
  BkResourceStore *store;
  BkAudio *audio;
  BkAudioClip *clips[BK_ENDING_SOUND_BUFFERS];
  unsigned first, count;
};
static int fail(char e[256], const char *why) {
  snprintf(e, 256, "ending audio: %s", why);
  return 0;
}
static BkEndingAudio *create(BkResourceStore *store, BkAudio *audio,
                             unsigned first, unsigned count, char e[256]) {
  if (!store || !audio || first > BK_AUDIO_VOICES - count ||
      bk_audio_stats(audio).failed) {
    fail(e, "invalid store/mixer/slots");
    return NULL;
  }
  BkEndingAudio *a = calloc(1, sizeof(*a));
  if (!a) {
    fail(e, "allocation failed");
    return NULL;
  }
  a->store = store;
  a->audio = audio;
  a->first = first;
  a->count = count;
  for (unsigned i = 0; i < count; i++)
    if (!bk_audio_clear(audio, first + i, e)) {
      free(a);
      return NULL;
    }
  return a;
}
BkEndingAudio *bk_ending_audio_create(BkResourceStore *store, BkAudio *audio,
                                      unsigned first, char e[256]) {
  return create(store, audio, first, 6, e);
}
BkEndingAudio *bk_ending_audio_create_entry(BkResourceStore *store,
                                            BkAudio *audio, unsigned first,
                                            unsigned group, unsigned variant,
                                            int32_t music_volume, char e[256]) {
  const char *music = bk_ending_sound_music(group, variant);
  if (!music || music_volume < -10000 || music_volume > 0) {
    fail(e, "invalid entry profile/volume");
    return NULL;
  }
  BkEndingAudio *a = create(store, audio, first, BK_ENDING_SOUND_BUFFERS, e);
  if (!a)
    return NULL;
  a->clips[BK_ENDING_SOUND_MUSIC] =
      bk_audio_clip_load(store, "bk3_02", music, e);
  if (!a->clips[BK_ENDING_SOUND_MUSIC])
    goto failed;
  for (unsigned i = 0; i < BK_ENDING_EFFECTS; i++) {
    const char *name = bk_ending_sound_effect(i);
    if (!name)
      continue;
    a->clips[2 + i] = bk_audio_clip_load(store, "bk3_02", name, e);
    if (!a->clips[2 + i])
      goto failed;
  }
  if (!bk_audio_play(audio, first + BK_ENDING_SOUND_MUSIC,
                     a->clips[BK_ENDING_SOUND_MUSIC], 1, music_volume, 0, e))
    goto failed;
  return a;
failed:
  bk_ending_audio_destroy(a);
  return NULL;
}
int bk_ending_audio_present(const BkEndingAudio *a, unsigned slot,
                            int *present) {
  if (!a || slot >= a->count || !present)
    return 0;
  *present = a->clips[slot] != NULL;
  return 1;
}
int bk_ending_audio_stop(BkEndingAudio *a, char e[256]) {
  if (!a)
    return fail(e, "missing owner");
  for (unsigned i = 0; i < a->count; i++) {
    if (!bk_audio_clear(a->audio, a->first + i, e))
      return 0;
    bk_audio_clip_release(a->clips[i]);
    a->clips[i] = NULL;
  }
  return 1;
}
void bk_ending_audio_destroy(BkEndingAudio *a) {
  if (!a)
    return;
  for (unsigned i = 0; i < a->count; i++)
    bk_audio_clip_release(a->clips[i]);
  free(a);
}
int bk_ending_audio_release_speech(BkEndingAudio *a, char e[256]) {
  if (!a || a->count < 2)
    return fail(e, "missing speech owner");
  for (unsigned i = 0; i < 2; ++i) {
    if (!a->clips[i])
      continue;
    if (!bk_audio_clear(a->audio, a->first + i, e))
      return 0;
    bk_audio_clip_release(a->clips[i]);
    a->clips[i] = NULL;
  }
  return 1;
}
int bk_ending_audio_bind(BkEndingAudio *a, unsigned slot, const char *pack,
                         const char *name, char e[256]) {
  if (!a || slot >= a->count || !pack || !name)
    return fail(e, "invalid buffer binding");
  if (!bk_audio_clear(a->audio, a->first + slot, e))
    return 0;
  bk_audio_clip_release(a->clips[slot]);
  a->clips[slot] = NULL;
  a->clips[slot] = bk_audio_clip_load(a->store, pack, name, e);
  return a->clips[slot] != NULL;
}
static int bind_speech(BkEndingAudio *a, unsigned slot, const char *name,
                       char e[256]) {
  if (!bk_audio_clear(a->audio, a->first + slot, e))
    return 0;
  bk_audio_clip_release(a->clips[slot]);
  a->clips[slot] = NULL;
  BkBlob blob = {0};
  BkResourceResult result =
      bk_resources_read(a->store, "bk3_06", name, &blob, e);
  if (result == BK_RESOURCE_MISSING && bk_ending_sound_absent_speech(name)) {
    if (e)
      *e = 0;
    return 1; /* observed null buffer;4ad2bf(null) performs no playback */
  }
  if (result != BK_RESOURCE_OK)
    return 0;
  a->clips[slot] = bk_audio_clip_decode(blob.data, blob.size, e);
  bk_blob_free(&blob);
  if (!a->clips[slot])
    return 0;
  return 1;
}
int bk_ending_audio_load_speech(BkEndingAudio *a, unsigned slot,
                                const char *name, char e[256]) {
  if (!a || slot > 1 || slot >= a->count || !name)
    return fail(e, "invalid speech load");
  return bind_speech(a, slot, name, e);
}
int bk_ending_audio_speech(BkEndingAudio *a, unsigned slot, const char *name,
                           int32_t volume, char e[256]) {
  if (slot > 1 || !a || !name || volume < -10000 || volume > 0)
    return fail(e, "invalid speech");
  if (!bind_speech(a, slot, name, e))
    return 0;
  return !a->clips[slot] || bk_audio_play(a->audio, a->first + slot,
                                          a->clips[slot], 0, volume, 0, e);
}
int bk_ending_audio_auxiliary_voice(BkEndingAudio *a, unsigned group,
                                    int32_t cue, unsigned slot,
                                    int32_t volume, char e[256]) {
  char name[32];
  if (!a || group >= 5 || cue < 0 || cue > 99 || slot > 1)
    return fail(e, "invalid auxiliary voice identity");
  if (snprintf(name, sizeof(name), "PH%u33%02d.wav", group + 1, cue) < 0)
    return fail(e, "auxiliary voice name formatting failed");
  return bk_ending_audio_speech(a, slot, name, volume, e);
}
int bk_ending_audio_call(BkEndingAudio *a, unsigned group, int32_t variant,
                         int32_t selection, const BkEndingAudioCall *c,
                         int *playing, char e[256]) {
  if (!a || !c || !playing || c->slot >= a->count)
    return fail(e, "invalid command");
  unsigned slot = c->slot;
  if (c->operation == BK_ENDING_AUDIO_VOICE ||
      c->operation == BK_ENDING_AUDIO_CUE) {
    char pack[16], name[32];
    if (!bk_ending_audio_resource(group, variant, selection, c, pack, name, e))
      return 0;
    if (c->operation == BK_ENDING_AUDIO_VOICE && group == 0 && variant &&
        c->cue == 30)
      slot = 1;
    if (!strcmp(pack, "bk3_06") && slot < 2) {
      if (!bind_speech(a, slot, name, e))
        return 0;
    } else if (!bk_ending_audio_bind(a, slot, pack, name, e))
      return 0;
  }
  *playing = 0;
  switch (c->operation) {
  case BK_ENDING_AUDIO_STATUS:
    return !a->clips[slot] ||
           bk_audio_playing(a->audio, a->first + slot, playing);
  case BK_ENDING_AUDIO_PAUSE:
    /* bind has no queued epoch; Stop on an already stopped buffer is a no-op.
     */
    if (!a->clips[slot] ||
        !bk_audio_playing(a->audio, a->first + slot, playing))
      return !a->clips[slot];
    return !*playing || bk_audio_pause(a->audio, a->first + slot, e);
  case BK_ENDING_AUDIO_RESTART:
  case BK_ENDING_AUDIO_VOICE:
  case BK_ENDING_AUDIO_CUE:
    return !a->clips[slot] ||
           bk_audio_play(a->audio, a->first + slot, a->clips[slot],
                         (c->flags & 1) != 0, c->volume, 0, e);
  }
  return fail(e, "unknown command");
}
int bk_ending_audio_level(BkEndingAudio *a, unsigned slot,
                          BkEndingVoiceEnvelope *v, float *level, char e[256]) {
  if (!a || slot >= a->count)
    return fail(e, "invalid envelope slot");
  int playing = 0;
  if (!bk_audio_playing(a->audio, a->first + slot, &playing))
    return fail(e, "cannot read speech status");
  if (!playing)
    return bk_ending_voice_envelope_step(v, 0, 0, 0, level, e);
  BkAudioCursor cursor;
  if (!bk_audio_cursor(a->audio, a->first + slot, &cursor))
    return fail(e, "cannot read consumed speech cursor");
  int sampled = 0;
  int32_t magnitude = 0;
  if (cursor.playing && cursor.buffered && cursor.pcm) {
    size_t channels = bk_pcm_channels(cursor.pcm);
    size_t frames = bk_pcm_frames(cursor.pcm);
    if (channels && frames <= SIZE_MAX / channels &&
        cursor.source_frame < frames) {
      if (!bk_ending_voice_pcm_level(bk_pcm_samples(cursor.pcm),
                                      frames * channels,
                                      cursor.source_frame * channels,
                                      &sampled, &magnitude, e))
        return 0;
    }
  }
  return bk_ending_voice_envelope_step(v, 1, sampled, magnitude, level, e);
}
static int duck_status(void *ctx, unsigned slot, int *playing, char e[256]) {
  BkEndingAudio *a = ctx;
  if (!bk_audio_playing(a->audio, a->first + slot, playing))
    return fail(e, "duck status query failed");
  return 1;
}
static int duck_volume(void *ctx, int32_t *volume, char e[256]) {
  BkEndingAudio *a = ctx;
  int32_t pan;
  if (!bk_audio_get_gain(a->audio, a->first, volume, &pan))
    return fail(e, "duck volume query failed");
  return 1;
}
static int duck_gain(void *ctx, int32_t volume, char e[256]) {
  BkEndingAudio *a = ctx;
  int32_t current, pan;
  if (!bk_audio_get_gain(a->audio, a->first, &current, &pan))
    return fail(e, "duck gain query failed");
  /*4e01e4 ignores HRESULT. Model rejected DirectSound range writes without
   * changing the mixer, rather than clamping a request that native rejects. */
  return volume < -10000 || volume > 0 ||
         bk_audio_gain(a->audio, a->first, volume, pan, e);
}
int bk_ending_audio_duck(BkEndingAudio *a, int32_t *transition,
                         int32_t direction, int32_t voice_master, float seconds,
                         int32_t *result, char e[256]) {
  if (!a || bk_audio_stats(a->audio).failed)
    return fail(e, "duck missing owner or failed mixer");
  BkEndingDuckInput in = {direction,
                          voice_master,
                          seconds,
                          {a->clips[0] != NULL, a->clips[1] != NULL}};
  BkEndingDuckOps ops = {a, duck_status, duck_volume, duck_gain};
  return bk_ending_sound_duck(transition, &in, &ops, result, e);
}
