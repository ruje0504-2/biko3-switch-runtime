#include "scene/npc_audio.h"
#include "scene/voice_audio.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
static const char *const names[] = {"se106.wav", "se107.wav", "se116.wav",
                                    "se117.wav", "se118.wav", "se144.wav",
                                    "se148.wav"};
struct BkNpcAudio {
  BkResourceStore *resources;
  BkAudio *audio;
  unsigned footsteps, speech;
  BkAudioClip *clips[7], *speech_clip;
  int32_t speech_volume;
};
static int fail(char *error, const char *message) {
  snprintf(error, 256, "NPC audio: %s", message);
  return 0;
}
BkNpcAudio *bk_npc_audio_create(BkResourceStore *resources, BkAudio *audio,
                                unsigned footsteps, unsigned speech,
                                char error[256]) {
  if (!resources || !audio || footsteps >= BK_AUDIO_VOICES ||
      speech >= BK_AUDIO_VOICES || footsteps == speech ||
      bk_audio_stats(audio).failed) {
    fail(error, "invalid resources/mixer/voice slots");
    return NULL;
  }
  BkNpcAudio *npc = calloc(1, sizeof(*npc));
  if (!npc) {
    fail(error, "allocation failed");
    return NULL;
  }
  npc->resources = resources;
  npc->audio = audio;
  npc->footsteps = footsteps;
  npc->speech = speech;
  for (unsigned i = 0; i < 7; i++) {
    npc->clips[i] = bk_audio_clip_load(resources, "bk3_02", names[i], error);
    if (!npc->clips[i]) {
      bk_npc_audio_destroy(npc);
      return NULL;
    }
  }
  return npc;
}
void bk_npc_audio_destroy(BkNpcAudio *npc) {
  if (!npc)
    return;
  for (unsigned i = 0; i < 7; i++)
    bk_audio_clip_release(npc->clips[i]);
  bk_audio_clip_release(npc->speech_clip);
  free(npc);
}
int bk_npc_audio_stop(BkNpcAudio *npc, char error[256]) {
  if (!npc)
    return fail(error, "missing instance");
  return bk_audio_clear(npc->audio, npc->footsteps, error) &&
         bk_audio_clear(npc->audio, npc->speech, error);
}
int bk_npc_audio_footsteps(BkNpcAudio *npc, const BkEntryNpcFootsteps *events,
                           char error[256]) {
  if (!npc || !events || events->footsteps.count > 8)
    return fail(error, "invalid footstep events");
  if (!events->footsteps.count)
    return 1;
  if (!events->footsteps.sound_file)
    return fail(error, "missing footstep name");
  unsigned clip = 0;
  while (clip < 7 && strcmp(names[clip], events->footsteps.sound_file))
    clip++;
  if (clip == 7)
    return fail(error, "unknown footstep name");
  for (unsigned i = 0; i < events->footsteps.count; i++)
    if (!bk_audio_play(npc->audio, npc->footsteps, npc->clips[clip], 0,
                       events->audio.volume, events->audio.pan, error))
      return 0;
  return 1;
}
int bk_npc_audio_prepare_speech(BkNpcAudio *npc, const char *pack,
                                const char *name, int32_t volume,
                                char error[256]) {
  if (!npc || !pack || !name || volume < -10000 || volume > 0)
    return fail(error, "invalid speech resource");
  BkAudioClip *clip = bk_audio_clip_load(npc->resources, pack, name, error);
  if (!clip)
    return 0;
  if (!bk_audio_clear(npc->audio, npc->speech, error)) {
    bk_audio_clip_release(clip);
    return 0;
  }
  bk_audio_clip_release(npc->speech_clip);
  npc->speech_clip = clip;
  npc->speech_volume = volume;
  return 1;
}
int bk_npc_audio_restart_speech(BkNpcAudio *npc, int loop, char error[256]) {
  if (!npc || !npc->speech_clip || (loop != 0 && loop != 1))
    return fail(error, "missing speech buffer/invalid loop");
  return bk_audio_play(npc->audio, npc->speech, npc->speech_clip, loop,
                       npc->speech_volume, 0, error);
}
int bk_npc_audio_speak(BkNpcAudio *npc, const char *pack, const char *name,
                       int loop, int32_t volume, char error[256]) {
  if (loop != 0 && loop != 1)
    return fail(error, "invalid speech loop");
  return bk_npc_audio_prepare_speech(npc, pack, name, volume, error) &&
         bk_npc_audio_restart_speech(npc, loop, error);
}
int bk_npc_audio_voice(BkNpcAudio *npc, BkVoiceEnvelope *shared, float seconds,
                       float *level, char error[256]) {
  if (!npc)
    return fail(error, "missing instance");
  return bk_scene_voice_envelope(npc->audio, npc->speech, shared, seconds,
                                 level, error);
}
int bk_npc_frame_tail(BkEntryAssets *entry, BkNpcAudio *audio,
                      BkNpcSpatialState *state, BkFaceState *face,
                      uint32_t *random, BkVoiceEnvelope *envelope,
                      uint8_t *latches, size_t latch_count,
                      const BkNpcFrameTailInput *input, char error[256]) {
  if (!entry || !audio || !state || !face || !random || !envelope || !input ||
      !isfinite(input->presentation.seconds) || input->presentation.seconds < 0)
    return fail(error, "invalid frame tail");
  BkEntryNpcFootsteps events;
  if (!bk_entry_assets_prepare_npc_footsteps(entry, state, &input->actions,
                                             input->effect_volume, latches,
                                             latch_count, &events, error) ||
      !bk_npc_audio_footsteps(audio, &events, error))
    return 0;
  BkNpcShadow *shadow = bk_entry_assets_shadow(entry);
  if (shadow &&
      !bk_npc_shadow_place(
          shadow, bk_actor_pose_placement(bk_entry_assets_actor(entry)), error))
    return 0;
  BkEntryNpcPresentation presentation = input->presentation;
  if (!bk_npc_audio_voice(audio, envelope, presentation.seconds,
                          &presentation.voice_level, error))
    return 0;
  return bk_entry_assets_step_npc_presentation(entry, state, face, random,
                                               &presentation, error);
}

int bk_npc_audio_release_speech(BkNpcAudio *npc, char error[256]) {
  if (!npc || !bk_audio_clear(npc->audio, npc->speech, error))
    return 0;
  bk_audio_clip_release(npc->speech_clip);
  npc->speech_clip = NULL;
  return 1;
}
