#ifdef NDEBUG
#undef NDEBUG
#endif
#include "platform/audio_output.h"
#include <assert.h>
#include <errno.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <switch.h>
#include <time.h>
enum {
  INIT = 1,
  START,
  APPEND,
  RELEASE,
  STOP,
  ALLOCATION,
  FORMAT,
  THREAD_CREATE,
  THREAD_START
};
static struct {
  AudioOutBuffer *queue[4];
  int16_t copies[4][960];
  unsigned count, release_count, initialized, started, stopped, exited;
  int failure, invalid_release, automatic, constant_pcm;
  double next_release;
  uint64_t played, flushes;
  void *flushed;
} native;
static double seconds(void) {
  struct timespec t;
  assert(clock_gettime(CLOCK_MONOTONIC, &t) == 0);
  return t.tv_sec + t.tv_nsec * 1e-9;
}
void mutexInit(Mutex *m) { assert(!pthread_mutex_init(m, NULL)); }
void mutexLock(Mutex *m) { assert(!pthread_mutex_lock(m)); }
void mutexUnlock(Mutex *m) { assert(!pthread_mutex_unlock(m)); }
static void *entry(void *p) {
  Thread *t = p;
  t->entry(t->context);
  return NULL;
}
Result threadCreate(Thread *t, void (*fn)(void *), void *arg, void *stack,
                    size_t bytes, int priority, int core) {
  assert(!stack && bytes == 0x10000 && priority == 0x2b && core == -2);
  if (native.failure == THREAD_CREATE)
    return 1;
  t->entry = fn;
  t->context = arg;
  return 0;
}
Result threadStart(Thread *t) {
  if (native.failure == THREAD_START)
    return 1;
  return pthread_create(&t->native, NULL, entry, t);
}
Result threadWaitForExit(Thread *t) { return pthread_join(t->native, NULL); }
Result threadClose(Thread *t) {
  (void)t;
  return 0;
}
void svcSleepThread(int64_t ns) {
  struct timespec t = {ns / 1000000000, ns % 1000000000};
  while (nanosleep(&t, &t) && errno == EINTR) {
  }
}
void armDCacheFlush(void *p, size_t bytes) {
  assert(bytes == 4096 && !((uintptr_t)p & 4095));
  native.flushed = p;
  native.flushes++;
}
void *memalign(size_t alignment, size_t size) {
  return native.failure == ALLOCATION ? NULL : aligned_alloc(alignment, size);
}
Result audoutInitialize(void) {
  native.initialized++;
  return native.failure == INIT;
}
void audoutExit(void) {
  /* Ownership check: queued memory must still exist until service close. */
  for (unsigned i = 0; i < native.count; i++)
    assert(!memcmp(native.queue[i]->buffer, native.copies[i], 1920));
  native.count = 0;
  native.exited++;
}
u32 audoutGetSampleRate(void) {
  return native.failure == FORMAT ? 44100 : 48000;
}
u32 audoutGetChannelCount(void) { return 2; }
PcmFormat audoutGetPcmFormat(void) { return PcmFormat_Int16; }
Result audoutStartAudioOut(void) {
  native.started++;
  return native.failure == START;
}
Result audoutStopAudioOut(void) {
  native.stopped++;
  return native.failure == STOP;
}
Result audoutAppendAudioOutBuffer(AudioOutBuffer *buffer) {
  assert(native.started && native.count < 4);
  assert(native.flushed == buffer->buffer);
  native.flushed = NULL;
  if (native.automatic && !native.count)
    native.next_release = seconds() + .01;
  assert(!((uintptr_t)buffer->buffer & 4095) && buffer->buffer_size == 4096 &&
         buffer->data_size == 1920 && !buffer->next && !buffer->data_offset);
  if (native.failure == APPEND)
    return 1;
  for (unsigned i = 0; i < native.count; i++)
    assert(native.queue[i] != buffer);
  if (native.constant_pcm) {
    const int16_t *p = buffer->buffer;
    for (unsigned i = 0; i < 960; ++i)
      assert(p[i] == 1234);
  }
  native.queue[native.count] = buffer;
  memcpy(native.copies[native.count++], buffer->buffer, 1920);
  return 0;
}
Result audoutGetReleasedAudioOutBuffer(AudioOutBuffer **out, u32 *count) {
  if (native.failure == RELEASE)
    return 1;
  *count = 0;
  if (native.automatic) {
    native.release_count = native.count && seconds() >= native.next_release;
    if (native.release_count)
      native.next_release += .01;
  }
  if (!native.count || !native.release_count)
    return 0;
  *count = 1;
  *out = native.invalid_release ? (AudioOutBuffer *)&native : native.queue[0];
  assert(!memcmp(native.queue[0]->buffer, native.copies[0], 1920));
  native.count--;
  native.played += 480;
  native.release_count--;
  memmove(native.queue, native.queue + 1, native.count * sizeof(*native.queue));
  memmove(native.copies, native.copies + 1,
          native.count * sizeof(*native.copies));
  return 0;
}
static void normal(void) {
  char error[256];
  BkAudioSink sink;
  memset(&native, 0, sizeof(native));
  BkAudioOutput *output = bk_audio_output_open(&sink, NULL, error);
  assert(output && sink.rate == 48000 && sink.block_frames == 480 &&
         sink.capacity_frames == 1920);
  assert(!bk_audio_output_open(&sink, NULL, error));
  BkAudio *audio = bk_audio_create(&sink, error);
  assert(audio && bk_audio_fill(audio, error));
  assert(native.count == 4);
  for (unsigned i = 0; i < 100; i++) {
    unsigned n = i % 4 + 1;
    native.release_count = n;
    uint64_t before = bk_audio_stats(audio).consumed;
    assert(bk_audio_poll(audio, error));
    assert(bk_audio_stats(audio).consumed == before + 480 * n);
    assert(native.count == 4 - n);
    assert(bk_audio_fill(audio, error) && native.count == 4);
  }
  bk_audio_destroy(audio);
  bk_audio_output_close(output);
  assert(native.initialized == 1 && native.started == 1 &&
         native.stopped == 1 && native.exited == 1);
}
static void failures(void) {
  char error[256];
  BkAudioSink sink;
  for (int failure = INIT; failure <= FORMAT; failure++) {
    memset(&native, 0, sizeof(native));
    native.failure = failure;
    BkAudioOutput *output = bk_audio_output_open(&sink, NULL, error);
    if (failure == INIT || failure == START || failure == ALLOCATION ||
        failure == FORMAT) {
      assert(!output && native.exited == (unsigned)(failure != INIT));
      continue;
    }
    assert(output);
    BkAudio *audio = bk_audio_create(&sink, error);
    assert(audio);
    if (failure == APPEND) {
      assert(!bk_audio_fill(audio, error));
      assert(!bk_audio_stats(audio).submitted);
    } else if (failure == RELEASE) {
      assert(bk_audio_fill(audio, error));
      assert(!bk_audio_poll(audio, error));
    } else {
      assert(bk_audio_fill(audio, error));
    }
    bk_audio_destroy(audio);
    bk_audio_output_close(output);
    assert(native.exited == 1 && native.stopped == 1);
  }
  memset(&native, 0, sizeof(native));
  BkAudioOutput *output = bk_audio_output_open(&sink, NULL, error);
  assert(output);
  BkAudio *audio = bk_audio_create(&sink, error);
  assert(audio && bk_audio_fill(audio, error));
  native.release_count = 1;
  native.invalid_release = 1;
  assert(!bk_audio_poll(audio, error));
  assert(bk_audio_stats(audio).failed && !bk_audio_stats(audio).consumed);
  bk_audio_destroy(audio);
  bk_audio_output_close(output);
}
static void copied_samples(void) {
  char error[256];
  BkAudioSink sink;
  memset(&native, 0, sizeof(native));
  BkAudioOutput *output = bk_audio_output_open(&sink, NULL, error);
  assert(output);
  int16_t samples[960];
  for (unsigned n = 0; n < 4; n++) {
    for (unsigned i = 0; i < 960; i++)
      samples[i] = (int16_t)(i * 31 + n);
    assert(sink.submit(sink.context, samples, 480, error));
    memset(samples, 0x77, sizeof(samples));
  }
  native.release_count = 2;
  uint64_t consumed;
  assert(sink.poll(sink.context, &consumed, error) && consumed == 960);
  assert(sink.submit(sink.context, samples, 480, error));
  memset(samples, 0, sizeof(samples));
  bk_audio_output_close(output); /* native exit checks remaining copied data */
}
static BkAudioClip *constant_clip(int value, char error[256]) {
  unsigned char wav[44 + 960] = {0};
  memcpy(wav, "RIFF", 4);
  memcpy(wav + 8, "WAVEfmt ", 8);
  memcpy(wav + 36, "data", 4);
  const unsigned offsets[] = {4, 16, 24, 28, 40};
  const uint32_t values[] = {sizeof(wav) - 8, 16, 48000, 96000, 960};
  for (unsigned i = 0; i < 5; ++i)
    for (unsigned b = 0; b < 4; ++b)
      wav[offsets[i] + b] = (uint8_t)(values[i] >> (8 * b));
  wav[20] = wav[22] = 1;
  wav[32] = 2;
  wav[34] = 16;
  for (unsigned i = 0; i < 480; ++i) {
    wav[44 + 2 * i] = (uint8_t)value;
    wav[45 + 2 * i] = (uint8_t)(value >> 8);
  }
  return bk_audio_clip_decode(wav, sizeof(wav), error);
}
static void independent_pump(void) {
  char error[256];
  BkAudioSink sink;
  memset(&native, 0, sizeof(native));
  native.automatic = 1;
  BkAudioOutput *output = bk_audio_output_open(&sink, NULL, error);
  assert(output);
  BkAudio *audio = bk_audio_create(&sink, error);
  assert(audio);
  BkAudioClip *clip = constant_clip(1234, error);
  assert(clip);
  assert(bk_audio_play(audio, 0, clip, 1, 0, 0, error));
  bk_audio_clip_release(clip);
  native.constant_pcm = 1;
  assert(bk_audio_output_start(output, audio, error));
  assert(!bk_audio_output_start(output, audio, error));
  /* Deliberately stall the main/game thread far beyond the40ms buffer.
   * The actual production pump runs on a host pthread through libnx stubs. */
  for (unsigned i = 0; i < 12; ++i) {
    uint64_t before = bk_audio_stats(audio).consumed;
    svcSleepThread(150000000);
    assert(bk_audio_output_check(output, error));
    BkAudioStats stats = bk_audio_stats(audio);
    assert(stats.consumed >= before + 10 * 480 && !stats.failed);
    assert(stats.queue_drains == 0);
  }
  bk_audio_output_stop(output);
  native.constant_pcm = 0;
  assert(bk_audio_output_start(output, audio, error));
  for (unsigned i = 0; i < 300; ++i) {
    BkAudioCursor old;
    assert(bk_audio_cursor(audio, 0, &old) && old.pcm);
    int16_t sample = bk_pcm_samples(old.pcm)[0];
    clip = constant_clip(i % 2 ? -2200 : 2200, error);
    assert(clip);
    assert(bk_audio_play(audio, 0, clip, 1, 0, 0, error));
    bk_audio_clip_release(clip);
    assert(bk_audio_gain(audio, 0, -1000, i % 2 ? -300 : 300, error));
    int32_t volume, pan;
    assert(bk_audio_get_gain(audio, 0, &volume, &pan) && volume == -1000 &&
           pan == (i % 2 ? -300 : 300));
    assert(bk_audio_frequency(audio, 0, i % 2 ? 22090 : 48000, error));
    assert(bk_audio_pause(audio, 0, error));
    assert(bk_audio_resume(audio, 0, 1, error));
    svcSleepThread(1000000);
    assert(bk_pcm_samples(old.pcm)[0] == sample);
    assert(bk_audio_output_check(output, error));
  }
  bk_audio_output_stop(output);
  uint64_t held = bk_audio_stats(audio).submitted;
  svcSleepThread(20000000);
  assert(bk_audio_stats(audio).submitted == held);
  assert(native.played >= 12 * 10 * 480 && native.flushes >= 12 * 10);
  printf("PASS independent audio pump: %llu played frames during12x150ms main "
         "stalls; no drains\n",
         (unsigned long long)native.played);
  bk_audio_destroy(audio);
  bk_audio_output_close(output);
  for (int stage = THREAD_CREATE; stage <= THREAD_START; ++stage) {
    memset(&native, 0, sizeof(native));
    native.failure = stage;
    output = bk_audio_output_open(&sink, NULL, error);
    assert(output);
    audio = bk_audio_create(&sink, error);
    assert(audio);
    assert(!bk_audio_output_start(output, audio, error));
    bk_audio_destroy(audio);
    bk_audio_output_close(output);
  }
}
int main(void) {
  independent_pump();
  normal();
  failures();
  copied_samples();
  puts("PASS libnx boundary mock: queue ownership, release order and failures");
}
