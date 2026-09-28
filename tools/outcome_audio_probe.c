/* Actual se100/se007 direct Play behavior: independent continuous sample
 * reference detects accidental restarts on repeated native calls. Offline. */
#include "scene/outcome_audio.h"
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
  Voice voices[2];
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
    for (unsigned j = 0; j < 2; ++j) {
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
  int rc = 1;
  char error[256] = {0}, path[1024];
  BkResourceStore *store = bk_resources_create(error);
  BkAudio *audio = NULL;
  BkOutcomeAudio *events = NULL;
  BkPcm *pcm[2] = {0};
  BkBlob raw = {0};
  Sink sink = {.hash = UINT64_C(14695981039346656037)};
  unsigned calls = 0, restarts = 0;
  CHECK(store);
  CHECK(snprintf(path, sizeof(path), "%s/bk3_02.pp", argv[1]) <
        (int)sizeof(path));
  CHECK(bk_resources_mount(store, "bk3_02", path, error));
  const char *names[] = {"se100.wav", "se007.wav"};
  for (unsigned i = 0; i < 2; ++i) {
    CHECK(bk_resources_read(store, "bk3_02", names[i], &raw, error) ==
          BK_RESOURCE_OK);
    pcm[i] = bk_pcm_decode(raw.data, raw.size, error);
    bk_blob_free(&raw);
    CHECK(pcm[i]);
    assert(bk_pcm_rate(pcm[i]) == 22050 && bk_pcm_channels(pcm[i]) == 1);
  }
  BkAudioSink output = {&sink, 48000, 240, 960, submit, poll};
  audio = bk_audio_create(&output, error);
  CHECK(audio);
  events = bk_outcome_audio_create(store, audio, 0, 1, -900, error);
  CHECK(events);
  assert(!bk_outcome_audio_create(store, audio, 0, 0, -900, error));
  for (unsigned tick = 0; tick < 1400; ++tick) {
    sink.consumed = sink.submitted;
    CHECK(bk_audio_poll(audio, error));
    if (tick == 450 || tick == 1000) {
      CHECK(bk_outcome_audio_stop(events, error));
      memset(sink.voices, 0, sizeof(sink.voices));
    } else if (tick % 200 < 175) {
      for (unsigned i = 0; i < 2; ++i) {
        if (i == 1 && tick % 3)
          continue;
        if (!sink.voices[i].pcm ||
            sink.voices[i].phase / 48000 >= bk_pcm_frames(pcm[i])) {
          play(&sink, i, pcm[i], -900, 0);
          restarts++;
        }
        CHECK(i ? bk_outcome_audio_play_response(events, error)
                : bk_outcome_audio_play_outcome(events, error));
        calls++;
      }
    }
    CHECK(bk_audio_fill(audio, error));
  }
  CHECK(bk_outcome_audio_stop(events, error));
  bk_outcome_audio_destroy(events);
  events = NULL;
  memset(sink.voices, 0, sizeof(sink.voices));
  sink.consumed = sink.submitted;
  CHECK(bk_audio_poll(audio, error));
  CHECK(bk_audio_fill(audio, error));
  printf("PASS outcome audio calls=%u restarts=%u samples=%" PRIu64
         " nonzero=%" PRIu64 " hash=%016" PRIx64 "\n",
         calls, restarts, sink.samples, sink.nonzero, sink.hash);
  rc = 0;
done:
  if (rc)
    fprintf(stderr, "FAIL outcome audio: %s\n", error);
  bk_outcome_audio_destroy(events);
  bk_audio_destroy(audio);
  for (unsigned i = 0; i < 2; ++i)
    bk_pcm_destroy(pcm[i]);
  bk_blob_free(&raw);
  bk_resources_destroy(store);
  return rc;
}
