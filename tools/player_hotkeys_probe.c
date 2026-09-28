/* Offline hotkeys integration: real slot0/5/7 PCM vs independent resampler.
 * Capture requests are recorded at this CPU boundary; screenshot-probe also
 * binds the same policy to the real GPU/files output. */
#include "scene/player_hotkeys.h"
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
  Voice voices[3];
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
    for (unsigned j = 0; j < 3; ++j) {
      Voice *v = &s->voices[j];
      if (!v->pcm)
        continue;
      size_t count = bk_pcm_frames(v->pcm), at = v->phase / 48000;
      if (at >= count)
        continue;
      const int16_t *source = bk_pcm_samples(v->pcm);
      size_t next = at + 1 < count ? at + 1 : at;
      unsigned channels = bk_pcm_channels(v->pcm);
      for (unsigned c = 0; c < 2; ++c) {
        unsigned channel = channels == 1 ? 0 : c;
        double value = source[at * channels + channel] +
                       (source[next * channels + channel] -
                        source[at * channels + channel]) *
                           ((double)(v->phase % 48000) / 48000.);
        sum[c] += (float)(value * v->gain[c]);
      }
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

typedef struct {
  unsigned captures, photo, album;
} Capture;
static int request(void *context, int photo, unsigned album, char error[256]) {
  (void)error;
  Capture *c = context;
  ++c->captures;
  c->photo = (unsigned)photo;
  c->album = album;
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
  BkSystemAudio *sounds[3] = {0};
  BkPcm *pcm[3] = {0};
  BkBlob raw = {0};
  Sink sink = {.hash = UINT64_C(14695981039346656037)};
  CHECK(store);
  snprintf(path, sizeof(path), "%s/bk3_02.pp", argv[1]);
  CHECK(bk_resources_mount(store, "bk3_02", path, error));
  BkAudioSink output = {&sink, 48000, 240, 960, submit, poll};
  audio = bk_audio_create(&output, error);
  CHECK(audio);
  const unsigned slots[] = {0, 5, 7};
  const char *names[] = {"se000.wav", "se005.wav", "se099.wav"};
  for (unsigned i = 0; i < 3; ++i) {
    sounds[i] =
        bk_system_audio_create_slot(store, audio, i, slots[i], -600, error);
    CHECK(sounds[i]);
    CHECK(bk_resources_read(store, "bk3_02", names[i], &raw, error) ==
          BK_RESOURCE_OK);
    pcm[i] = bk_pcm_decode(raw.data, raw.size, error);
    bk_blob_free(&raw);
    CHECK(pcm[i]);
    assert(bk_pcm_channels(pcm[i]) == 1 || bk_pcm_channels(pcm[i]) == 2);
  }
  Capture captures = {0};
  BkPlayerHotkeyServices services = {sounds[0], sounds[1], sounds[2], &captures,
                                     request};
  BkPlayerHotkeys state = {0};
  uint8_t camera = 0;
  unsigned expected_captures = 0, commands = 0;
  for (unsigned tick = 0; tick < 1400; ++tick) {
    sink.consumed = sink.submitted;
    CHECK(bk_audio_poll(audio, error));
    unsigned buttons = tick % 8, group = tick % 5, album = (group + 2) % 5;
    unsigned special = tick % 3;
    state.photo_count = (int)(tick % 105);
    int before = state.photo_count;
    if (buttons & 3) {
      play(&sink, 0, pcm[0], -600, 0);
      commands += (!!(buttons & 1) + !!(buttons & 2));
    }
    if (buttons & 1)
      ++expected_captures;
    if (special != 1 && (buttons & 4)) {
      unsigned slot = before < 100 ? 2 : 1;
      play(&sink, slot, pcm[slot], -600, 0);
      ++commands;
      if (before < 100)
        ++expected_captures;
    }
    CHECK(bk_scene_player_hotkeys(&services, &state, &camera, group, album,
                                  (uint8_t)special, buttons, error));
    assert(captures.captures == expected_captures);
    if ((buttons & 1) || (special != 1 && (buttons & 4) && before < 100)) {
      assert(captures.album == album);
      assert(captures.photo ==
             (unsigned)(special != 1 && (buttons & 4) && before < 100));
    }
    assert(state.photo_count ==
           before + (special != 1 && (buttons & 4) && before < 100));
    CHECK(bk_audio_fill(audio, error));
  }
  printf("PASS hotkeys audio commands=%u capture-requests=%u samples=%" PRIu64
         " nonzero=%" PRIu64 " hash=%016" PRIx64 "\n",
         commands, expected_captures, sink.samples, sink.nonzero, sink.hash);
  rc = 0;
done:
  if (rc)
    fprintf(stderr, "FAIL hotkeys audio: %s\n", error);
  for (unsigned i = 0; i < 3; ++i) {
    bk_system_audio_destroy(sounds[i]);
    bk_pcm_destroy(pcm[i]);
  }
  bk_audio_destroy(audio);
  bk_blob_free(&raw);
  bk_resources_destroy(store);
  return rc;
}
