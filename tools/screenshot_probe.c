/* Actual HUD -> synchronous capture -> keyed original watermark -> BMP files.
 * Compare captured crop to a separate rendering stopped BEFORE ordinary HUD. */
#include "app/capture_output.h"
#include "scene/player_hotkeys.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#define W 640
#define H 480
#define CHECK(x)                                                               \
  do {                                                                         \
    if (!(x))                                                                  \
      goto done;                                                               \
  } while (0)
/* Output remains offline; real samples are independently verified by
 * player-hotkeys-probe. Here the sink consumes every submitted block. */
static int audio_submit(void *ctx, const int16_t *p, size_t n, char e[256]) {
  (void)p;
  (void)e;
  *(uint64_t *)ctx += n;
  return 1;
}
static int audio_poll(void *ctx, uint64_t *n, char e[256]) {
  (void)e;
  *n = *(uint64_t *)ctx;
  return 1;
}
typedef struct {
  BkCaptureTime time;
  unsigned reads;
} Clock;
static int clock_read(void *ctx, BkCaptureTime *t, char e[256]) {
  (void)e;
  Clock *c = ctx;
  *t = c->time;
  ++c->reads;
  return 1;
}
static int load_image(const char *path, BkImage *image, char e[256]) {
  FILE *f = fopen(path, "rb");
  if (!f)
    return 0;
  if (fseek(f, 0, SEEK_END)) {
    fclose(f);
    return 0;
  }
  long n = ftell(f);
  if (n <= 0 || n > 4 * 1024 * 1024 || fseek(f, 0, SEEK_SET)) {
    fclose(f);
    return 0;
  }
  uint8_t *raw = malloc((size_t)n);
  if (!raw) {
    fclose(f);
    return 0;
  }
  int ok = fread(raw, 1, (size_t)n, f) == (size_t)n;
  fclose(f);
  if (ok)
    ok = bk_image_decode(raw, (size_t)n, image, e);
  free(raw);
  return ok;
}
int main(int argc, char **argv) {
  if (argc != 3)
    return 2;
  char e[256] = {0}, path[1400];
  int rc = 1;
  BkAudio *audio = NULL;
  BkSystemAudio *sounds[3] = {0};
  uint64_t audio_frames = 0;
  unsigned samples = 0, changed = 0;
  BkRenderer *r = NULL;
  BkResourceStore *store = bk_resources_create(e);
  BkCaptureFiles *files = NULL;
  BkScreenshot *shot = NULL;
  BkPlayerHudRender *hud = NULL;
  BkTexture *bg = NULL;
  BkBlob blob = {0};
  BkImage watermark = {0}, written = {0};
  uint8_t *ref = malloc(W * H * 4), *final = malloc(W * H * 4);
  CHECK(store && ref && final);
  for (unsigned i = 0; i < 3; ++i) {
    const char *pack = i == 2 ? "bk3_02" : i ? "bk3_15" : "bk3_00";
    snprintf(path, sizeof(path), "%s/%s.pp", argv[1], pack);
    CHECK(bk_resources_mount(store, pack, path, e));
  }
  CHECK(bk_resources_read(store, "bk3_15", "cp.bmp", &blob, e) ==
        BK_RESOURCE_OK);
  CHECK(bk_image_decode(blob.data, blob.size, &watermark, e));
  bk_blob_free(&blob);
  r = bk_renderer_create(W, H, stderr, e);
  CHECK(r);
  files = bk_capture_files_create(argv[2], e);
  CHECK(files);
  Clock clock = {{2026, 9, 27, 15, 47, 36, 18}, 0};
  BkCaptureOutput output = {files, {&clock, clock_read}};
  BkScreenshotOutput output_ops = bk_capture_output_service(&output);
  shot = bk_screenshot_create(r, store, &output_ops, e);
  CHECK(shot);
  BkPlayerHudCapture service = bk_screenshot_service(shot);
  hud = bk_player_hud_render_create(r, store, 2, 0, e);
  CHECK(hud);
  uint8_t rgba[] = {46, 83, 129, 255};
  BkImage color = {1, 1, rgba};
  bg = bk_texture_create(r, &color, e);
  CHECK(bg);
  const BkVertex quad[6] = {
      {-1, -1, .8f, 0, 0, 1, 1, 1, 1}, {1, -1, .8f, 1, 0, 1, 1, 1, 1},
      {1, 1, .8f, 1, 1, 1, 1, 1, 1},   {-1, -1, .8f, 0, 0, 1, 1, 1, 1},
      {1, 1, .8f, 1, 1, 1, 1, 1, 1},   {-1, 1, .8f, 0, 1, 1, 1, 1, 1}};
  BkViewport crop = {32, 24, 576, 432};
  BkScreenshotRequest request = {shot, crop};
  BkAudioSink sink = {&audio_frames, 48000, 240, 960, audio_submit, audio_poll};
  audio = bk_audio_create(&sink, e);
  CHECK(audio);
  const unsigned slots[] = {0, 5, 7};
  for (unsigned i = 0; i < 3; ++i) {
    sounds[i] = bk_system_audio_create_slot(store, audio, i, slots[i], -600, e);
    CHECK(sounds[i]);
  }
  BkPlayerHotkeyServices hotkeys = {sounds[0], sounds[1], sounds[2], &request,
                                    bk_screenshot_request_service};
  BkPlayerHotkeys keys = {.photo_count = 95};
  uint8_t camera = 0;

  for (unsigned i = 0; i < 10; ++i) {
    BkPlayerHudState state = {0};
    CHECK(bk_player_hud_initialize(&state, 2, 0, crop.width));
    BkPlayerHudInput in = {.counter = 80,
                           .npc_in_view = 1,
                           .prop_available = 1,
                           .npc_prompt = 1,
                           .cover_available = 1};
    for (unsigned k = 0; k < 21; ++k)
      in.actions[k] = (int32_t)k;
    memset(in.inventory, 1, 5);
    if (i % 3 == 0) {
      in.action = 11;
      in.interface_mode = 2;
      in.trigger_kind = 10;
    }
    BkScreenPoint point = {{288, 216}, .5f};
    uint8_t outcome = 0;
    BkPlayerHudFrame frame;
    CHECK(bk_player_hud_update(&state, &in, &outcome, &point, .25f, 1000,
                               crop.width, e));
    CHECK(bk_player_hud_draws(&state, &in, crop.width, &frame, e));
    BkPlayerHudFrame prefix = frame;
    prefix.count = frame.capture_after;
    prefix.capture = 0;
    CHECK(
        bk_player_hud_render_prepare(hud, &prefix, crop.width, crop.height, e));
    CHECK(bk_renderer_begin(r, e));
    CHECK(bk_renderer_draw(r, bg, quad, 6, bk_identity, e));
    CHECK(bk_renderer_viewport(r, &crop, e));
    CHECK(bk_player_hud_render_draw(hud, NULL, e));
    CHECK(bk_renderer_end(r, e));
    CHECK(bk_renderer_readback(r, ref, W * H * 4, e));
    unsigned photo = i % 2, group = i / 2;
    clock.time.ticks_ms = 18 + i;
    CHECK(bk_audio_poll(audio, e));
    unsigned active_group = (group + 1) % 5;
    CHECK(bk_scene_player_hotkeys(
        &hotkeys, &keys, &camera, active_group, group, 0,
        BK_PLAYER_PAUSE | BK_PLAYER_CAMERA | (photo ? BK_PLAYER_PHOTO : 0), e));
    assert(keys.menu_request == 1 && camera == ((i + 1) % 2));
    if (photo)
      assert(keys.photos[active_group] == (int)(96 + group));
    CHECK(bk_audio_fill(audio, e));
    CHECK(
        bk_player_hud_render_prepare(hud, &frame, crop.width, crop.height, e));
    CHECK(bk_renderer_begin(r, e));
    CHECK(bk_renderer_draw(r, bg, quad, 6, bk_identity, e));
    CHECK(bk_renderer_viewport(r, &crop, e));
    CHECK(bk_player_hud_render_draw(hud, &service, e));
    CHECK(bk_renderer_end(r, e));
    CHECK(bk_renderer_readback(r, final, W * H * 4, e));
    assert(service.capture(service.context,
                           e)); /*pending cleared, no second write/clock*/
    char name[128] = "sy_99.bmp";
    if (photo)
      CHECK(bk_capture_photo_name(name, group, &clock.time));
    snprintf(path, sizeof(path), "%s/%s%s", argv[2], photo ? "album/" : "",
             name);
    CHECK(load_image(path, &written, e));
    assert(written.width == crop.width && written.height == crop.height);
    for (unsigned y = 0; y < crop.height; ++y)
      for (unsigned x = 0; x < crop.width; ++x) {
        const uint8_t *want = ref + ((size_t)(y + crop.y) * W + x + crop.x) * 4;
        if (photo && x >= crop.width - 136 && x < crop.width - 8 &&
            y >= crop.height - 40 && y < crop.height - 8) {
          const uint8_t *cp =
              watermark.rgba + ((size_t)(y - (crop.height - 40)) * 128 + x -
                                (crop.width - 136)) *
                                   4;
          if (cp[0] != 255 || cp[1] != 0 || cp[2] != 0)
            want = cp;
        }
        const uint8_t *got = written.rgba + ((size_t)y * crop.width + x) * 4;
        if (memcmp(got, want, 3)) {
          snprintf(e, 256, "screenshot mismatch case%u xy%u,%u", i, x, y);
          goto done;
        }
        samples += 3;
        if (memcmp(final + ((size_t)(y + crop.y) * W + x + crop.x) * 4,
                   ref + ((size_t)(y + crop.y) * W + x + crop.x) * 4, 3))
          ++changed;
      }
    bk_image_free(&written);
  }
  assert(clock.reads == 5 && changed > 0 && keys.photo_count == 100);
  CHECK(bk_scene_player_hotkeys(&hotkeys, &keys, &camera, 0, 0, 0,
                                BK_PLAYER_PHOTO, e));
  CHECK(service.capture(service.context, e)); /* limit: no pending screenshot */
  keys.photo_count = 99;
  CHECK(bk_scene_player_hotkeys(&hotkeys, &keys, &camera, 0, 0, 1,
                                BK_PLAYER_PHOTO, e));
  CHECK(service.capture(service.context,
                        e)); /* special1: no pending screenshot */
  assert(keys.photo_count == 99);
  CHECK(bk_screenshot_request(shot, 1, 0, &crop, e));
  bk_screenshot_cancel(shot);
  CHECK(service.capture(service.context, e));
  assert(clock.reads == 5);
  printf("PASS screenshot: 5 pause replacements +5 photos, %u RGB bytes exact, "
         "%u later-HUD pixels excluded\n",
         samples, changed);
  rc = 0;
done:
  if (rc)
    fprintf(stderr, "screenshot probe: %s\n", e);
  for (unsigned i = 0; i < 3; ++i)
    bk_system_audio_destroy(sounds[i]);
  bk_audio_destroy(audio);
  bk_image_free(&written);
  bk_image_free(&watermark);
  bk_blob_free(&blob);
  bk_screenshot_destroy(shot);
  bk_player_hud_render_destroy(hud);
  bk_texture_destroy(r, bg);
  bk_renderer_destroy(r);
  bk_capture_files_destroy(files);
  bk_resources_destroy(store);
  free(ref);
  free(final);
  return rc;
}
