#include "media/audio.h"
#include "media/pcm_mix.h"
#include <math.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
struct BkAudioClip {
  BkPcm *pcm;
  float baseline;
  atomic_size_t references;
};
typedef struct Epoch {
  struct Epoch *next;
  BkAudioClip *clip;
  uint64_t boundary, origin, started;
  BkPcmPhase source;
  uint32_t frequency;
  int loop, paused;
  int32_t volume, pan;
} Epoch;
struct BkAudio {
  BkAudioSink sink;
  BkAudioSync sync;
  BkAudioClip *cursor_clip[BK_AUDIO_VOICES];
  BkAudioStats stats;
  Epoch *first[BK_AUDIO_VOICES], *last[BK_AUDIO_VOICES];
  float *mix;
  int16_t *output;
  float output_gain;
};
static int fail(char *error, const char *message) {
  snprintf(error, 256, "audio: %s", message);
  return 0;
}
BkAudioClip *bk_audio_clip_decode(const void *bytes, size_t size,
                                  char error[256]) {
  BkPcm *pcm = bk_pcm_decode(bytes, size, error);
  if (!pcm)
    return NULL;
  BkAudioClip *clip = calloc(1, sizeof(*clip));
  if (!clip) {
    bk_pcm_destroy(pcm);
    fail(error, "clip allocation failed");
    return NULL;
  }
  clip->pcm = pcm;
  clip->baseline = 1.f;
  atomic_init(&clip->references, 1);
  return clip;
}
BkAudioClip *bk_audio_clip_load(BkResourceStore *resources, const char *pack,
                                const char *name, char error[256]) {
  BkBlob blob = {0};
  if (bk_resources_read(resources, pack, name, &blob, error) != BK_RESOURCE_OK)
    return NULL;
  BkAudioClip *clip = bk_audio_clip_decode(blob.data, blob.size, error);
  bk_blob_free(&blob);
  return clip;
}
BkAudioClip *bk_audio_clip_load_music(BkResourceStore *resources,
                                      const char *pack, const char *name,
                                      char error[256]) {
  BkAudioClip *clip = bk_audio_clip_load(resources, pack, name, error);
  if (clip)
    clip->baseline = 1.5f;
  return clip;
}
void bk_audio_clip_release(BkAudioClip *clip) {
  if (clip && atomic_fetch_sub_explicit(&clip->references, 1,
                                        memory_order_acq_rel) == 1) {
    bk_pcm_destroy(clip->pcm);
    free(clip);
  }
}
uint32_t bk_audio_clip_rate(const BkAudioClip *clip) {
  return clip ? bk_pcm_rate(clip->pcm) : 0;
}
static void release_epoch(Epoch *epoch) {
  bk_audio_clip_release(epoch->clip);
  free(epoch);
}
BkAudio *bk_audio_create(const BkAudioSink *sink, char error[256]) {
  if (!sink || !sink->submit || !sink->poll || !sink->rate ||
      sink->rate > 192000 || !sink->block_frames || sink->block_frames > 4096 ||
      sink->capacity_frames < sink->block_frames ||
      sink->capacity_frames > 192000 ||
      sink->capacity_frames % sink->block_frames) {
    fail(error, "invalid sink format/capacity");
    return NULL;
  }
  BkAudio *audio = calloc(1, sizeof(*audio));
  if (!audio) {
    fail(error, "allocation failed");
    return NULL;
  }
  audio->sink = *sink;
  audio->output_gain = 1.f;
  audio->mix = calloc(sink->block_frames * 2, sizeof(*audio->mix));
  audio->output = calloc(sink->block_frames * 2, sizeof(*audio->output));
  if (!audio->mix || !audio->output) {
    bk_audio_destroy(audio);
    fail(error, "mix allocation failed");
    return NULL;
  }
  return audio;
}
int bk_audio_set_output_gain(BkAudio *audio, float gain, char error[256]) {
  if (!audio || audio->stats.failed || audio->stats.submitted ||
      !isfinite(gain) || gain < 0)
    return fail(error, "invalid output gain or playback already started");
  audio->output_gain = gain;
  return 1;
}
void bk_audio_destroy(BkAudio *audio) {
  if (!audio)
    return;
  for (unsigned i = 0; i < BK_AUDIO_VOICES; i++) {
    bk_audio_clip_release(audio->cursor_clip[i]);
    Epoch *e = audio->first[i];
    while (e) {
      Epoch *next = e->next;
      release_epoch(e);
      e = next;
    }
  }
  free(audio->mix);
  free(audio->output);
  free(audio);
}
static int valid_voice(BkAudio *audio, unsigned voice, char *error) {
  if (!audio || audio->stats.failed || voice >= BK_AUDIO_VOICES)
    return fail(error, "invalid/failed session or voice");
  return 1;
}
static int gain_valid(int32_t volume, int32_t pan) {
  return volume >= -10000 && volume <= 0 && pan >= -10000 && pan <= 10000;
}
static int command(BkAudio *audio, unsigned voice, const Epoch *value,
                   char *error) {
  Epoch *tail = audio->last[voice];
  int reuse = tail && tail->boundary == audio->stats.submitted;
  Epoch *e = reuse ? tail : malloc(sizeof(*e));
  if (!e)
    return fail(error, "voice epoch allocation failed");
  if (value->clip)
    atomic_fetch_add_explicit(&value->clip->references, 1,
                              memory_order_relaxed);
  if (reuse)
    bk_audio_clip_release(e->clip);
  *e = *value;
  e->next = NULL;
  e->boundary = audio->stats.submitted;
  if (!reuse) {
    if (tail)
      tail->next = e;
    else
      audio->first[voice] = e;
    audio->last[voice] = e;
  }
  return 1;
}
static int audio_play(BkAudio *audio, unsigned voice, BkAudioClip *clip,
                      int loop, int32_t volume, int32_t pan, char error[256]) {
  if (!valid_voice(audio, voice, error))
    return 0;
  if (!clip || (loop != 0 && loop != 1) || !gain_valid(volume, pan))
    return fail(error, "invalid play parameters");
  Epoch e = {.clip = clip,
             .origin = audio->stats.submitted,
             .started = audio->stats.submitted,
             .frequency = bk_pcm_rate(clip->pcm),
             .loop = loop,
             .volume = volume,
             .pan = pan};
  return command(audio, voice, &e, error);
}
static int audio_gain(BkAudio *audio, unsigned voice, int32_t volume,
                      int32_t pan, char error[256]) {
  if (!valid_voice(audio, voice, error))
    return 0;
  if (!gain_valid(volume, pan))
    return fail(error, "invalid gain parameters");
  Epoch *tail = audio->last[voice];
  if (!tail || !tail->clip)
    return fail(error, "gain requires a loaded voice");
  if (tail->volume == volume && tail->pan == pan)
    return 1;
  Epoch e = *tail;
  e.volume = volume;
  e.pan = pan;
  return command(audio, voice, &e, error);
}
static int epoch_phase(const BkAudio *audio, const Epoch *e, uint64_t clock,
                       BkPcmPhase *out) {
  if (e->paused) {
    *out = e->source;
    return 1;
  }
  uint64_t elapsed = clock > e->origin ? clock - e->origin : 0;
  return bk_pcm_phase_advance(e->clip->pcm, audio->sink.rate, e->frequency,
                              elapsed, e->loop, e->source, out);
}
static int audio_frequency(BkAudio *audio, unsigned voice, uint32_t hz,
                           char error[256]) {
  if (!valid_voice(audio, voice, error))
    return 0;
  Epoch *tail = audio->last[voice];
  if (!tail || !tail->clip)
    return fail(error, "frequency requires a loaded voice");
  if (hz && (hz < 100 || hz > 100000))
    return fail(error, "invalid playback frequency");
  if (!hz)
    hz = bk_pcm_rate(tail->clip->pcm);
  if (hz == tail->frequency)
    return 1;
  Epoch e = *tail;
  if (!epoch_phase(audio, tail, audio->stats.submitted, &e.source))
    return fail(error, "invalid playback phase");
  e.frequency = hz;
  e.origin = audio->stats.submitted;
  return command(audio, voice, &e, error);
}
static int audio_pause(BkAudio *audio, unsigned voice, char error[256]) {
  if (!valid_voice(audio, voice, error))
    return 0;
  Epoch *tail = audio->last[voice];
  if (!tail || !tail->clip)
    return fail(error, "pause requires a loaded voice");
  if (tail->paused)
    return 1;
  Epoch e = *tail;
  if (!epoch_phase(audio, tail, audio->stats.submitted, &e.source))
    return fail(error, "invalid playback phase");
  e.origin = audio->stats.submitted;
  e.paused = 1;
  return command(audio, voice, &e, error);
}
static int audio_resume(BkAudio *audio, unsigned voice, int loop,
                        char error[256]) {
  if (!valid_voice(audio, voice, error))
    return 0;
  Epoch *tail = audio->last[voice];
  if (!tail || !tail->clip || (loop != 0 && loop != 1))
    return fail(error, "resume requires a loaded voice and valid loop flag");
  Epoch e = *tail;
  if (!epoch_phase(audio, tail, audio->stats.submitted, &e.source))
    return fail(error, "invalid playback phase");
  int ended = e.source.frame == bk_pcm_frames(e.clip->pcm);
  if (!tail->paused && !ended && tail->loop == loop)
    return 1;
  if (ended)
    e.source = (BkPcmPhase){0};
  if (tail->paused || ended)
    e.started = audio->stats.submitted;
  e.origin = audio->stats.submitted;
  e.paused = 0;
  e.loop = loop;
  return command(audio, voice, &e, error);
}
static int audio_clear(BkAudio *audio, unsigned voice, char error[256]) {
  if (!valid_voice(audio, voice, error))
    return 0;
  Epoch *tail = audio->last[voice];
  if (!tail || !tail->clip)
    return 1;
  Epoch e = {0};
  return command(audio, voice, &e, error);
}
static int audio_poll(BkAudio *audio, char error[256]) {
  if (!audio || audio->stats.failed)
    return fail(error, "invalid/failed session");
  uint64_t consumed = 0;
  if (!audio->sink.poll(audio->sink.context, &consumed, error)) {
    audio->stats.failed = 1;
    return 0;
  }
  if (consumed < audio->stats.consumed || consumed > audio->stats.submitted) {
    audio->stats.failed = 1;
    return fail(error, "sink returned invalid consumed clock");
  }
  if (consumed == audio->stats.submitted && consumed > audio->stats.consumed)
    audio->stats.queue_drains++;
  audio->stats.consumed = consumed;
  for (unsigned i = 0; i < BK_AUDIO_VOICES; i++) {
    Epoch *e = audio->first[i];
    while (e && e->next && e->next->boundary <= consumed) {
      Epoch *next = e->next;
      release_epoch(e);
      e = next;
    }
    audio->first[i] = e;
  }
  return 1;
}
static int audio_fill(BkAudio *audio, char error[256]) {
  if (!audio || audio->stats.failed)
    return fail(error, "invalid/failed session");
  size_t frames = audio->sink.block_frames;
  while (audio->stats.submitted - audio->stats.consumed + frames <=
         audio->sink.capacity_frames) {
    if (audio->stats.submitted > UINT64_MAX - frames) {
      audio->stats.failed = 1;
      return fail(error, "output clock exhausted");
    }
    memset(audio->mix, 0, frames * 2 * sizeof(*audio->mix));
    for (unsigned i = 0; i < BK_AUDIO_VOICES; i++) {
      Epoch *e = audio->last[i];
      BkPcmPhase phase;
      if (e && e->clip && !e->paused &&
          (!epoch_phase(audio, e, audio->stats.submitted, &phase) ||
           !bk_pcm_mix_phase_gain(e->clip->pcm, audio->sink.rate, e->frequency,
                                  phase, e->loop, e->volume, e->pan,
                                  e->clip->baseline, audio->mix, frames, error))) {
        audio->stats.failed = 1;
        return 0;
      }
    }
    if (!bk_pcm_quantize_gain(audio->mix, audio->output, frames,
                              audio->output_gain)) {
      audio->stats.failed = 1;
      return fail(error, "mixed samples are invalid");
    }
    if (!audio->sink.submit(audio->sink.context, audio->output, frames,
                            error)) {
      audio->stats.failed = 1;
      return 0;
    }
    audio->stats.submitted += frames;
  }
  return 1;
}
static int audio_cursor(BkAudio *audio, unsigned voice, BkAudioCursor *out) {
  if (!audio || audio->stats.failed || voice >= BK_AUDIO_VOICES || !out)
    return 0;
  BkAudioCursor cursor = {0};
  Epoch *e = audio->first[voice];
  while (e && e->next && e->next->boundary <= audio->stats.consumed)
    e = e->next;
  cursor.pending_change = e && (e->boundary > audio->stats.consumed || e->next);
  if (e && e->clip && e->boundary <= audio->stats.consumed) {
    BkPcmPhase phase;
    cursor.pcm = e->clip->pcm;
    if (!epoch_phase(audio, e, audio->stats.consumed, &phase))
      return 0;
    cursor.source_frame = phase.frame;
    cursor.playing = !e->paused && phase.frame < bk_pcm_frames(cursor.pcm);
    cursor.buffered = audio->stats.consumed < audio->stats.submitted;
  }
  /* The pump can collect old epochs immediately after unlock. Retain the
   * audible clip until this voice's next cursor query, not its next pump. */
  BkAudioClip *hold = cursor.pcm ? e->clip : NULL;
  if (hold)
    atomic_fetch_add_explicit(&hold->references, 1, memory_order_relaxed);
  bk_audio_clip_release(audio->cursor_clip[voice]);
  audio->cursor_clip[voice] = hold;
  *out = cursor;
  return 1;
}
static BkAudioStats audio_stats(const BkAudio *audio) {
  return audio ? audio->stats : (BkAudioStats){.failed = 1};
}

