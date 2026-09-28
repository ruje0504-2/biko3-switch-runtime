#include "media/audio.h"
#include "render/renderer.h"
#include "resource/store.h"
#include "scene/ending_normal_session.h"
#include "scene/scene.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x)                                                               \
  do {                                                                         \
    if (!(x)) {                                                                \
      fprintf(stderr, "line%d: %s\n", __LINE__, error);                      \
      goto done;                                                               \
    }                                                                          \
  } while (0)

typedef struct {
  uint64_t submitted;
} SinkState;

static int submit(void *context, const int16_t *samples, size_t frames,
                  char error[256]) {
  SinkState *s = context;
  (void)samples;
  (void)error;
  s->submitted += frames;
  return 1;
}

static int poll(void *context, uint64_t *consumed, char error[256]) {
  SinkState *s = context;
  (void)error;
  *consumed = s->submitted;
  return 1;
}

int main(int argc, char **argv) {
  if (argc < 2 || argc > 4)
    return 2;
  const unsigned variant = argc == 3 ? (unsigned)strtoul(argv[2], NULL, 10) : 0;
  const int story = argc == 4 && !strcmp(argv[3], "--story");
  if (argc == 4 && (!story || strcmp(argv[2], "0") != 0))
    return 2;
  if (variant > 1)
    return 2;
  char error[256] = {0}, path[1024];
  int result = 1;
  BkRenderer *renderer = NULL;
  BkResourceStore *store = NULL;
  BkAudio *audio = NULL;
  BkScene *scene = NULL;
  BkEndingRecords *records = NULL;
  SinkState sink_state = {0};
  const char *packs[] = {"bk3_02", "bk3_03", "bk3_04", "bk3_08",
                         "bk3_18", "fambom"};
  BkSceneServices services;
  CHECK(renderer = bk_renderer_create(64, 48, stdout, error));
  CHECK(store = bk_resources_create(error));
  for (unsigned i = 0; i < sizeof(packs) / sizeof(*packs); ++i) {
    snprintf(path, sizeof(path), "%s/%s.pp", argv[1], packs[i]);
    CHECK(bk_resources_mount(store, packs[i], path, error));
  }
  BkAudioSink sink = {&sink_state, 48000, 240, 960, submit, poll};
  CHECK(audio = bk_audio_create(&sink, error));
  services = (BkSceneServices){store, renderer, stdout, audio, NULL};
  if (story)
    CHECK(records = calloc(1, sizeof(*records)));
  CHECK(scene = story ? bk_ending_normal_scene_create_story(
                             &services, 0, 0, records, error)
                      : bk_ending_normal_scene_create(&services, 0, variant,
                                                      error));
  {
    BkSceneFrame first = {{0, 0, 0}, {0}};
    CHECK(bk_audio_poll(audio, error));
    CHECK(bk_audio_fill(audio, error));
    CHECK(bk_renderer_begin(renderer, error));
    CHECK(bk_scene_draw(scene, &first, error));
    CHECK(bk_renderer_end(renderer, error));
    CHECK(bk_ending_normal_scene_after_present(scene, error));
  }
  for (unsigned frame = 0; frame < 64; ++frame) {
    BkSceneFrame sample = {{1, 1.0 / 60.0, 0}, {0}};
    CHECK(bk_audio_poll(audio, error));
    CHECK(bk_scene_step(scene, 1.0 / 60.0, &sample.input, error));
    CHECK(bk_audio_fill(audio, error));
    CHECK(bk_renderer_begin(renderer, error));
    CHECK(bk_scene_draw(scene, &sample, error));
    CHECK(bk_renderer_end(renderer, error));
    CHECK(bk_ending_normal_scene_after_present(scene, error));
  }
  {
    BkRenderStats stats = bk_renderer_stats(renderer);
    const BkEndingState *state = bk_ending_normal_scene_state(scene);
    CHECK(state && state->frame.phase == (variant ? 2 : 1));
    CHECK(stats.draws > 0 && stats.skin_dispatches > 0 && sink_state.submitted);
    printf("ending normal scene PASS entry=%s variant=%u frames=64 draws=%llu skin=%llu "
           "audio_frames=%llu allocations=%llu\n",
           story ? "story" : "gallery", variant,
           (unsigned long long)stats.draws,
           (unsigned long long)stats.skin_dispatches,
           (unsigned long long)sink_state.submitted,
           (unsigned long long)stats.live_allocations);
  }
  result = 0;
done:
  bk_scene_destroy(scene);
  bk_audio_destroy(audio);
  bk_resources_destroy(store);
  bk_renderer_destroy(renderer);
  free(records);
  return result;
}
