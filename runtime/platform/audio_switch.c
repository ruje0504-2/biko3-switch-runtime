#include "platform/audio_output.h"
#include <malloc.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <switch.h>
enum { BLOCK_FRAMES = 480, BUFFER_COUNT = 4, BUFFER_BYTES = 4096 };
typedef struct {
  AudioOutBuffer native;
  uint64_t end;
  int busy;
} OutputBuffer;
struct BkAudioOutput {
  OutputBuffer buffers[BUFFER_COUNT];
  uint64_t submitted, consumed;
  int initialized, started, failed;
  BkAudio *audio;
  Mutex mutex;
  Thread thread;
  int thread_created, thread_started;
  atomic_int stop_requested, pump_failed;
  char pump_error[256];
};
static BkAudioOutput *active;
static int native_error(BkAudioOutput *output, Result result, const char *call,
                        char *error) {
  if (R_SUCCEEDED(result))
    return 1;
  output->failed = 1;
  snprintf(error, 256, "audio output: %s failed (0x%08x)", call, result);
  return 0;
}
static int submit(void *context, const int16_t *samples, size_t frames,
                  char error[256]) {
  BkAudioOutput *output = context;
  OutputBuffer *slot = NULL;
  for (unsigned i = 0; i < BUFFER_COUNT; i++)
    if (!output->buffers[i].busy) {
      slot = &output->buffers[i];
      break;
    }
  if (output->failed || !samples || frames != BLOCK_FRAMES || !slot ||
      output->submitted > UINT64_MAX - frames) {
    snprintf(error, 256, "audio output: invalid/full/failed queue");
    output->failed = 1;
    return 0;
  }
  memcpy(slot->native.buffer, samples, frames * 2 * sizeof(*samples));
  /* audout receives a descriptor, not an IPC copy of the sample payload. */
  armDCacheFlush(slot->native.buffer, BUFFER_BYTES);
  if (!native_error(output, audoutAppendAudioOutBuffer(&slot->native), "append",
                    error))
    return 0;
  slot->busy = 1;
  output->submitted += frames;
  slot->end = output->submitted;
  return 1;
}
static int poll(void *context, uint64_t *consumed, char error[256]) {
  BkAudioOutput *output = context;
  if (output->failed) {
    snprintf(error, 256, "audio output: failed session");
    return 0;
  }
  for (unsigned n = 0; n < BUFFER_COUNT; n++) {
    AudioOutBuffer *released = NULL;
    u32 count = 0;
    if (!native_error(output,
                      audoutGetReleasedAudioOutBuffer(&released, &count),
                      "release", error))
      return 0;
    if (!count)
      break;
    OutputBuffer *slot = NULL;
    for (unsigned i = 0; i < BUFFER_COUNT; i++)
      if (released == &output->buffers[i].native) {
        slot = &output->buffers[i];
        break;
      }
    if (count != 1 || !slot || !slot->busy ||
        slot->end != output->consumed + BLOCK_FRAMES) {
      output->failed = 1;
      snprintf(error, 256, "audio output: invalid released buffer/order");
      return 0;
    }
    output->consumed = slot->end;
    slot->busy = 0;
  }
  /* Completed-buffer lower bound, 10ms granularity. Never infer playback from
   * submitted frames or elapsed wall time, including when the queue runs dry.
   * Flushed/stopped buffers are not counted as played; only close stops here.
   */
  *consumed = output->consumed;
  return 1;
}
BkAudioOutput *bk_audio_output_open(BkAudioSink *sink, FILE *log,
                                    char error[256]) {
  if (!sink || active) {
    snprintf(error, 256, "audio output: invalid sink or already open");
    return NULL;
  }
  BkAudioOutput *output = calloc(1, sizeof(*output));
  if (!output) {
    snprintf(error, 256, "audio output: allocation failed");
    return NULL;
  }
  mutexInit(&output->mutex);
  atomic_init(&output->stop_requested, 0);
  atomic_init(&output->pump_failed, 0);
  if (!native_error(output, audoutInitialize(), "initialize", error))
    goto fail;
  output->initialized = 1;
  if (audoutGetSampleRate() != 48000 || audoutGetChannelCount() != 2 ||
      audoutGetPcmFormat() != PcmFormat_Int16) {
    snprintf(error, 256, "audio output: expected 48000Hz stereo PCM16");
    goto fail;
  }
  for (unsigned i = 0; i < BUFFER_COUNT; i++) {
    OutputBuffer *slot = &output->buffers[i];
    slot->native.buffer = memalign(4096, BUFFER_BYTES);
    if (!slot->native.buffer) {
      snprintf(error, 256, "audio output: aligned allocation failed");
      goto fail;
    }
    memset(slot->native.buffer, 0, BUFFER_BYTES);
    slot->native.buffer_size = BUFFER_BYTES;
    slot->native.data_size = BLOCK_FRAMES * 2 * sizeof(int16_t);
  }
  if (!native_error(output, audoutStartAudioOut(), "start", error))
    goto fail;
  output->started = 1;
  active = output;
  *sink = (BkAudioSink){
      output, 48000, BLOCK_FRAMES, BUFFER_COUNT * BLOCK_FRAMES, submit, poll};
  if (log) {
    fprintf(log, "Audio: libnx audout, 48000Hz PCM16 stereo; 4x10ms queue; "
                 "cursor uses completed buffers (10ms lower bound).\n");
    fflush(log);
  }
  return output;
fail:
  bk_audio_output_close(output);
  return NULL;
}
static void lock_mixer(void *context) {
  mutexLock(&((BkAudioOutput *)context)->mutex);
}
static void unlock_mixer(void *context) {
  mutexUnlock(&((BkAudioOutput *)context)->mutex);
}
static void pump(void *context) {
  BkAudioOutput *output = context;
  while (!atomic_load_explicit(&output->stop_requested, memory_order_acquire)) {
    if (!bk_audio_poll(output->audio, output->pump_error) ||
        !bk_audio_fill(output->audio, output->pump_error)) {
      atomic_store_explicit(&output->pump_failed, 1, memory_order_release);
      break;
    }
    /* Short bounded sleep, no busy spin and no mixer lock held while asleep.
     * 2ms polling fits well inside the4x10ms device queue. */
    svcSleepThread(2000000);
  }
}
int bk_audio_output_start(BkAudioOutput *output, BkAudio *audio, char e[256]) {
  if (!output || !audio || output->audio || output->failed) {
    snprintf(e, 256, "audio output: invalid pump start");
    return 0;
  }
  BkAudioSync sync = {output, lock_mixer, unlock_mixer};
  if (!bk_audio_set_sync(audio, &sync, e))
    return 0;
  output->audio = audio;
  atomic_store(&output->stop_requested, 0);
  atomic_store(&output->pump_failed, 0);
  if (!bk_audio_poll(audio, e) || !bk_audio_fill(audio, e) ||
      !native_error(
          output,
          threadCreate(&output->thread, pump, output, NULL, 0x10000, 0x2b, -2),
          "threadCreate", e))
    goto bad;
  output->thread_created = 1;
  if (!native_error(output, threadStart(&output->thread), "threadStart", e))
    goto bad;
  output->thread_started = 1;
  return 1;
bad:
  bk_audio_output_stop(output);
  return 0;
}
int bk_audio_output_check(BkAudioOutput *output, char e[256]) {
  if (!output || !output->thread_started) {
    snprintf(e, 256, "audio output: pump not running");
    return 0;
  }
  if (atomic_load_explicit(&output->pump_failed, memory_order_acquire)) {
    memcpy(e, output->pump_error, 256);
    return 0;
  }
  return 1;
}
void bk_audio_output_stop(BkAudioOutput *output) {
  if (!output)
    return;
  atomic_store_explicit(&output->stop_requested, 1, memory_order_release);
  if (output->thread_started)
    threadWaitForExit(&output->thread);
  if (output->thread_created)
    threadClose(&output->thread);
  output->thread_started = output->thread_created = 0;
  if (output->audio) {
    char ignored[256];
    bk_audio_set_sync(output->audio, NULL, ignored);
    output->audio = NULL;
  }
}
void bk_audio_output_close(BkAudioOutput *output) {
  if (!output)
    return;
  bk_audio_output_stop(output);
  if (output->started)
    audoutStopAudioOut();
  if (output->initialized)
    audoutExit();
  for (unsigned i = 0; i < BUFFER_COUNT; i++)
    free(output->buffers[i].native.buffer);
  if (active == output)
    active = NULL;
  free(output);
}
