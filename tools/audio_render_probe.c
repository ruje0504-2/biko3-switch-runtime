/* App actor scene driven by a controlled offline consumer, never a device. */
#include "scene/scene.h"
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
enum { WIDTH = 640, HEIGHT = 480, BYTES = WIDTH * HEIGHT * 4 };
typedef struct {
  uint64_t submitted, consumed, nonzero;
} Sink;
static int submit(void *ctx, const int16_t *samples, size_t frames,
                  char *error) {
  (void)error;
  Sink *sink = ctx;
  for (size_t i = 0; i < frames * 2; i++)
    sink->nonzero += samples[i] != 0;
  sink->submitted += frames;
  return 1;
}
static int poll(void *ctx, uint64_t *consumed, char *error) {
  (void)error;
  *consumed = ((Sink *)ctx)->consumed;
  return 1;
}
static int draw(BkRenderer *renderer, BkScene *scene, uint8_t *pixels,
                char *error) {
  BkSceneFrame frame = {0};
  return bk_renderer_begin(renderer, error) &&
         bk_scene_draw(scene, &frame, error) &&
         bk_renderer_end(renderer, error) &&
         bk_renderer_readback(renderer, pixels, BYTES, error);
}
static int save(const char *path, const uint8_t *bytes) {
  FILE *file = fopen(path, "wb");
  if (!file)
    return 0;
  int ok = fwrite(bytes, 1, BYTES, file) == BYTES;
  return fclose(file) == 0 && ok;
}
int main(int argc, char **argv) {
  if (argc != 3) {
    fprintf(stderr, "usage: audio-render-probe DATA_DIRECTORY OUTPUT.rgba\n");
    return 2;
  }
  char error[256] = {0};
  BkResourceStore *store = bk_resources_create(error);
  BkRenderer *renderer = NULL;
  BkScene *spoken = NULL, *silent = NULL;
  Sink sink = {0};
  BkAudioSink adapter = {&sink, 48000, 480, 1920, submit, poll};
  BkAudio *audio = bk_audio_create(&adapter, error);
  uint8_t *a = malloc(BYTES), *b = malloc(BYTES);
  int rc = 1;
  if (!store || !audio || !a || !b)
    goto done;
  const char *packs[] = {"bk3_01", "bk3_02", "bk3_04", "bk3_06"};
  for (unsigned i = 0; i < 4; i++) {
    char path[1024];
    if (snprintf(path, sizeof(path), "%s/%s.pp", argv[1], packs[i]) >=
            (int)sizeof(path) ||
        !bk_resources_mount(store, packs[i], path, error))
      goto done;
  }
  if (!bk_resources_mount_directory(store, "faces", argv[1], 20480, error) ||
      !bk_resources_mount_directory(store, "routes", argv[1], 20480, error))
    goto done;
  renderer = bk_renderer_create(WIDTH, HEIGHT, stderr, error);
  if (!renderer)
    goto done;
  BkSceneServices services = {store, renderer, stderr, audio, NULL};
  spoken = bk_scene_create(BK_SCENE_ACTOR_PREVIEW, &services, error);
  services.audio = NULL;
  silent = bk_scene_create(BK_SCENE_ACTOR_PREVIEW, &services, error);
  if (!spoken || !silent || !draw(renderer, spoken, a, error) ||
      !draw(renderer, silent, b, error))
    goto done;
  /* Banner differs intentionally; compare only the body above bottom overlay.
   */
  if (memcmp(a, b, WIDTH * 430 * 4)) {
    snprintf(error, 256, "silent initial body differs with an idle audio sink");
    goto done;
  }
  unsigned changed = 0, captures = 0, max_pixels = 0;
  for (unsigned frame = 0; frame < 240; frame++) {
    if (sink.submitted) {
      uint64_t available = sink.submitted - sink.consumed;
      sink.consumed += available < 800 ? available : 800;
    }
    BkInput idle = {0},
            input = {.pressed = frame == 0 ? BK_BUTTON_AUDIO_PREVIEW : 0};
    if (!bk_audio_poll(audio, error) ||
        !bk_scene_step(spoken, 1.0 / 60, &input, error) ||
        !bk_scene_step(silent, 1.0 / 60, &idle, error) ||
        !bk_audio_fill(audio, error))
      goto done;
    if (frame % 10)
      continue;
    if (!draw(renderer, spoken, a, error) || !draw(renderer, silent, b, error))
      goto done;
    captures += 2;
    unsigned pixels = 0;
    for (unsigned i = 0; i < WIDTH * 430; i++)
      pixels += memcmp(a + i * 4, b + i * 4, 4) != 0;
    changed += pixels != 0;
    if (pixels > max_pixels) {
      max_pixels = pixels;
      if (!save(argv[2], a)) {
        snprintf(error, 256, "voice image write failed");
        goto done;
      }
    }
  }
  if (!sink.nonzero || !changed) {
    snprintf(error, 256,
             "voice PCM did not produce audio and visible mouth change");
    goto done;
  }
  fprintf(stderr,
          "PASS offline audio->app actor scene->GPU: 240 steps, %u readbacks, "
          "%u voiced images, max %u body pixels differ, %" PRIu64
          " nonzero samples\n",
          captures, changed, max_pixels, sink.nonzero);
  rc = 0;
done:
  if (rc)
    fprintf(stderr, "FAIL audio render probe: %s\n", error);
  bk_scene_destroy(spoken);
  bk_scene_destroy(silent);
  bk_audio_destroy(audio);
  bk_renderer_destroy(renderer);
  bk_resources_destroy(store);
  free(a);
  free(b);
  return rc;
}
