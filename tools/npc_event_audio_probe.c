/* Real PCM and independent continuous resampling reference for NPC AI/wait/
 * route events, shared boundary replacement, retained initialization gains,
 * invalid-command preflight and separately owned service lifetimes. Offline. */
#include "scene/npc_event_audio.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <inttypes.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
typedef struct {
  const BkPcm *pcm;
  uint64_t phase;
  double gain[2];
} Voice;
typedef struct {
  Voice voices[5];
  uint64_t submitted, consumed, samples, nonzero, hash;
} Sink;
static void play(Sink *s, unsigned voice, const BkPcm *pcm, int volume,
                 int pan) {
  Voice *v = &s->voices[voice];
  *v = (Voice){.pcm = pcm};
  v->gain[0] = pow(10., (volume - (pan > 0 ? pan : 0)) / 2000.);
  v->gain[1] = pow(10., (volume + (pan < 0 ? pan : 0)) / 2000.);
}
static int submit(void *context, const int16_t *pcm, size_t frames,
                  char *error) {
  Sink *s = context;
  for (size_t i = 0; i < frames; ++i) {
    float sum[2] = {0};
    for (unsigned j = 0; j < 5; ++j) {
      Voice *v = &s->voices[j];
      if (!v->pcm)
        continue;
      size_t count = bk_pcm_frames(v->pcm), at = v->phase / 48000;
      if (at >= count)
        continue;
      const int16_t *source = bk_pcm_samples(v->pcm);
      size_t next = at + 1 < count ? at + 1 : at;
      double value = source[at] + (source[next] - source[at]) *
                                      ((double)(v->phase % 48000) / 48000.);
      for (unsigned c = 0; c < 2; ++c)
        sum[c] += (float)(value * v->gain[c]);
      v->phase += bk_pcm_rate(v->pcm);
    }
    for (unsigned c = 0; c < 2; ++c) {
      int expected = sum[c] <= -32768  ? -32768
                     : sum[c] >= 32767 ? 32767
                                       : (int)lroundf(sum[c]);
      if (abs(pcm[2 * i + c] - expected) > 1) {
        snprintf(error, 256,
                 "reference mismatch frame %" PRIu64 " channel %u: %d != %d",
                 s->submitted + i, c, pcm[2 * i + c], expected);
        return 0;
      }
      uint16_t bits = (uint16_t)pcm[2 * i + c];
      for (unsigned b = 0; b < 2; ++b) {
        s->hash ^= (bits >> (8 * b)) & 255;
        s->hash *= UINT64_C(1099511628211);
      }
      s->samples++;
      s->nonzero += pcm[2 * i + c] != 0;
    }
  }
  s->submitted += frames;
  return 1;
}
static int poll(void *context, uint64_t *consumed, char *error) {
  (void)error;
  *consumed = ((Sink *)context)->consumed;
  return 1;
}
#define CHECK(x)                                                               \
  do {                                                                         \
    if (!(x))                                                                  \
      goto done;                                                               \
  } while (0)
