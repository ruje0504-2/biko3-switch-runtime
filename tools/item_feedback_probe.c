/* Actual pickup -> se101 PCM and i00_00 message, using an explicit offline
 * sink. PCM is independently compared to authored samples at authored rate;
 * this is not audible host output or a complete game/notice display loop. */
#include "scene/item_assets.h"
#include "scene/item_feedback.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
typedef struct {
  const BkPcm *reference;
  uint64_t submitted, consumed, samples, nonzero, hash;
  size_t position;
  int active;
} Sink;
static int submit(void *context, const int16_t *pcm, size_t frames,
                  char *error) {
  (void)error;
  Sink *s = context;
  const int16_t *source = bk_pcm_samples(s->reference);
  size_t count = bk_pcm_frames(s->reference);
  uint32_t channels = bk_pcm_channels(s->reference);
  for (size_t i = 0; i < frames; ++i) {
    for (unsigned c = 0; c < 2; ++c) {
      int16_t expected =
          s->active && s->position < count
              ? source[s->position * channels + (channels == 1 ? 0 : c)]
              : 0;
      assert(pcm[i * 2 + c] == expected);
      uint16_t bits = (uint16_t)expected;
      s->nonzero += expected != 0;
      s->samples++;
      for (unsigned j = 0; j < 2; ++j) {
        s->hash ^= (bits >> (j * 8)) & 255;
        s->hash *= UINT64_C(1099511628211);
      }
    }
    if (s->active && s->position < count)
      s->position++;
  }
  s->submitted += frames;
  return 1;
}
static int poll(void *context, uint64_t *consumed, char *error) {
  (void)error;
  *consumed = ((Sink *)context)->consumed;
  return 1;
}
int main(int argc, char **argv) {
  if (argc != 2)
    return 2;
  char error[256] = {0}, path[1024];
  BkResourceStore *store = bk_resources_create(error);
  BkAudio *audio = NULL;
  BkItemFeedback *feedback = NULL;
  BkItemAssets *items = NULL;
  BkPcm *pcm = NULL;
  BkBlob blob = {0}, text = {0};
  unsigned profiles = 0, hits = 0, messages = 0;
  int rc = 1;
  Sink sink = {.hash = UINT64_C(14695981039346656037)};
#define CHECK(x)                                                               \
  do {                                                                         \
    if (!(x))                                                                  \
      goto done;                                                               \
  } while (0)
  CHECK(store);
  const char *packs[] = {"bk3_02", "bk3_05", "bk3_16"};
  for (unsigned i = 0; i < 3; ++i) {
    CHECK(snprintf(path, sizeof(path), "%s/%s.pp", argv[1], packs[i]) <
          (int)sizeof(path));
    CHECK(bk_resources_mount(store, packs[i], path, error));
  }
  CHECK(bk_resources_read(store, "bk3_02", "se101.wav", &blob, error) ==
        BK_RESOURCE_OK);
  pcm = bk_pcm_decode(blob.data, blob.size, error);
  CHECK(pcm);
  bk_blob_free(&blob);
  CHECK(bk_resources_read(store, "bk3_05", "i00_00.txt", &text, error) ==
        BK_RESOURCE_OK);
  sink.reference = pcm;
  BkAudioSink output = {&sink, bk_pcm_rate(pcm), 147, 588, submit, poll};
  audio = bk_audio_create(&output, error);
  CHECK(audio);
  /* Missing resources/invalid args fail without starting or clearing voice. */
  BkResourceStore *empty = bk_resources_create(error);
  CHECK(empty);
  assert(!bk_item_feedback_create(empty, audio, 2, 0, error));
  bk_resources_destroy(empty);
  assert(!bk_item_feedback_create(store, audio, BK_AUDIO_VOICES, 0, error));
  assert(!bk_item_feedback_create(store, audio, 2, 1, error));
  feedback = bk_item_feedback_create(store, audio, 2, 0, error);
  CHECK(feedback);
  BkItemPickupOps ops = bk_item_feedback_ops(feedback);
  CHECK(bk_audio_fill(audio, error)); /* Initial silence. */
  for (unsigned g = 0; g < 5; ++g)
    for (unsigned a = 0; a < 9; ++a) {
      BkItemPickupState state = {0};
      BkItemState retained[BK_ITEM_LIMIT] = {0};
      items =
          bk_item_assets_create(store, g, a, state.collected, retained, error);
      CHECK(items);
      profiles++;
      for (unsigned slot = 0; slot < bk_item_assets_count(items); ++slot) {
        const BkItemState *s = bk_item_assets_state(items, slot);
        float current[3], previous[3];
        memcpy(current, s->position, 12);
        memcpy(previous, current, 12);
        current[0] -= 12;
        previous[0] += 12;
        CHECK(bk_item_assets_step(items, 1.f / 60.f, error));
        sink.consumed += 147;
        CHECK(bk_audio_poll(audio, error));
        BkItemPickups picked;
        CHECK(bk_item_assets_pickup(items, &state, current, previous, &ops,
                                    &picked, error));
        if (picked.count) {
          hits += picked.count;
          sink.position = 0;
          sink.active = 1;
          BkMessage expected = {0};
          CHECK(bk_message_lookup(text.data, text.size,
                                  (int32_t)(g * 10000 + state.selected_item),
                                  &expected, error));
          const BkMessage *actual = bk_item_feedback_message(feedback);
          assert(actual->length && actual->length == expected.length);
          assert(actual->carriage_returns == expected.carriage_returns);
          assert(!memcmp(actual->bytes, expected.bytes, BK_MESSAGE_CAPACITY));
          assert(state.notice_visible == 1 && state.notice_timer_armed == 0 &&
                 state.message_cursor == 0);
          assert(state.collected[state.selected_item] == 1);
          messages++;
        }
        CHECK(bk_audio_fill(audio, error));
      }
      bk_item_assets_destroy(items);
      items = NULL;
    }
  assert(hits == 31);
  /* Let the final one-shot finish; further blocks must remain silent. */
  for (size_t n = 0; n < bk_pcm_frames(pcm) / 147 + 8; ++n) {
    sink.consumed += 147;
    CHECK(bk_audio_poll(audio, error) && bk_audio_fill(audio, error));
  }
  int playing;
  assert(bk_audio_playing(audio, 2, &playing) && !playing);
  CHECK(ops.play_sound(ops.context, error));
  sink.position = 0;
  sink.active = 1;
  CHECK(bk_audio_fill(audio, error));
  /* Destroying the adapter cannot invalidate its queued/retained clip. */
  bk_item_feedback_destroy(feedback);
  feedback = NULL;
  for (unsigned i = 0; i < 16; ++i) {
    sink.consumed += 147;
    CHECK(bk_audio_poll(audio, error) && bk_audio_fill(audio, error));
  }
  CHECK(bk_audio_clear(audio, 2, error));
  sink.active = 0;
  sink.consumed += 147;
  CHECK(bk_audio_poll(audio, error) && bk_audio_fill(audio, error));
  assert(sink.nonzero);
  printf("PASS item feedback offline profiles=%u pickups=%u messages=%u "
         "PCM_samples=%" PRIu64 " nonzero=%" PRIu64 " hash=%016" PRIx64 "\n",
         profiles, hits, messages, sink.samples, sink.nonzero, sink.hash);
  rc = 0;
done:
  if (rc)
    fprintf(stderr, "%s\n", error);
  bk_item_assets_destroy(items);
  if (feedback) {
    char ignored[256];
    bk_item_feedback_stop(feedback, ignored);
  }
  bk_item_feedback_destroy(feedback);
  bk_audio_destroy(audio);
  bk_pcm_destroy(pcm);
  bk_blob_free(&blob);
  bk_blob_free(&text);
  bk_resources_destroy(store);
  return rc;
}
