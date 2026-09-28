#include "scene/prop_audio.h"
#include <stdlib.h>
#include <string.h>
static const char *const names[] = {
    "se123.wav", "se124.wav", "se125.wav", "se126.wav", "se127.wav",
    "se128.wav", "se131.wav", "se145.wav", "se149.wav", "se150.wav"};
typedef struct {
  BkPropSoundState state;
  int32_t kind, volume, pan;
  BkAudioClip *clip;
  uint32_t frequency;
  int loop, submitted;
} Instance;
struct BkPropAudio {
  BkAudio *audio;
  BkAudioClip *clips[10];
  unsigned first, count;
  Instance instances[16];
};
static int fail(char *error, const char *msg) {
  snprintf(error, 256, "prop audio: %s", msg);
  return 0;
}
void bk_prop_audio_destroy(BkPropAudio *a) {
  if (!a)
    return;
  for (unsigned i = 0; i < 10; ++i)
    bk_audio_clip_release(a->clips[i]);
  free(a);
}
int bk_prop_audio_stop(BkPropAudio *a, char error[256]) {
  if (!a)
    return fail(error, "missing instance");
  for (unsigned i = 0; i < a->count; ++i) {
    if (!bk_audio_clear(a->audio, a->first + i, error))
      return 0;
    a->instances[i].submitted = 0;
  }
  return 1;
}
static int apply(BkPropAudio *a, unsigned index, const BkPropSoundCommand *cmd,
                 char error[256]) {
  Instance *p = &a->instances[index];
  unsigned voice = a->first + index;
  if (cmd->release) {
    if (!bk_audio_clear(a->audio, voice, error))
      return 0;
    p->clip = NULL;
    p->submitted = 0;
  }
  if (cmd->file) {
    unsigned i = 0;
    while (i < 10 && strcmp(names[i], cmd->file))
      ++i;
    if (i == 10)
      return fail(error, "unknown sound command");
    p->clip = a->clips[i];
    p->volume = cmd->initial_volume;
    p->pan = 0;
    p->frequency = bk_audio_clip_rate(p->clip);
    p->loop = cmd->loop;
  }
  if (cmd->spatial) {
    if (!p->clip)
      return fail(error, "spatial update requires loaded clip");
    p->volume = cmd->gain.volume;
    p->pan = cmd->gain.pan;
    if (cmd->frequency) {
      int32_t hz = (int32_t)bk_audio_clip_rate(p->clip) + 2 * p->volume;
      /* Native ignores a failed SetFrequency; retain previous frequency.
       * All real prop assets remain within this supported range. */
      if (!hz)
        p->frequency = bk_audio_clip_rate(p->clip);
      else if (hz >= 100 && hz <= 100000)
        p->frequency = (uint32_t)hz;
    }
  }
  if ((cmd->file && cmd->loop) || cmd->play) {
    if (!p->clip || !bk_audio_play(a->audio, voice, p->clip, p->loop, p->volume,
                                   p->pan, error))
      return 0;
    p->submitted = 1;
  } else if (cmd->spatial && p->submitted) {
    if (!bk_audio_gain(a->audio, voice, p->volume, p->pan, error))
      return 0;
  }
  if (p->submitted && !bk_audio_frequency(a->audio, voice, p->frequency, error))
    return 0;
  return 1;
}
BkPropAudio *bk_prop_audio_create(BkResourceStore *resources, BkAudio *audio,
                                  unsigned first, const BkPropAssets *props,
                                  const BkPropSoundState *retained,
                                  size_t retained_count, int32_t volume,
                                  char error[256]) {
  unsigned count = bk_prop_assets_count(props);
  if (!resources || !audio || !props || count > 16 || first > BK_AUDIO_VOICES ||
      count > BK_AUDIO_VOICES - first || volume < -10000 || volume > 0 ||
      (retained && retained_count < count) || bk_audio_stats(audio).failed) {
    fail(error, "invalid resources/mixer/voice range/states");
    return NULL;
  }
  BkPropAudio *a = calloc(1, sizeof(*a));
  if (!a) {
    fail(error, "allocation failed");
    return NULL;
  }
  a->audio = audio;
  a->first = first;
  for (unsigned i = 0; i < 10 && count; ++i) {
    a->clips[i] = bk_audio_clip_load(resources, "bk3_02", names[i], error);
    if (!a->clips[i])
      goto bad;
  }
  a->count = count;
  for (unsigned i = 0; i < count; ++i) {
    Instance *p = &a->instances[i];
    p->kind = bk_prop_assets_state(props, i)->kind;
    if (retained)
      p->state = retained[i];
    BkPropSoundCommand cmd;
    if (!bk_prop_sound_initial(p->kind, volume, &cmd) ||
        !apply(a, i, &cmd, error))
      goto bad;
  }
  return a;
bad:
  {
    char ignored[256];
    bk_prop_audio_stop(a, ignored);
  }
  bk_prop_audio_destroy(a);
  return NULL;
}
const BkPropSoundState *bk_prop_audio_state(const BkPropAudio *a, uint32_t i) {
  return a && i < a->count ? &a->instances[i].state : NULL;
}
int bk_prop_audio_set_stage(BkPropAudio *a, uint32_t i, int8_t stage) {
  if (!a || i >= a->count)
    return 0;
  a->instances[i].state.stage = stage;
  return 1;
}
static int transport(BkPropAudio *a, const BkPropInteractionSound *c,
                     char error[256]) {
  Instance *p = &a->instances[c->index];
  unsigned voice = a->first + c->index;
  if (!p->clip)
    return fail(error, "interaction requires a loaded buffer");
  if (!p->submitted) {
    /* Loaded secondary buffers exist even before their first Play. Coalescing
     * pause at the same producer boundary emits no samples when Stop is the
     * first interaction. Existing gain/frequency from512c0e are retained. */
    if (!bk_audio_play(a->audio, voice, p->clip, c->play ? c->loop : p->loop,
                       p->volume, p->pan, error) ||
        !bk_audio_frequency(a->audio, voice, p->frequency, error))
      return 0;
    p->submitted = 1;
  }
  if (c->play) {
    if (!bk_audio_resume(a->audio, voice, c->loop, error))
      return 0;
    p->loop = c->loop;
  } else if (!bk_audio_pause(a->audio, voice, error))
    return 0;
  return 1;
}
int bk_prop_audio_interact(BkPropAudio *a, BkPropAssets *props,
                           BkPlayerControl *player, int8_t *stimulus,
                           uint8_t *outcome, BkPropInteractionShared *shared,
                           const BkPropInteractionInput *in,
                           BkPropInteractionCommands *out, char error[256]) {
  if (!a || !props || !out || a->count != bk_prop_assets_count(props) ||
      bk_audio_stats(a->audio).failed)
    return fail(error, "invalid interaction instances/session");
  BkPropInteractionActor actors[16] = {0};
  for (unsigned i = 0; i < a->count; ++i) {
    if (!bk_prop_assets_bind_interaction(props, i, &actors[i]) ||
        actors[i].motion->kind != a->instances[i].kind)
      return fail(error, "prop kind changed without sound initialization");
    actors[i].sound = &a->instances[i].state;
  }
  BkPropInteractionCommands commands;
  if (!bk_prop_interaction_step(actors, player, stimulus, outcome, shared, in,
                                &commands, error))
    return 0;
  for (unsigned i = 0; i < commands.count; ++i)
    if (!transport(a, &commands.sounds[i], error))
      return 0;
  *out = commands;
  return 1;
}
typedef struct {
  BkPropAudio *audio;
  BkPropSoundInput input;
} Context;
static int placed(void *context, uint32_t i, const BkPropState *prop,
                  char error[256]) {
  Context *c = context;
  BkPropAudio *a = c->audio;
  Instance *p = &a->instances[i];
  BkPropSoundInput in = c->input;
  in.voice_present = p->clip != NULL;
  if (!bk_audio_playing(a->audio, a->first + i, &in.voice_playing))
    return fail(error, "cannot query current voice");
  BkPropSoundState next = p->state;
  BkPropSoundCommand cmd;
  if (!bk_prop_sound_step(&next, prop, &in, &cmd))
    return fail(error, "invalid sound policy");
  if (!apply(a, i, &cmd, error))
    return 0;
  p->state = next;
  return 1;
}
int bk_prop_audio_step(BkPropAudio *a, BkPropAssets *props,
                       BkPropShared *shared, const BkPropMotionInput *in,
                       const BkCollision *collision,
                       const BkNpcSceneInput *ground, size_t ground_count,
                       BkPropMotionEffects effects[16],
                       const BkPropSoundInput *sound, char error[256]) {
  if (!a || !props || !sound || a->count != bk_prop_assets_count(props))
    return fail(error, "missing/mismatched prop instances");
  for (unsigned i = 0; i < a->count; ++i)
    if (a->instances[i].kind != bk_prop_assets_state(props, i)->kind)
      return fail(error, "prop kind changed without sound initialization");
  Context context = {a, *sound};
  return bk_prop_assets_step_spatial_consume(props, shared, in, collision,
                                             ground, ground_count, effects,
                                             placed, &context, error);
}

int bk_prop_audio_restart(BkPropAudio *a, uint32_t index, char error[256]) {
  if (!a || index >= a->count || !a->instances[index].clip)
    return fail(error, "missing prop sound buffer");
  Instance *p = &a->instances[index];
  if (!bk_audio_play(a->audio, a->first + index, p->clip, 0, p->volume, p->pan,
                     error) ||
      !bk_audio_frequency(a->audio, a->first + index, p->frequency, error))
    return 0;
  p->loop = 0;
  p->submitted = 1;
  return 1;
}