static int audio_playing(const BkAudio *audio, unsigned voice, int *playing) {
  if (!audio || audio->stats.failed || voice >= BK_AUDIO_VOICES || !playing)
    return 0;
  const Epoch *e = audio->last[voice];
  int result = 0;
  if (e && e->clip && !e->paused) {
    if (e->started > audio->stats.consumed) {
      result = 1; /* An explicitly requested new play, still queued. */
    } else {
      /* Future gain/frequency changes must not report natural completion
       * early: the same play is still audible under an older rate epoch. */
      e = audio->first[voice];
      while (e && e->next && e->next->boundary <= audio->stats.consumed)
        e = e->next;
      BkPcmPhase phase;
      if (!e || !e->clip ||
          !epoch_phase(audio, e, audio->stats.consumed, &phase))
        return 0;
      result = !e->paused && phase.frame < bk_pcm_frames(e->clip->pcm);
    }
  }
  *playing = result;
  return 1;
}

int bk_audio_set_sync(BkAudio *a, const BkAudioSync *sync, char error[256]) {
  if (!a || (sync && (!sync->lock || !sync->unlock)))
    return fail(error, "invalid synchronization hooks");
  a->sync = sync ? *sync : (BkAudioSync){0};
  return 1;
}
static void lock_audio(const BkAudio *a) {
  if (a && a->sync.lock)
    a->sync.lock(a->sync.context);
}
static void unlock_audio(const BkAudio *a) {
  if (a && a->sync.unlock)
    a->sync.unlock(a->sync.context);
}
int bk_audio_play(BkAudio *a, unsigned v, BkAudioClip *c, int loop,
                  int32_t volume, int32_t pan, char e[256]) {
  lock_audio(a);
  int ok = audio_play(a, v, c, loop, volume, pan, e);
  unlock_audio(a);
  return ok;
}
int bk_audio_gain(BkAudio *a, unsigned v, int32_t volume, int32_t pan,
                  char e[256]) {
  lock_audio(a);
  int ok = audio_gain(a, v, volume, pan, e);
  unlock_audio(a);
  return ok;
}
int bk_audio_get_gain(const BkAudio *a, unsigned v, int32_t *volume,
                      int32_t *pan) {
  lock_audio(a);
  int ok = a && !a->stats.failed && v < BK_AUDIO_VOICES && volume && pan;
  const Epoch *tail = ok ? a->last[v] : NULL;
  ok = ok && tail && tail->clip;
  if (ok) {
    *volume = tail->volume;
    *pan = tail->pan;
  }
  unlock_audio(a);
  return ok;
}
int bk_audio_frequency(BkAudio *a, unsigned v, uint32_t hz, char e[256]) {
  lock_audio(a);
  int ok = audio_frequency(a, v, hz, e);
  unlock_audio(a);
  return ok;
}
int bk_audio_pause(BkAudio *a, unsigned v, char e[256]) {
  lock_audio(a);
  int ok = audio_pause(a, v, e);
  unlock_audio(a);
  return ok;
}
int bk_audio_resume(BkAudio *a, unsigned v, int loop, char e[256]) {
  lock_audio(a);
  int ok = audio_resume(a, v, loop, e);
  unlock_audio(a);
  return ok;
}
int bk_audio_clear(BkAudio *a, unsigned v, char e[256]) {
  lock_audio(a);
  int ok = audio_clear(a, v, e);
  unlock_audio(a);
  return ok;
}
int bk_audio_poll(BkAudio *a, char e[256]) {
  lock_audio(a);
  int ok = audio_poll(a, e);
  unlock_audio(a);
  return ok;
}
int bk_audio_fill(BkAudio *a, char e[256]) {
  lock_audio(a);
  int ok = audio_fill(a, e);
  unlock_audio(a);
  return ok;
}
int bk_audio_cursor(const BkAudio *a, unsigned v, BkAudioCursor *out) {
  lock_audio(a);
  int ok = audio_cursor((BkAudio *)a, v, out);
  unlock_audio(a);
  return ok;
}
int bk_audio_playing(const BkAudio *a, unsigned v, int *out) {
  lock_audio(a);
  int ok = audio_playing(a, v, out);
  unlock_audio(a);
  return ok;
}
BkAudioStats bk_audio_stats(const BkAudio *a) {
  lock_audio(a);
  BkAudioStats stats = audio_stats(a);
  unlock_audio(a);
  return stats;
}
