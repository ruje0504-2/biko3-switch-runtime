#include "scene/selection_audio.h"
#include "scene/selection_world.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#define CHECK(v)                                                               \
  do {                                                                         \
    if (!(v))                                                                  \
      goto done;                                                               \
  } while (0)
typedef struct {
  uint64_t submitted, consumed, samples, nonzero, hash;
} Sink;
static int submit(void *p, const int16_t *pcm, size_t frames, char e[256]) {
  (void)e;
  Sink *s = p;
  for (size_t i = 0; i < frames * 2; ++i) {
    uint16_t v = (uint16_t)pcm[i];
    s->nonzero += v != 0;
    s->hash = (s->hash ^ (v & 255)) * UINT64_C(1099511628211);
    s->hash = (s->hash ^ (v >> 8)) * UINT64_C(1099511628211);
  }
  s->samples += frames * 2;
  s->submitted += frames;
  return 1;
}
static int poll(void *p, uint64_t *consumed, char e[256]) {
  (void)e;
  *consumed = ((Sink *)p)->consumed;
  return 1;
}
typedef struct {
  BkSelectionAudio *audio;
  BkVoiceEnvelope envelope;
  float last;
  unsigned calls;
} Voice;
static int level(void *p, float seconds, float *out, char e[256]) {
  Voice *v = p;
  ++v->calls;
  if (!bk_selection_audio_voice_level(v->audio, &v->envelope, seconds, out, e))
    return 0;
  v->last = *out;
  return 1;
}
/* Independent decoded-PCM calculation, without the adapter or envelope API. */
static float reference(BkVoiceEnvelope *state, const BkAudioCursor *c,
                       float dt) {
  if (!c->playing || !c->buffered)
    return 0;
  size_t channels = bk_pcm_channels(c->pcm);
  size_t count = bk_pcm_frames(c->pcm) * channels;
  if (count < 221)
    return 0;
  const int16_t *p = bk_pcm_samples(c->pcm);
  int32_t sum = 0;
  for (unsigned i = 0; i < 220; ++i) {
    int32_t x = p[(c->source_frame * channels + i) % count];
    sum += x < 0 ? -x : x;
  }
  state->target = (float)((sum / 110) / 512.);
  if (state->target < state->smoothed) {
    state->smoothed = (float)((double)state->smoothed - 10. * dt);
    if (state->smoothed <= state->target)
      state->smoothed = state->target;
  } else if (state->target > state->smoothed) {
    state->smoothed = (float)((double)state->smoothed + 10. * dt);
    if (state->smoothed >= state->target)
      state->smoothed = state->target;
  } else
    state->smoothed = state->target;
  if (state->smoothed >= 9)
    state->smoothed = 9;
  return state->smoothed;
}
int main(int argc, char **argv) {
  if (argc != 2)
    return 2;
  char error[256] = {0}, path[1024];
  BkResourceStore *store = bk_resources_create(error);
  Sink sink = {.hash = UINT64_C(14695981039346656037)};
  BkAudio *mixer = bk_audio_create(
      &(BkAudioSink){&sink, 48000, 480, 1920, submit, poll}, error);
  BkSelectionAudio *audio = NULL;
  BkSelectionWorld *world = NULL;
  BkSelectionActorAssets *retired = NULL;
  unsigned frames = 0, levels = 0, audible = 0, unavailable = 0;
  int rc = 1;
  CHECK(store && mixer);
  const char *packs[] = {"bk3_01", "bk3_02", "bk3_03", "bk3_04", "bk3_06"};
  for (unsigned i = 0; i < 5; ++i) {
    snprintf(path, sizeof(path), "%s/%s.pp", argv[1], packs[i]);
    CHECK(bk_resources_mount(store, packs[i], path, error));
  }
  CHECK(bk_resources_mount_directory(store, "faces", argv[1], 20480, error));
  audio = bk_selection_audio_create(store, mixer, 60, 61, -900, error);
  CHECK(audio);
  BkMenuCamera camera = {0};
  for (unsigned i = 0; i < 16; ++i)
    camera.pose.world[i] = camera.matrix[i] = i % 5 == 0;
  uint32_t rng = 2468, clocks[4] = {1000, 1001, 1002, 1003};
  world = bk_selection_world_create(store, 0, 0, .016f, &camera, clocks, &rng,
                                    error);
  CHECK(world);
  Voice voice = {.audio = audio};
  BkSelectionWorldOps ops = {.context = &voice, .voice_level = level};
  for (unsigned group = 0; group < 5; ++group) {
    if (group) {
      CHECK(bk_selection_audio_release_voice(audio, error));
      CHECK(bk_selection_world_replace(world, group, 0, clocks, &rng, &retired,
                                       error));
    }
    BkAudioClip *clip =
        bk_selection_actor_assets_voice(bk_selection_world_body(world));
    CHECK(bk_selection_audio_bind_voice(audio, clip, -700, error));
    int playing;
    CHECK(bk_selection_audio_voice_status(audio, &playing, error));
    assert(!playing);
    bk_selection_actor_assets_destroy(retired);
    retired = NULL;
    for (unsigned tick = 0; tick < 240; ++tick) {
      uint64_t advance = (unsigned[]){0, 240, 480, 960, 1920, 3000}[tick % 6];
      uint64_t queued = sink.submitted - sink.consumed;
      sink.consumed += advance < queued ? advance : queued;
      CHECK(bk_audio_poll(mixer, error));
      if (tick == 1 || tick == 32 || tick == 100 || tick == 180)
        CHECK(bk_selection_audio_voice_play(audio, -700 - (int32_t)group * 200,
                                            error));
      if (tick == 40 || tick == 120)
        CHECK(bk_selection_audio_voice_stop(audio, error));
      if (tick == 63)
        CHECK(bk_selection_audio_release_voice(audio, error));
      if (tick == 64)
        CHECK(bk_selection_audio_bind_voice(audio, clip, -700, error));
      if (tick % 30 == 0)
        CHECK(bk_selection_audio_music_gain(
            audio, -900 - (int32_t)(tick % 100) * 30, error));
      if (tick == 72) {
        CHECK(bk_audio_fill(mixer, error));
        CHECK(bk_selection_audio_voice_status(audio, &playing, error));
        assert(!playing); /* Binding never emits a transient play epoch. */
        BkAudioCursor stopped;
        assert(bk_audio_cursor(mixer, 61, &stopped));
        assert(!stopped.playing);
      }
      BkSelectionWorldInput in = {
          .selected = group,
          .camera_mode = (tick / 20) % 2,
          .voice_active = tick % 5 != 0,
          .seconds = (float[]){0, .016f, .033f, .1f, .22f}[tick % 5],
          .timestamp = 1100 + frames * 50};
      for (unsigned i = 0; i < 3; ++i)
        in.face_clocks[i] = in.timestamp + i + 1;
      BkVoiceEnvelope expected = voice.envelope;
      BkAudioCursor cursor;
      assert(bk_audio_cursor(mixer, 61, &cursor));
      float wanted =
          in.voice_active ? reference(&expected, &cursor, in.seconds) : 0;
      unsigned calls = voice.calls;
      CHECK(bk_selection_world_step(world, &in, &ops, &rng, error));
      assert(!memcmp(&voice.envelope, &expected, sizeof(expected)));
      assert(voice.calls == calls + in.voice_active);
      if (in.voice_active) {
        assert(wanted == voice.last);
        ++levels;
        audible += cursor.playing && cursor.buffered;
        unavailable += !cursor.playing || !cursor.buffered;
      }
      const BkFrameVisit *visits;
      uint32_t count;
      CHECK(bk_actor_forest_draw(bk_selection_world_forest(world), 0, &visits,
                                 &count, error));
      CHECK(bk_audio_fill(mixer, error));
      ++frames;
    }
  }
  CHECK(bk_selection_audio_stop(audio, error));
  CHECK(bk_selection_audio_voice_status(audio, &(int){0}, error));
  assert(sink.nonzero && audible > 100 && unavailable > 100);
  printf("selection audio PASS: 5 real voices, %u world frames, %u exact "
         "envelopes "
         "(%u audible/%u inactive), %llu PCM samples, hash %016llx\n",
         frames, levels, audible, unavailable, (unsigned long long)sink.samples,
         (unsigned long long)sink.hash);
  rc = 0;
done:
  if (rc)
    fprintf(stderr, "%s\n", error);
  if (audio) {
    char ignored[256];
    bk_selection_audio_stop(audio, ignored);
  }
  bk_selection_audio_destroy(audio);
  bk_selection_actor_assets_destroy(retired);
  bk_selection_world_destroy(world);
  bk_audio_destroy(mixer);
  bk_resources_destroy(store);
  return rc;
}
