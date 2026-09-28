/* Native-generated bank/gain trace with real PCM, delayed consumption and
 * actual shared auxiliary/effect slots. This is not a complete flow16 test. */
#include "scene/ending_audio.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#define CHECK(x)                                                               \
  do {                                                                         \
    if (!(x))                                                                  \
      goto done;                                                               \
  } while (0)
enum { FIRST = 8, N = BK_ENDING_SOUND_BUFFERS };
typedef struct {
  uint64_t submitted, consumed, samples, nonzero, hash;
  int16_t batch[8192];
  size_t count;
} Sink;
static int submit(void *p, const int16_t *pcm, size_t frames, char e[256]) {
  (void)e;
  Sink *s = p;
  assert(s->count + frames * 2 <= 8192);
  memcpy(s->batch + s->count, pcm, frames * 2 * sizeof(*pcm));
  s->count += frames * 2;
  for (size_t i = 0; i < frames * 2; i++) {
    uint16_t v = (uint16_t)pcm[i];
    s->nonzero += v != 0;
    s->hash = (s->hash ^ (v & 255)) * UINT64_C(1099511628211);
    s->hash = (s->hash ^ (v >> 8)) * UINT64_C(1099511628211);
  }
  s->samples += frames * 2;
  s->submitted += frames;
  return 1;
}
static int poll(void *p, uint64_t *out, char e[256]) {
  (void)e;
  *out = ((Sink *)p)->consumed;
  return 1;
}
static int advance(BkAudio *mix[2], Sink sink[2], unsigned step, char e[256]) {
  for (unsigned i = 0; i < 2; i++) {
    uint64_t n = (unsigned[]){0, 240, 480, 960, 1920}[step % 5];
    uint64_t queued = sink[i].submitted - sink[i].consumed;
    sink[i].consumed += n < queued ? n : queued;
    sink[i].count = 0;
    if (!bk_audio_poll(mix[i], e))
      return 0;
  }
  return 1;
}
static int compare(BkAudio *mix[2], Sink sink[2], char e[256]) {
  for (unsigned slot = 0; slot < BK_AUDIO_VOICES; slot++) {
    int a, b;
    assert(bk_audio_playing(mix[0], slot, &a) &&
           bk_audio_playing(mix[1], slot, &b) && a == b);
    BkAudioCursor ca, cb;
    assert(bk_audio_cursor(mix[0], slot, &ca) &&
           bk_audio_cursor(mix[1], slot, &cb));
    assert(ca.source_frame == cb.source_frame && ca.playing == cb.playing &&
           ca.buffered == cb.buffered &&
           ca.pending_change == cb.pending_change);
    assert((ca.pcm != NULL) == (cb.pcm != NULL));
    if (ca.pcm)
      assert(bk_pcm_frames(ca.pcm) == bk_pcm_frames(cb.pcm));
  }
  for (unsigned i = 0; i < 2; i++)
    if (!bk_audio_fill(mix[i], e))
      return 0;
  assert(
      sink[0].count == sink[1].count &&
      !memcmp(sink[0].batch, sink[1].batch, sink[0].count * sizeof(int16_t)));
  assert(sink[0].submitted == sink[1].submitted &&
         sink[0].hash == sink[1].hash);
  return 1;
}
static int command(BkEndingAudio *owner, BkAudio *reference,
                   BkAudioClip *clips[N], unsigned slot,
                   BkEndingAudioOperation op, int32_t volume, char e[256]) {
  BkEndingAudioCall call = {
      .operation = op, .slot = slot, .flags = 1, .volume = volume};
  int playing;
  if (!bk_ending_audio_call(owner, 0, 0, 0, &call, &playing, e))
    return 0;
  if (!clips[slot])
    return 1;
  if (op == BK_ENDING_AUDIO_RESTART)
    return bk_audio_play(reference, FIRST + slot, clips[slot], 1, volume, 0, e);
  assert(op == BK_ENDING_AUDIO_PAUSE);
  return bk_audio_pause(reference, FIRST + slot, e);
}
static int bind_voice(BkEndingAudio *owner, BkResourceStore *store,
                      BkAudio *reference, BkAudioClip *clips[N], unsigned slot,
                      const char *name, char e[256]) {
  if (!bk_ending_audio_bind(owner, slot, "bk3_06", name, e) ||
      !bk_audio_clear(reference, FIRST + slot, e))
    return 0;
  bk_audio_clip_release(clips[slot]);
  clips[slot] = bk_audio_clip_load(store, "bk3_06", name, e);
  return clips[slot] != NULL;
}
int main(int argc, char **argv) {
  if (argc != 4)
    return 2;
  int rc = 1;
  char e[256] = {0}, path[1024], label[32];
  FILE *trace = fopen(argv[2], "r"), *bad = NULL;
  BkResourceStore *store = bk_resources_create(e), *broken = NULL;
  BkAudio *mix[2] = {0};
  BkAudioClip *clips[N] = {0}, *guard = NULL;
  BkEndingAudio *owner = NULL;
  Sink sink[2] = {{.hash = UINT64_C(14695981039346656037)},
                  {.hash = UINT64_C(14695981039346656037)}};
  unsigned profiles = 0, frames = 0, aliases = 0, missing = 0, rejected = 0;
  CHECK(trace && store);
  for (unsigned i = 0; i < 2; i++) {
    const char *pack = i ? "bk3_06" : "bk3_02";
    snprintf(path, sizeof(path), "%s/%s.pp", argv[1], pack);
    CHECK(bk_resources_mount(store, pack, path, e));
    BkAudioSink output = {&sink[i], 48000, 480, 1920, submit, poll};
    mix[i] = bk_audio_create(&output, e);
    CHECK(mix[i]);
  }
  guard = bk_audio_clip_load(store, "bk3_02", "se002.wav", e);
  CHECK(guard);
  for (unsigned i = 0; i < 2; i++)
    for (unsigned j = 0; j < 2; j++)
      CHECK(bk_audio_play(mix[i], j ? 63 : 0, guard, 1, -4000, 0, e));
  while (fscanf(trace, "%31s", label) == 1) {
    unsigned group, variant;
    int music_volume;
    CHECK(!strcmp(label, "ENTRY") &&
          fscanf(trace, "%u %u %d", &group, &variant, &music_volume) == 3);
    owner = bk_ending_audio_create_entry(store, mix[0], FIRST, group, variant,
                                         music_volume, e);
    CHECK(owner);
    for (unsigned i = 0; i < 46; i++) {
      unsigned slot;
      char name[64];
      CHECK(fscanf(trace, "%31s %u %63s", label, &slot, name) == 3 &&
            !strcmp(label, "LOAD") && slot < N);
      if (strcmp(name, "-")) {
        clips[slot] = bk_audio_clip_load(store, "bk3_02", name, e);
        CHECK(clips[slot]);
      } else
        missing++;
      int present = -1;
      CHECK(bk_ending_audio_present(owner, slot, &present));
      assert(present == (clips[slot] != NULL));
    }
    CHECK(bk_audio_play(mix[1], FIRST + BK_ENDING_SOUND_MUSIC,
                        clips[BK_ENDING_SOUND_MUSIC], 1, music_volume, 0, e));
    int32_t latch = 73;
    for (unsigned t = 0; t < 360; t++) {
      int event, direction, master, expected_latch, expected_result, writes,
          written, expected_gain;
      float dt;
      CHECK(fscanf(trace, "%31s %d %d %d %a %d %d %d %d %d", label, &event,
                   &direction, &master, &dt, &expected_latch, &expected_result,
                   &writes, &written, &expected_gain) == 10 &&
            !strcmp(label, "FRAME"));
      CHECK(advance(mix, sink, frames, e));
      if (event == 1 || event == 2) {
        unsigned slot = event == 1 ? 0 : 1;
        CHECK(bind_voice(owner, store, mix[1], clips, slot,
                         event == 1 ? "PH12105.wav" : "PH11201.wav", e));
        CHECK(command(owner, mix[1], clips, slot, BK_ENDING_AUDIO_RESTART,
                      event == 1 ? -500 : -1000, e));
        /* Gain getter must preserve nonzero pan when ducking. */
        if (!slot)
          for (unsigned i = 0; i < 2; i++)
            CHECK(bk_audio_gain(mix[i], FIRST, -500, 1200, e));
      } else if (event == 3 || event == 4)
        CHECK(command(owner, mix[1], clips, event == 3 ? 1 : 0,
                      BK_ENDING_AUDIO_PAUSE, 0, e));
      unsigned effect = 2 + t / 8;
      if (t % 8 == 0 || t % 8 == 4)
        CHECK(command(owner, mix[1], clips, effect,
                      t % 8 ? BK_ENDING_AUDIO_PAUSE : BK_ENDING_AUDIO_RESTART,
                      -2300, e));
      if (t < 32 && t % 8 == 2) {
        CHECK(
            bind_voice(owner, store, mix[1], clips, effect, "PH11201.wav", e));
        CHECK(command(owner, mix[1], clips, effect, BK_ENDING_AUDIO_RESTART,
                      -2300, e));
        aliases++;
      }
      int32_t result = -1;
      CHECK(bk_ending_audio_duck(owner, &latch, direction, master, dt, &result,
                                 e));
      assert(latch == expected_latch && result == expected_result);
      if (writes && written >= -10000 && written <= 0)
        CHECK(bk_audio_gain(mix[1], FIRST, written, 1200, e));
      else if (writes)
        rejected++;
      for (unsigned i = 0; i < 2; i++) {
        int32_t volume, pan;
        CHECK(bk_audio_get_gain(mix[i], FIRST, &volume, &pan));
        assert(volume == expected_gain && pan == 1200);
        CHECK(bk_audio_get_gain(mix[i], FIRST + BK_ENDING_SOUND_MUSIC, &volume,
                                &pan));
        assert(volume == music_volume && pan == 0);
      }
      CHECK(compare(mix, sink, e));
      frames++;
    }
    CHECK(bk_ending_audio_stop(owner, e));
    bk_ending_audio_destroy(owner);
    owner = NULL;
    for (unsigned i = 0; i < N; i++) {
      CHECK(bk_audio_clear(mix[1], FIRST + i, e));
      bk_audio_clip_release(clips[i]);
      clips[i] = NULL;
    }
    /* Already queued PCM survives destruction; unrelated mixer slots survive.
     */
    CHECK(advance(mix, sink, 3, e) && compare(mix, sink, e));
    for (unsigned i = 0; i < 2; i++) {
      int playing;
      CHECK(bk_audio_playing(mix[i], 0, &playing) && playing);
      CHECK(bk_audio_playing(mix[i], 63, &playing) && playing);
    }
    profiles++;
  }
  assert(profiles == 10 && frames == 3600 && aliases == 40 && missing == 40 &&
         rejected && sink[0].nonzero);
  CHECK(!bk_ending_audio_create_entry(store, mix[0], 17, 0, 0, -500, e));
  CHECK(!bk_ending_audio_create_entry(store, mix[0], FIRST, 5, 0, -500, e));
  CHECK(!bk_ending_audio_create_entry(store, mix[0], FIRST, 0, 2, -500, e));
  broken = bk_resources_create(e);
  CHECK(broken);
  CHECK(!bk_ending_audio_create_entry(broken, mix[0], FIRST, 0, 0, -500, e));
  snprintf(path, sizeof(path), "%s/bk3_02.pp", argv[1]);
  CHECK(bk_resources_mount(broken, "bk3_02", path, e));
  CHECK(!mkdir(argv[3], 0700) || errno == EEXIST);
  snprintf(path, sizeof(path), "%s/se301.wav", argv[3]);
  bad = fopen(path, "wb");
  CHECK(bad && fwrite("corrupt-wave", 1, 12, bad) == 12);
  CHECK(!fclose(bad));
  bad = NULL;
  CHECK(bk_resources_mount_directory(broken, "bk3_02", argv[3], 1024, e));
  for (unsigned g = 0; g < 5; g++) {
    CHECK(!bk_ending_audio_create_entry(broken, mix[0], FIRST, g, 0, -500, e));
    for (unsigned s = FIRST; s < FIRST + N; s++) {
      int playing;
      CHECK(bk_audio_playing(mix[0], s, &playing) && !playing);
    }
    assert(!bk_audio_stats(mix[0]).failed);
  }
  printf("PASS ending sound: %u profiles/%u native duck frames, %llu exact PCM "
         "samples, %llu nonzero, FNV%016llx; %u alias replacements/%u absent "
         "effects/%u rejected gains, 64 cursor/status slots, ten retirements, "
         "missing music/five late corrupt-effect failures\n",
         profiles, frames, (unsigned long long)sink[0].samples,
         (unsigned long long)sink[0].nonzero, (unsigned long long)sink[0].hash,
         aliases, missing, rejected);
  rc = 0;
done:
  if (rc)
    fprintf(stderr, "FAIL ending sound: %s\n", e);
  if (trace)
    fclose(trace);
  if (bad)
    fclose(bad);
  bk_ending_audio_destroy(owner);
  for (unsigned i = 0; i < N; i++)
    bk_audio_clip_release(clips[i]);
  bk_audio_clip_release(guard);
  for (unsigned i = 0; i < 2; i++)
    bk_audio_destroy(mix[i]);
  bk_resources_destroy(broken);
  bk_resources_destroy(store);
  return rc;
}
