/* Original area rules -> two real PCM voices, including shared player effect
 * replacement. Offline sink checks exact samples at their authored22050Hz. */
#include "scene/area_audio.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
typedef struct {
  const BkPcm *reference;
  uint64_t submitted, samples, nonzero, hash;
  size_t position;
  int voices;
} Sink;
static int submit(void *context, const int16_t *pcm, size_t frames,
                  char *error) {
  (void)error;
  Sink *s = context;
  const int16_t *source = s->reference ? bk_pcm_samples(s->reference) : NULL;
  size_t count = s->reference ? bk_pcm_frames(s->reference) : 0;
  for (size_t i = 0; i < frames; ++i) {
    int32_t v = s->position < count ? source[s->position] * s->voices : 0;
    if (v > 32767)
      v = 32767;
    if (v < -32768)
      v = -32768;
    for (unsigned c = 0; c < 2; ++c) {
      assert(pcm[i * 2 + c] == v);
      s->samples++;
      s->nonzero += v != 0;
      s->hash ^= (uint16_t)pcm[i * 2 + c];
      s->hash *= UINT64_C(1099511628211);
    }
    s->position++;
  }
  s->submitted += frames;
  return 1;
}
static int poll(void *context, uint64_t *consumed, char *error) {
  (void)error;
  *consumed = ((Sink *)context)->submitted;
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
  BkPcm *pcm[3] = {0};
  BkBlob blob = {0};
  Sink sink = {.hash = UINT64_C(14695981039346656037)};
  unsigned cues = 0;
  const char *names[] = {"se304.wav", "se155.wav", "se156.wav"};
  CHECK(store);
  CHECK(snprintf(path, sizeof(path), "%s/bk3_02.pp", argv[1]) <
        (int)sizeof(path));
  CHECK(bk_resources_mount(store, "bk3_02", path, error));
  for (unsigned i = 0; i < 3; ++i) {
    CHECK(bk_resources_read(store, "bk3_02", names[i], &blob, error) ==
          BK_RESOURCE_OK);
    pcm[i] = bk_pcm_decode(blob.data, blob.size, error);
    bk_blob_free(&blob);
    CHECK(pcm[i]);
    assert(bk_pcm_rate(pcm[i]) == 22050 && bk_pcm_channels(pcm[i]) == 1);
  }
  BkAudioSink output = {&sink, 22050, 147, 588, submit, poll};
  audio = bk_audio_create(&output, error);
  CHECK(audio);
  player = bk_player_audio_create(store, audio, 2, error);
  CHECK(player);
  area = bk_area_audio_create(store, audio, 3, player, error);
  CHECK(area);
  BkResourceStore *empty = bk_resources_create(error);
  CHECK(empty);
  assert(!bk_area_audio_create(empty, audio, 3, player, error));
  bk_resources_destroy(empty);
  const unsigned triggers[][3] = {
      {0, 6, 234}, {0, 7, 55},  {1, 6, 132}, {1, 7, 26},  {2, 4, 165},
      {2, 5, 280}, {2, 6, 230}, {3, 6, 276}, {3, 7, 241}, {4, 7, 329}};
  for (unsigned t = 0; t < 10; ++t) {
    BkAreaBoundaryState state = {0};
    BkPropState props[16] = {{0}};
    BkNpcSpatialState npc = {0};
    BkNpcInteractionState interaction = {0};
    npc.path.position[0] = npc.path.position[2] = 500;
    npc.path.cursor = triggers[t][2];
    BkAreaBoundaryInput input = {.group = triggers[t][0],
                                 .area = triggers[t][1],
                                 .npc_group = (int32_t)triggers[t][0],
                                 .npc_present = 1,
                                 .player_position = {500, 0, 500},
                                 .player_yaw = 180,
                                 .player_wall = "exit",
                                 .boundary_wall = "exit",
                                 .bounds = {-200, 200, 200, -200}};
    BkAreaBoundaryCommands commands;
    CHECK(bk_area_boundary_step(&state, props, &npc, &interaction, &input,
                                &commands));
    assert(commands.count == 2);
    unsigned file = 0;
    while (file < 3 && strcmp(commands.commands[0].file, names[file]))
      file++;
    assert(file < 3);
    CHECK(bk_area_audio_apply(area, &commands, error));
    sink.reference = pcm[file];
    sink.position = 0;
    sink.voices = 2;
    for (unsigned frame = 0; frame < 4; ++frame) {
      CHECK(bk_audio_poll(audio, error));
      CHECK(bk_audio_fill(audio, error));
    }
    /* Invalid second command must not restart the otherwise-valid first. */
    commands.commands[1].file = "missing.wav";
    assert(!bk_area_audio_apply(area, &commands, error));
    CHECK(bk_audio_poll(audio, error));
    CHECK(bk_audio_fill(audio, error));
    CHECK(bk_area_audio_stop(area, error));
    sink.voices = 1;
    CHECK(bk_audio_poll(audio, error));
    CHECK(bk_audio_fill(audio, error));
    CHECK(bk_player_audio_stop(player, error));
    sink.voices = 0;
    CHECK(bk_audio_poll(audio, error));
    CHECK(bk_audio_fill(audio, error));
    cues++;
  }
  assert(sink.nonzero);
  printf("PASS area cues=%u samples=%" PRIu64 " nonzero=%" PRIu64
         " hash=%016" PRIx64 "\n",
         cues, sink.samples, sink.nonzero, sink.hash);
  rc = 0;
done:
  if (rc)
    fprintf(stderr, "%s\n", error);
  bk_area_audio_destroy(area);
  bk_player_audio_destroy(player);
  bk_audio_destroy(audio);
  for (unsigned i = 0; i < 3; ++i)
    bk_pcm_destroy(pcm[i]);
  bk_blob_free(&blob);
  bk_resources_destroy(store);
  return rc;
}
