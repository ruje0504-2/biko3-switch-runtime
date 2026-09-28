#include "scene/ending_audio.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define CHECK(x)                                                               \
  do {                                                                         \
    if (!(x))                                                                  \
      goto done;                                                               \
  } while (0)
enum { FIRST = 50 };
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
/* Reference consumes original DirectSound calls recorded by x86 execution.
 * It never invokes the portable ending controller or resource-name helper. */
static int reference_command(BkAudioClip *clips[6], BkResourceStore *store,
                             BkAudio *m, FILE *trace, char e[256]) {
  unsigned slot;
  int kind, loop, volume;
  char pack[32], name[64];
  if (fscanf(trace, "%d %u %31s %63s %d %d", &kind, &slot, pack, name, &loop,
             &volume) != 6 ||
      slot >= 6)
    return 0;
  switch (kind) {
  case 0:
    if (!bk_audio_clear(m, FIRST + slot, e))
      return 0;
    bk_audio_clip_release(clips[slot]);
    clips[slot] = NULL;
    return 1;
  case 1:
    assert(!clips[slot]);
    clips[slot] = bk_audio_clip_load(store, pack, name, e);
    return clips[slot] != NULL;
  case 2:
    assert(clips[slot]);
    return bk_audio_play(m, FIRST + slot, clips[slot], loop, volume, 0, e);
  case 3:
    assert(clips[slot]);
    return bk_audio_pause(m, FIRST + slot, e);
  }
  return 0;
}
int main(int argc, char **argv) {
  if (argc != 3)
    return 2;
  int rc = 1;
  char e[256] = {0}, path[1024];
  FILE *trace = fopen(argv[2], "r");
  BkResourceStore *store = bk_resources_create(e);
  BkAudio *mix[2] = {0};
  BkAudioClip *reference[6] = {0};
  BkEndingAudio *audio = NULL;
  Sink sink[2] = {{.hash = UINT64_C(14695981039346656037)},
                  {.hash = UINT64_C(14695981039346656037)}};
  unsigned calls = 0, commands = 0;
  CHECK(trace && store);
  for (unsigned i = 0; i < 2; i++) {
    const char *pack = i ? "bk3_06" : "bk3_02";
    snprintf(path, sizeof(path), "%s/%s.pp", argv[1], pack);
    CHECK(bk_resources_mount(store, pack, path, e));
    BkAudioSink output = {&sink[i], 48000, 480, 1920, submit, poll};
    mix[i] = bk_audio_create(&output, e);
    CHECK(mix[i]);
  }
  audio = bk_ending_audio_create(store, mix[0], FIRST, e);
  CHECK(audio);
  /* Native null buffers have valid stopped/no-op transport operations. */
  for (unsigned slot = 0; slot < 6; slot++) {
    int playing = 1;
    for (int operation = 0; operation < 3; operation++) {
      BkEndingAudioCall call = {.operation = operation, .slot = slot};
      CHECK(bk_ending_audio_call(audio, 0, 0, 0, &call, &playing, e));
      assert(!playing);
    }
  }
  while (1) {
    unsigned group, slot;
    int variant, selection, operation, cue, bank, flags, volume, count;
    int scanned = fscanf(trace, "%u %d %d %d %u %d %d %d %d %d", &group,
                         &variant, &selection, &operation, &slot, &cue, &bank,
                         &flags, &volume, &count);
    if (scanned == EOF)
      break;
    CHECK(scanned == 10 && count >= 0);
    for (unsigned i = 0; i < 2; i++) {
      uint64_t advance = (unsigned[]){0, 240, 480, 960, 1920, 3000}[calls % 6];
      uint64_t queued = sink[i].submitted - sink[i].consumed;
      sink[i].consumed += advance < queued ? advance : queued;
      CHECK(bk_audio_poll(mix[i], e));
      sink[i].count = 0;
    }
    BkEndingAudioCall call = {operation, slot, cue, bank, flags, volume};
    int playing;
    CHECK(bk_ending_audio_call(audio, group, variant, selection, &call,
                               &playing, e));
    for (int i = 0; i < count; i++) {
      CHECK(reference_command(reference, store, mix[1], trace, e));
      commands++;
    }
    for (unsigned v = 0; v < 6; v++) {
      int a, b;
      assert(bk_audio_playing(mix[0], FIRST + v, &a) &&
             bk_audio_playing(mix[1], FIRST + v, &b) && a == b);
      BkEndingAudioCall status = {.operation = BK_ENDING_AUDIO_STATUS,
                                  .slot = v};
      CHECK(bk_ending_audio_call(audio, group, variant, selection, &status,
                                 &playing, e));
      assert(playing == b);
      BkAudioCursor ca, cb;
      assert(bk_audio_cursor(mix[0], FIRST + v, &ca) &&
             bk_audio_cursor(mix[1], FIRST + v, &cb));
      assert(ca.source_frame == cb.source_frame && ca.playing == cb.playing &&
             ca.buffered == cb.buffered);
      assert((ca.pcm != NULL) == (cb.pcm != NULL));
      if (ca.pcm)
        assert(bk_pcm_frames(ca.pcm) == bk_pcm_frames(cb.pcm) &&
               bk_pcm_rate(ca.pcm) == bk_pcm_rate(cb.pcm));
    }
    for (unsigned i = 0; i < 2; i++)
      CHECK(bk_audio_fill(mix[i], e));
    assert(
        sink[0].count == sink[1].count &&
        !memcmp(sink[0].batch, sink[1].batch, sink[0].count * sizeof(int16_t)));
    assert(sink[0].submitted == sink[1].submitted &&
           sink[0].hash == sink[1].hash);
    calls++;
  }
  assert(calls == 1446 && commands == 3612 && sink[0].nonzero);
  CHECK(
      !bk_ending_audio_bind(audio, 0, "bk3_06", "missing-ending-voice.wav", e));
  int playing = 1;
  BkEndingAudioCall status = {.operation = BK_ENDING_AUDIO_STATUS};
  CHECK(bk_ending_audio_call(audio, 0, 0, 0, &status, &playing, e));
  assert(!playing); /* Failed replacement cleared the old sound. */
  CHECK(bk_ending_audio_stop(audio, e));
  for (unsigned v = 0; v < 6; v++)
    assert(bk_audio_playing(mix[0], FIRST + v, &playing) && !playing);
  printf("PASS ending audio: %u original calls/%u commands, %llu exact PCM "
         "samples, %llu nonzero, FNV%016llx; six cursors/status, null and "
         "missing buffers\n",
         calls, commands, (unsigned long long)sink[0].samples,
         (unsigned long long)sink[0].nonzero, (unsigned long long)sink[0].hash);
  rc = 0;
done:
  if (rc)
    fprintf(stderr, "call%u: %s\n", calls, e);
  if (trace)
    fclose(trace);
  if (audio) {
    char ignored[256];
    bk_ending_audio_stop(audio, ignored);
  }
  bk_ending_audio_destroy(audio);
  for (unsigned i = 0; i < 6; i++)
    bk_audio_clip_release(reference[i]);
  for (unsigned i = 0; i < 2; i++)
    bk_audio_destroy(mix[i]);
  bk_resources_destroy(store);
  return rc;
}