int main(int argc, char **argv) {
  if (argc != 2)
    return 2;
  char error[256] = {0}, path[1024];
  int rc = 1;
  BkResourceStore *store = bk_resources_create(error);
  BkAudio *audio = NULL;
  BkPlayerAudio *player = NULL;
  BkAreaAudio *area = NULL;
  BkSystemAudio *system = NULL;
  BkNpcEventAudio *events = NULL;
  BkPcm *pcm[8] = {0};
  BkBlob blob = {0};
  Sink sink = {.hash = UINT64_C(14695981039346656037)};
  const char *names[] = {"se002.wav", "se114.wav", "se115.wav", "se151.wav",
                         "se152.wav", "se304.wav", "se155.wav", "se156.wav"};
  unsigned ais = 0, waits = 0, routes = 0, replacements = 0, rejected = 0;
  CHECK(store);
  CHECK(snprintf(path, sizeof(path), "%s/bk3_02.pp", argv[1]) <
        (int)sizeof(path));
  CHECK(bk_resources_mount(store, "bk3_02", path, error));
  for (unsigned i = 0; i < 8; ++i) {
    CHECK(bk_resources_read(store, "bk3_02", names[i], &blob, error) ==
          BK_RESOURCE_OK);
    pcm[i] = bk_pcm_decode(blob.data, blob.size, error);
    bk_blob_free(&blob);
    CHECK(pcm[i]);
    assert(bk_pcm_channels(pcm[i]) == 1 && bk_pcm_rate(pcm[i]) == 22050);
  }
  BkAudioSink output = {&sink, 48000, 240, 960, submit, poll};
  audio = bk_audio_create(&output, error);
  CHECK(audio);
  player = bk_player_audio_create(store, audio, 0, error);
  CHECK(player);
  area = bk_area_audio_create(store, audio, 1, player, error);
  CHECK(area);
  system = bk_system_audio_create(store, audio, 2, -400, error);
  CHECK(system);
  events =
      bk_npc_event_audio_create(store, audio, 3, 4, -200, area, system, error);
  CHECK(events);
  assert(!bk_npc_event_audio_create(store, audio, 3, 3, -200, area, system,
                                    error));
  assert(!bk_system_audio_create(store, audio, 32, 0, error));
  BkResourceStore *empty = bk_resources_create(error);
  CHECK(empty);
  assert(!bk_npc_event_audio_create(empty, audio, 3, 4, -200, area, system,
                                    error));
  assert(!bk_system_audio_create(empty, audio, 2, 0, error));
  bk_resources_destroy(empty);
  const unsigned profiles[][4] = {{0, 1, 59, 4},  {1, 1, 83, 3},
                                  {1, 5, 75, 5},  {2, 6, 155, 6},
                                  {2, 8, 171, 6}, {4, 3, 27, 4}};
  for (unsigned frame = 0; frame < 1200; ++frame) {
    sink.consumed = sink.submitted;
    CHECK(bk_audio_poll(audio, error));
    const unsigned *p = profiles[(frame / 7) % 6];
    BkNpcSpatialEffects e = {0};
    float listener[3] = {7, 13, 29}, yaw = (float)((frame * 7) % 360);
    int live_volume = -(int)(frame % 31) * 87;
    if (frame % 64 == 0 || frame % 64 == 12)
      e.ai.sound = 1;
    if (frame % 64 == 6)
      e.ai.sound = 2;
    if (frame % 64 == 0)
      e.route.point.play_wait_sound = 1;
    if (frame % 7 == 3) {
      e.route.point.play_wait_sound = 0;
      e.route.point.play_route_sound = 1;
      e.route.sound_cursor = p[2];
      e.route.sound_position[0] = (float)(frame % 51) * 4;
      e.route.sound_position[1] = (float)(frame % 29) - 14;
      e.route.sound_position[2] = (float)(frame % 19) * -7;
    }
    CHECK(bk_npc_event_audio_apply(events, &e, p[0], p[1], listener, yaw,
                                   live_volume, error));
    if (e.ai.sound) {
      play(&sink, 2 + e.ai.sound, pcm[e.ai.sound], -200, 0);
      ais++;
    }
    if (e.route.point.play_wait_sound) {
      play(&sink, 2, pcm[0], -400, 0);
      waits++;
    }
    if (e.route.point.play_route_sound) {
      BkSpatialAudio gain;
      CHECK(bk_spatial_audio(&gain, e.route.sound_position, listener, yaw,
                             live_volume, 6));
      play(&sink, 1, pcm[p[3]], gain.volume, gain.pan);
      routes++;
      if (frame % 3 == 0) {
        BkAreaBoundaryCommands boundary = {
            2,
            {{BK_AREA_SOUND_NPC, "se156.wav", {-450, 210}},
             {BK_AREA_SOUND_PLAYER, "se304.wav", {-390, -120}}}};
        CHECK(bk_area_audio_apply(area, &boundary, error));
        play(&sink, 1, pcm[7], -450, 210);
        play(&sink, 0, pcm[5], -390, -120);
        replacements++;
      }
    }
    if (frame % 31 == 0) {
      CHECK(bk_system_audio_wait(system, error));
      play(&sink, 2, pcm[0], -400, 0);
      waits++;
    }
    if (frame % 64 == 40) {
      CHECK(bk_npc_event_audio_stop(events, error));
      sink.voices[3].pcm = sink.voices[4].pcm = NULL;
    }
    if (frame % 43 == 0) {
      CHECK(bk_area_audio_stop(area, error));
      sink.voices[1].pcm = NULL;
    }
    if (frame % 47 == 0) {
      CHECK(bk_system_audio_stop(system, error));
      sink.voices[2].pcm = NULL;
    }
    if (frame % 17 == 0) {
      BkNpcSpatialEffects bad = {.ai.sound = 1,
                                 .route.point.play_route_sound = 1,
                                 .route.sound_cursor = 59,
                                 .route.sound_position = {NAN, 0, 0}};
      assert(!bk_npc_event_audio_apply(events, &bad, 0, 1, listener, yaw,
                                       live_volume, error));
      rejected++;
    }
    CHECK(bk_audio_fill(audio, error));
  }
  /* Long enough to finish every one-shot naturally. */
  for (unsigned i = 0; i < 800; ++i) {
    sink.consumed = sink.submitted;
    CHECK(bk_audio_poll(audio, error));
    CHECK(bk_audio_fill(audio, error));
  }
  sink.consumed = sink.submitted;
  CHECK(bk_audio_poll(audio, error));
  for (unsigned i = 0; i < 5; ++i) {
    int playing;
    CHECK(bk_audio_playing(audio, i, &playing));
    assert(!playing);
  }
  assert(ais && waits && routes && replacements && rejected && sink.nonzero);
  printf("PASS NPC event audio ai=%u waits=%u route=%u same-frame-boundary=%u "
         "rejected=%u samples=%" PRIu64 " nonzero=%" PRIu64 " hash=%016" PRIx64
         "\n",
         ais, waits, routes, replacements, rejected, sink.samples, sink.nonzero,
         sink.hash);
  rc = 0;
done:
  if (rc)
    fprintf(stderr, "%s\n", error);
  bk_npc_event_audio_destroy(events);
  bk_system_audio_destroy(system);
  bk_area_audio_destroy(area);
  bk_player_audio_destroy(player);
  bk_audio_destroy(audio);
  for (unsigned i = 0; i < 8; ++i)
    bk_pcm_destroy(pcm[i]);
  bk_blob_free(&blob);
  bk_resources_destroy(store);
  return rc;
}
