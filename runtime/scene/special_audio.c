#include "scene/special_audio.h"
#include "world/spatial_audio.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
struct BkSpecialAudio {
  BkResourceStore *store;
  BkAudio *audio;
  BkAudioClip *clips[5];
  unsigned first;
  uint8_t *loops;
  int submitted[4], stopped;
  int32_t volume[5], pan[4];
};
static int fail(char e[256], const char *why) {
  snprintf(e, 256, "special audio: %s", why); return 0;
}
static int gain_valid(int32_t v) { return v >= -10000 && v <= 0; }
const char *bk_special_audio_music(unsigned group) {
  static const char *const names[] = {"bg036.wav", "bg041.wav", "bg037.wav", "bg040.wav", "bg038.wav"};
  return group < 5 ? names[group] : NULL;
}
const char *bk_special_audio_initial_effect(unsigned group) {
  static const char *const names[] = {"se120.wav", "se122.wav", NULL, "se143.wav", "se121.wav"};
  return group < 5 ? names[group] : NULL;
}
void bk_special_audio_destroy(BkSpecialAudio *a) {
  if (!a) return;
  for (unsigned i = 0; i < 5; ++i) bk_audio_clip_release(a->clips[i]);
  free(a);
}
int bk_special_audio_stop(BkSpecialAudio *a, char e[256]) {
  if (!a) return fail(e, "missing owner");
  if (a->stopped) return 1;
  for (unsigned i = 0; i < 5; ++i) {
    if (!bk_audio_clear(a->audio, a->first + i, e)) return 0;
    bk_audio_clip_release(a->clips[i]); a->clips[i] = NULL;
  }
  a->stopped = 1; return 1;
}
BkSpecialAudio *bk_special_audio_create(BkResourceStore *store, BkAudio *audio,
    unsigned first, unsigned group, int32_t volume, uint8_t loops[4], char e[256]) {
  if (!store || !audio || !loops || group >= 5 || first > BK_AUDIO_VOICES - 5 ||
      !gain_valid(volume) || bk_audio_stats(audio).failed) {
    fail(e, "invalid services/profile/slots/volume"); return NULL;
  }
  BkSpecialAudio *a = calloc(1, sizeof(*a));
  if (!a) { fail(e, "allocation failed"); return NULL; }
  a->store = store; a->audio = audio; a->first = first; a->loops = loops;
  a->volume[4] = -6000; a->volume[0] = volume;
  a->clips[4] = bk_audio_clip_load(store, "bk3_02", bk_special_audio_music(group), e);
  if (!a->clips[4]) goto bad;
  const char *effect = bk_special_audio_initial_effect(group);
  if (effect) {
    a->clips[0] = bk_audio_clip_load(store, "bk3_02", effect, e);
    if (!a->clips[0]) goto bad;
  }
  for (unsigned i = 0; i < 5; ++i)
    if (!bk_audio_clear(audio, first + i, e)) goto playback_failed;
  if (!bk_audio_play(audio, first + 4, a->clips[4], 1, -6000, 0, e)) goto playback_failed;
  if (effect && group != 1) {
    if (!bk_audio_play(audio, first, a->clips[0], 1, volume, 0, e)) goto playback_failed;
    a->submitted[0] = 1;
  }
  if (effect) loops[0] = group != 1;
  return a;
playback_failed: {
    char ignored[256]; bk_special_audio_stop(a, ignored);
  }
bad:
  bk_special_audio_destroy(a); return NULL;
}
int bk_special_audio_present(const BkSpecialAudio *a, unsigned slot, int *out) {
  if (!a || slot >= 4 || !out) return 0;
  *out = !a->stopped && a->clips[slot] != NULL; return 1;
}
int bk_special_audio_music_volume(const BkSpecialAudio *a, int32_t *out) {
  if (!a || a->stopped || !out) return 0;
  *out = a->volume[4]; return 1;
}
static const char *effect_name(const BkSpecialEventAudioCall *c) {
  if (!c->name) return NULL;
  const char *name = c->name;
  if (c->pack) {
    const char *pack = c->pack;
    if (*pack == '\\') ++pack;
    if (strcmp(pack, "bk3_02") && strcmp(pack, "bk3_02.pp")) return NULL;
    if (*name == '\\') ++name;
  } else {
    if (strncmp(name, "\\wav\\", 5)) return NULL;
    name += 5;
  }
  return *name && !strchr(name, '\\') && !strchr(name, '/') ? name : NULL;
}
int bk_special_audio_call(BkSpecialAudio *a, const BkSpecialEventAudioCall *c,
    int32_t music, int32_t voice, int32_t effect, char e[256]) {
  if (!a || a->stopped || !c || !gain_valid(music) || !gain_valid(voice) || !gain_valid(effect))
    return fail(e, "invalid owner/call/master volume");
  if (c->operation == BK_SPECIAL_MUSIC_FADE) {
    if (!isfinite(c->amount) || (double)c->amount < INT32_MIN || (double)c->amount > INT32_MAX - 10000)
      return fail(e, "invalid fade step");
    int32_t delta = (int32_t)c->amount;
    if (delta <= 1) delta = 1;
    int32_t value = a->volume[4];
    if ((uint8_t)c->flags == 1) { value += delta; if (value >= music) value = music; }
    else if ((uint8_t)c->flags == 0) { value -= delta; if (value <= -6000) value = -6000; }
    a->volume[4] = value; /* native state write precedes SetVolume */
    return bk_audio_gain(a->audio, a->first + 4, value, 0, e);
  }
  unsigned slot = c->slot;
  if (slot >= 4) return fail(e, "invalid effect slot");
  if (c->operation == BK_SPECIAL_EFFECT_LOAD) {
    const char *name = effect_name(c);
    if (!name || !gain_valid(c->volume) || c->flags > 2) return fail(e, "invalid effect resource");
    if (!bk_audio_clear(a->audio, a->first + slot, e)) return 0;
    bk_audio_clip_release(a->clips[slot]); a->clips[slot] = NULL; a->submitted[slot] = 0;
    a->clips[slot] = bk_audio_clip_load(a->store, "bk3_02", name, e);
    if (!a->clips[slot]) return 0;
    a->volume[slot] = c->volume; a->pan[slot] = 0; a->loops[slot] = c->flags != 0;
    if (c->flags != 1) return 1;
    if (!bk_audio_play(a->audio, a->first + slot, a->clips[slot], 1, c->volume, 0, e)) return 0;
    a->submitted[slot] = 1; return 1;
  }
  if (!a->clips[slot]) return fail(e, "effect buffer is not loaded");
  if (c->operation == BK_SPECIAL_EFFECT_SPATIAL) {
    unsigned category = (uint8_t)c->flags;
    BkSpatialAudio gain;
    if (category > 2 || !bk_spatial_audio(&gain, c->source, c->listener,
          c->listener[3], category == 0 ? music : category == 1 ? voice : effect, c->amount))
      return fail(e, "invalid spatial parameters");
    a->volume[slot] = gain.volume; a->pan[slot] = gain.pan;
    return !a->submitted[slot] || bk_audio_gain(a->audio, a->first + slot, gain.volume, gain.pan, e);
  }
  if (c->operation != BK_SPECIAL_EFFECT_PLAY && c->operation != BK_SPECIAL_EFFECT_DIRECT_PLAY)
    return fail(e, "unknown operation");
  int ok = c->operation == BK_SPECIAL_EFFECT_DIRECT_PLAY && a->submitted[slot]
      ? bk_audio_resume(a->audio, a->first + slot, c->flags & 1, e)
      : bk_audio_play(a->audio, a->first + slot, a->clips[slot], c->flags & 1,
                      a->volume[slot], a->pan[slot], e);
  if (ok) a->submitted[slot] = 1;
  return ok;
}
int bk_special_audio_level(BkSpecialAudio *a, unsigned slot, BkVoiceEnvelope *shared,
                            float seconds, float *out, char e[256]) {
  if (!a || a->stopped || slot >= 4 || !shared || !out || !isfinite(seconds) || seconds < 0)
    return fail(e, "invalid envelope query");
  if (!a->clips[slot]) { *out = 0; return 1; }
  return bk_scene_voice_envelope(a->audio, a->first + slot, shared, seconds, out, e);
}
