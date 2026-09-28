#ifdef NDEBUG
#undef NDEBUG
#endif
#include "media/audio.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
typedef struct {
  int16_t samples[131072];
  uint64_t submitted, consumed;
  unsigned calls, fail_submit;
  int fail_poll;
} Sink;
static int submit(void *ctx, const int16_t *in, size_t frames, char *error) {
  Sink *s = ctx;
  if (++s->calls == s->fail_submit) {
    snprintf(error, 256, "injected submit failure");
    return 0;
  }
  assert(s->submitted + frames <= 65536);
  memcpy(s->samples + s->submitted * 2, in, frames * 4);
  s->submitted += frames;
  return 1;
}
static int poll(void *ctx, uint64_t *consumed, char *error) {
  Sink *s = ctx;
  if (s->fail_poll) {
    snprintf(error, 256, "injected poll failure");
    return 0;
  }
  *consumed = s->consumed;
  return 1;
}
static BkAudio *create(Sink *s, char *error) {
  BkAudioSink sink = {s, 48000, 8, 32, submit, poll};
  BkAudio *audio = bk_audio_create(&sink, error);
  assert(audio);
  return audio;
}
static void u32(uint8_t *p, unsigned n) {
  for (unsigned i = 0; i < 4; i++)
    p[i] = (uint8_t)(n >> (8 * i));
}
static BkAudioClip *clip(int sign, char *error) {
  uint8_t wave[44 + 128 * 2] = {0};
  memcpy(wave, "RIFF", 4);
  u32(wave + 4, sizeof(wave) - 8);
  memcpy(wave + 8, "WAVEfmt ", 8);
  u32(wave + 16, 16);
  wave[20] = wave[22] = 1;
  u32(wave + 24, 48000);
  u32(wave + 28, 96000);
  wave[32] = 2;
  wave[34] = 16;
  memcpy(wave + 36, "data", 4);
  u32(wave + 40, 256);
  for (unsigned i = 0; i < 128; i++) {
    uint16_t v = (uint16_t)(sign * (1000 + (int)i));
    wave[44 + i * 2] = (uint8_t)v;
    wave[45 + i * 2] = (uint8_t)(v >> 8);
  }
  BkAudioClip *c = bk_audio_clip_decode(wave, sizeof(wave), error);
  assert(c);
  return c;
}
static void epochs(void) {
  char error[256];
  Sink s = {0};
  BkAudio *a = create(&s, error);
  BkAudioClip *c = clip(1, error);
  assert(bk_audio_play(a, 0, c, 1, 0, 0, error));
  bk_audio_clip_release(c); /* engine must retain PCM independently */
  assert(bk_audio_fill(a, error));
  assert(s.submitted == 32);
  for (unsigned i = 0; i < 32; i++)
    assert(s.samples[i * 2] == 1000 + (int)i &&
           s.samples[i * 2 + 1] == 1000 + (int)i);
  s.consumed = 5;
  assert(bk_audio_poll(a, error));
  c = clip(-1, error);
  assert(bk_audio_play(a, 0, c, 1, 0, 0, error));
  assert(bk_audio_gain(a, 0, 0, 10000, error));
  int32_t volume = 71, pan = 72;
  assert(bk_audio_get_gain(a, 0, &volume, &pan) && volume == 0 && pan == 10000);
  bk_audio_clip_release(c);
  BkAudioCursor cursor;
  assert(bk_audio_cursor(a, 0, &cursor));
  assert(cursor.playing && cursor.buffered && cursor.pending_change &&
         cursor.source_frame == 5 && bk_pcm_samples(cursor.pcm)[5] == 1005);
  s.consumed = 16;
  assert(bk_audio_poll(a, error) && bk_audio_fill(a, error));
  assert(s.submitted == 48);
  for (unsigned i = 32; i < 48; i++)
    assert(s.samples[i * 2] == 0 &&
           s.samples[i * 2 + 1] == -1000 - (int)(i - 32));
  assert(bk_audio_gain(a, 0, 0, -10000, error));
  s.consumed = 32;
  assert(bk_audio_poll(a, error));
  assert(bk_audio_cursor(a, 0, &cursor));
  assert(cursor.source_frame == 0 && cursor.pending_change &&
         bk_pcm_samples(cursor.pcm)[0] == -1000);
  assert(bk_audio_fill(a, error));
  assert(s.submitted == 64);
  for (unsigned i = 48; i < 64; i++)
    assert(s.samples[i * 2] == -1000 - (int)(i - 32) &&
           s.samples[i * 2 + 1] == 0); /* gain does not restart */
  int playing = -1;
  assert(bk_audio_playing(a, 0, &playing) && playing);
  assert(bk_audio_clear(a, 0, error));
  volume = 71;
  pan = 72;
  assert(!bk_audio_get_gain(a, 0, &volume, &pan) && volume == 71 && pan == 72);
  assert(bk_audio_playing(a, 0, &playing) && !playing);
  s.consumed = 48;
  assert(bk_audio_poll(a, error) && bk_audio_fill(a, error));
  assert(s.submitted == 80);
  assert(bk_audio_cursor(a, 0, &cursor));
  assert(cursor.playing && cursor.pending_change && cursor.source_frame == 16);
  assert(bk_audio_playing(a, 0, &playing) && !playing);
  for (unsigned i = 64; i < 80; i++)
    assert(s.samples[i * 2] == 0 && s.samples[i * 2 + 1] == 0);
  s.consumed = 80;
  assert(bk_audio_poll(a, error));
  assert(bk_audio_cursor(a, 0, &cursor) && !cursor.pcm && !cursor.playing);
  assert(bk_audio_stats(a).queue_drains == 1);
  c = clip(1, error);
  assert(bk_audio_play(a, 0, c, 1, 0, 0, error));
  assert(bk_audio_cursor(a, 0, &cursor)); /* no extra poll required */
  assert(cursor.playing && !cursor.buffered && !cursor.pending_change &&
         cursor.source_frame == 0);
  bk_audio_clip_release(c);
  assert(bk_audio_fill(a, error));
  assert(s.samples[160] == 1000);
  bk_audio_destroy(a);
}
static void lifetime_and_end(void) {
  char error[256];
  Sink s = {0};
  BkAudio *a = create(&s, error);
  BkAudioClip *c = clip(1, error);
  for (unsigned i = 0; i < 500; i++) {
    assert(bk_audio_play(a, 0, c, 1, 0, 0, error));
    assert(bk_audio_clear(a, 0, error));
  }
  assert(bk_audio_play(a, 0, c, 0, 0, 0, error));
  bk_audio_clip_release(c);
  for (unsigned i = 0; i < 5; i++) {
    assert(bk_audio_fill(a, error));
    s.consumed = s.submitted;
    assert(bk_audio_poll(a, error));
  }
  BkAudioCursor cursor;
  assert(bk_audio_cursor(a, 0, &cursor));
  assert(cursor.pcm && !cursor.playing && cursor.source_frame == 128 &&
         !cursor.buffered);
  for (unsigned i = 0; i < 160; i++)
    assert(s.samples[2 * i] == (i < 128 ? 1000 + (int)i : 0));
  bk_audio_destroy(a);
  s = (Sink){0};
  a = create(&s, error);
  c = clip(1, error);
  for (unsigned i = 0; i < BK_AUDIO_VOICES; i++)
    assert(bk_audio_play(a, i, c, 1, 0, 0, error));
  bk_audio_clip_release(c);
  assert(bk_audio_fill(a, error));
  /*64 simultaneous1000-valued sources saturate from the first frame. This
   *also detects accidentally retaining the old32-voice mixing loop. */
  assert(s.samples[0] == 32767 && s.samples[62] == 32767);
  for (unsigned i = 0; i < 1000; i++) {
    s.consumed += 8;
    assert(bk_audio_poll(a, error));
    for (unsigned j = 0; j < BK_AUDIO_VOICES; j++)
      assert(bk_audio_gain(a, j, (i % 2) ? 0 : -10000, 0, error));
    assert(bk_audio_fill(a, error));
  }
  bk_audio_destroy(a);
}
static void failures(void) {
  char error[256];
  Sink s = {.fail_submit = 2};
  BkAudio *a = create(&s, error);
  assert(!bk_audio_fill(a, error));
  assert(bk_audio_stats(a).failed && bk_audio_stats(a).submitted == 8);
  assert(!bk_audio_fill(a, error) && !bk_audio_poll(a, error));
  assert(!bk_audio_clear(a, 0, error));
  bk_audio_destroy(a);
  for (unsigned n = 0; n < 3; n++) {
    s = (Sink){0};
    a = create(&s, error);
    assert(bk_audio_fill(a, error));
    s.consumed = 8;
    assert(bk_audio_poll(a, error));
    if (n == 0)
      s.consumed = 7;
    else if (n == 1)
      s.consumed = 33;
    else
      s.fail_poll = 1;
    assert(!bk_audio_poll(a, error) && bk_audio_stats(a).failed);
    bk_audio_destroy(a);
  }
  s = (Sink){0};
  a = create(&s, error);
  BkAudioClip *c = clip(1, error);
  assert(!bk_audio_play(a, BK_AUDIO_VOICES, c, 0, 0, 0, error));
  assert(!bk_audio_play(a, 0, c, 2, 0, 0, error));
  assert(!bk_audio_play(a, 0, c, 0, 1, 0, error));
  assert(!bk_audio_play(a, 0, c, 0, 0, -10001, error));
  assert(!bk_audio_gain(a, 0, 0, 0, error));
  assert(!bk_audio_clip_decode("invalid", 7, error));
  assert(!bk_audio_stats(a).failed);
  assert(bk_audio_fill(a, error));
  for (unsigned i = 0; i < 64; i++)
    assert(!s.samples[i]);
  bk_audio_clip_release(c);
  bk_audio_destroy(a);
  assert(!bk_audio_create(NULL, error));
}
static void frequencies(void) {
  /* Independent integer reference: changes occur at unsubmitted boundaries,
   * while cursor checks deliberately lag by several output blocks. */
  for (int loop = 0; loop <= 1; ++loop) {
    char error[256];
    Sink s = {0};
    BkAudio *a = create(&s, error);
    BkAudioClip *c = clip(1, error);
    assert(bk_audio_clip_rate(c) == 48000);
    assert(!bk_audio_frequency(a, 0, 10000, error));
    assert(bk_audio_play(a, 0, c, loop, 0, 0, error));
    bk_audio_clip_release(c);
    uint64_t phase = 0;
    const uint64_t end = 128 * 48000;
    size_t positions[8193] = {0};
    const uint32_t frequencies[] = {48000, 100, 10000, 44100, 100000, 22090, 0};
    for (unsigned step = 0; step < 1000; ++step) {
      if (step)
        s.consumed += 8;
      assert(bk_audio_poll(a, error));
      uint64_t before = s.submitted;
      uint32_t hz = frequencies[step % 7];
      assert(bk_audio_frequency(a, 0, 32000, error));
      assert(bk_audio_gain(a, 0, 0, 0, error));
      assert(bk_audio_frequency(a, 0, hz, error));
      assert(!bk_audio_frequency(a, 0, 99, error));
      assert(!bk_audio_frequency(a, 0, 100001, error));
      if (!hz)
        hz = 48000;
      assert(bk_audio_fill(a, error));
      for (uint64_t i = before; i < s.submitted; ++i) {
        size_t at = (size_t)(phase / 48000);
        positions[i] = at;
        int expected = 0;
        if (at < 128) {
          size_t next = at + 1 < 128 ? at + 1 : loop ? 0 : at;
          double value =
              1000. + at + ((double)next - at) * (phase % 48000) / 48000.;
          expected = (int)lround(value);
          phase += hz;
          if (loop)
            phase %= end;
          else if (phase > end)
            phase = end;
        }
        assert(s.samples[2 * i] == expected &&
               s.samples[2 * i + 1] == expected);
      }
      BkAudioCursor cursor;
      assert(bk_audio_cursor(a, 0, &cursor));
      assert(cursor.source_frame == positions[s.consumed]);
      assert(cursor.playing == (positions[s.consumed] < 128));
      int playing = -1;
      assert(bk_audio_playing(a, 0, &playing) && playing == cursor.playing);
    }
    bk_audio_destroy(a);
  }
}
static void transport(void) {
  char error[256];
  Sink s = {0};
  BkAudio *a = create(&s, error);
  BkAudioClip *c = clip(1, error);
  assert(!bk_audio_pause(a, 0, error));
  assert(!bk_audio_resume(a, 0, 1, error));
  assert(bk_audio_play(a, 0, c, 1, 0, 0, error));
  bk_audio_clip_release(c);
  /* Independent rational timeline with commands at the producer boundary;
   * stored history models the consumer separately. Exercise fractional phase,
   * loop changes, end/restart, retained PCM and multiple coalesced commands. */
  uint64_t phase = 0;
  const uint64_t end = 128 * 48000;
  uint32_t hz = 22090;
  int paused = 0, loop = 1, pan = 0;
  size_t positions[16384] = {0};
  int statuses[16384] = {0};
  for (unsigned step = 0; step < 1600; ++step) {
    if (step)
      s.consumed += 8;
    assert(bk_audio_poll(a, error));
    unsigned command = step % 19;
    uint64_t before = s.submitted;
    if (command == 2 || command == 3 || command == 11) {
      assert(bk_audio_pause(a, 0, error));
      paused = 1;
    } else if (command == 6 || command == 7 || command == 14) {
      loop = (step / 19) % 2;
      assert(bk_audio_resume(a, 0, loop, error));
      if (phase == end)
        phase = 0;
      paused = 0;
    } else if (command == 17) {
      /* Stop and resume at the same boundary must not introduce a gap. */
      assert(bk_audio_pause(a, 0, error));
      assert(bk_audio_resume(a, 0, loop, error));
      if (phase == end)
        phase = 0;
      paused = 0;
    }
    if (command == 0 || command == 4 || command == 12) {
      const uint32_t rates[] = {22090, 100, 44100, 100000};
      hz = rates[(step / 19) % 4];
      assert(bk_audio_frequency(a, 0, hz, error));
      pan = (step / 19) % 3 == 0 ? -10000 : (step / 19) % 3 == 1 ? 0 : 10000;
      assert(bk_audio_gain(a, 0, 0, pan, error));
    }
    assert(!bk_audio_resume(a, 0, 2, error));
    assert(!bk_audio_pause(a, BK_AUDIO_VOICES, error));
    int playing = -1;
    assert(bk_audio_playing(a, 0, &playing));
    if (paused)
      assert(!playing);
    else if (command == 6 || command == 7 || command == 14 || command == 17)
      assert(playing);
    assert(bk_audio_fill(a, error));
    for (uint64_t i = before; i < s.submitted; ++i) {
      size_t at = (size_t)(phase / 48000);
      positions[i] = at;
      statuses[i] = !paused && at < 128;
      int expected = 0;
      if (statuses[i]) {
        size_t next = at + 1 < 128 ? at + 1 : loop ? 0 : at;
        expected = (int)lround(1000. + at +
                               ((double)next - at) * (phase % 48000) / 48000.);
        phase += hz;
        if (loop)
          phase %= end;
        else if (phase > end)
          phase = end;
      }
      assert(s.samples[2 * i] == (pan == 10000 ? 0 : expected));
      assert(s.samples[2 * i + 1] == (pan == -10000 ? 0 : expected));
    }
    BkAudioCursor cursor;
    assert(bk_audio_cursor(a, 0, &cursor));
    assert(cursor.pcm && cursor.source_frame == positions[s.consumed]);
    assert(cursor.playing == statuses[s.consumed]);
  }
  assert(bk_audio_clear(a, 0, error));
  assert(!bk_audio_pause(a, 0, error) && !bk_audio_resume(a, 0, 0, error));
  assert(bk_audio_play(a, 0, c = clip(-1, error), 0, 0, 0, error));
  bk_audio_clip_release(c);
  assert(bk_audio_pause(a, 0, error));
  int32_t volume, queried_pan;
  assert(bk_audio_get_gain(a, 0, &volume, &queried_pan) && volume == 0 &&
         queried_pan == 0);
  s.consumed = s.submitted;
  assert(bk_audio_poll(a, error) && bk_audio_fill(a, error));
  BkAudioCursor cursor;
  assert(bk_audio_cursor(a, 0, &cursor) && !cursor.playing &&
         !cursor.source_frame);
  assert(bk_audio_resume(a, 0, 0, error));
  uint64_t from = s.submitted;
  s.consumed = from;
  assert(bk_audio_poll(a, error) && bk_audio_fill(a, error));
  assert(s.samples[from * 2] == -1000);
  bk_audio_destroy(a);
}
static void cursor_lifetime(void) {
  char error[256];
  Sink sink = {0};
  BkAudio *audio = create(&sink, error);
  BkAudioClip *old = clip(1, error);
  assert(bk_audio_play(audio, 0, old, 1, 0, 0, error));
  bk_audio_clip_release(old);
  assert(bk_audio_fill(audio, error));
  BkAudioCursor cursor;
  assert(bk_audio_cursor(audio, 0, &cursor) && cursor.pcm);
  BkAudioClip *next = clip(-1, error);
  assert(bk_audio_play(audio, 0, next, 1, 0, 0, error));
  bk_audio_clip_release(next);
  sink.consumed = sink.submitted;
  assert(bk_audio_poll(audio, error) && bk_audio_fill(audio, error));
  /* Old epoch has been collected, but the query's PCM remains readable. */
  assert(bk_pcm_samples(cursor.pcm)[5] == 1005);
  assert(bk_audio_clear(audio, 0, error));
  sink.consumed = sink.submitted;
  assert(bk_audio_poll(audio, error));
  assert(bk_pcm_samples(cursor.pcm)[5] == 1005);
  assert(bk_audio_cursor(audio, 0, &cursor) && !cursor.pcm);
  bk_audio_destroy(audio);
}
int main(void) {
  cursor_lifetime();
  epochs();
  frequencies();
  transport();
  lifetime_and_end();
  failures();
  puts(
      "PASS audio queue epochs, ownership, played cursor, mixing and failures");
}
