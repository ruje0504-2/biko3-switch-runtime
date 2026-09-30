/* Actual application owner, entry loading/retirement, GPU HUD capture and file
 * output. Boundary requests/group selection are explicit fixtures; ordinary
 * opening dialogue, draw/present and the next game's capture are real. */
#include "../runtime/app/play_session.c"
#include <stdint.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "capture lifecycle line%d (%s): %s\n", \
    __LINE__, #x, error); goto done; } } while (0)
static unsigned frames, writes;
static uint64_t file_hash = UINT64_C(14695981039346656037);
static int submit_pcm(void *p, const int16_t *pcm, size_t n, char e[256]) {
  (void)pcm; (void)e; *(uint64_t *)p += n; return 1;
}
static int poll_pcm(void *p, uint64_t *n, char e[256]) {
  (void)e; *n = *(uint64_t *)p; return 1;
}
static int read_time(void *p, BkCaptureTime *time, char e[256]) {
  (void)p; (void)e;
  *time = (BkCaptureTime){2026, 9, 30, 12, 34, 56, 100 + writes++};
  return 1;
}
static int present_frame(BkScene *scene, BkRenderer *r, BkAudio *a, char e[256]) {
  return bk_audio_fill(a, e) && bk_renderer_begin(r, e) &&
      bk_scene_draw(scene, &(BkSceneFrame){0}, e) && bk_renderer_end(r, e) &&
      bk_play_session_after_present(scene, e);
}
static int tick_frame(BkScene *scene, BkRenderer *r, BkAudio *a, char e[256]) {
  BkInput input = {0};
  if (++frames % 30 == 0) input.pressed = BK_BUTTON_CONFIRM;
  return bk_audio_poll(a, e) && bk_scene_step(scene, 1. / 60., &input, e) &&
      present_frame(scene, r, a, e);
}
static int hash_photo(BkCaptureFiles *files, const char *root, unsigned group,
                      unsigned index, char e[256]) {
  (void)files;
  char name[128], path[2048];
  BkCaptureTime time = {2026, 9, 30, 12, 34, 56, 100 + index};
  if (!bk_capture_photo_name(name, group, &time)) return 0;
  snprintf(path, sizeof(path), "%s/album/%s", root, name);
  FILE *in = fopen(path, "rb");
  if (!in) { snprintf(e, 256, "expected live-group photo missing"); return 0; }
  uint8_t raw[54 + 320 * 240 * 3];
  size_t size = fread(raw, 1, sizeof(raw), in);
  int ok = size == sizeof(raw) && fgetc(in) == EOF && !ferror(in);
  if (fclose(in)) ok = 0;
  if (!ok) return 0;
  BkImage image = {0};
  if (!bk_image_decode(raw, size, &image, e)) return 0;
  ok = image.width == 320 && image.height == 240;
  bk_image_free(&image);
  for (size_t i = 0; i < size; ++i)
    file_hash = (file_hash ^ raw[i]) * UINT64_C(1099511628211);
  return ok;
}
int main(int argc, char **argv) {
  if (argc != 3) return 2;
  char error[256] = {0}, path[2048];
  int result = 1;
  BkResourceStore *store = NULL;
  BkRenderer *renderer = NULL;
  BkAudio *audio = NULL;
  BkCaptureFiles *files = NULL;
  BkScene *scene = NULL;
  uint64_t submitted = 0;
  CHECK(store = bk_resources_create(error));
  const char *packs[] = {"bk3_00", "bk3_01", "bk3_02", "bk3_03", "bk3_04",
      "bk3_05", "bk3_06", "bk3_07", "bk3_15", "bk3_16", "bk3_20"};
  for (unsigned i = 0; i < sizeof(packs) / sizeof(*packs); ++i) {
    CHECK(snprintf(path, sizeof(path), "%s/%s.pp", argv[1], packs[i]) < (int)sizeof(path));
    CHECK(bk_resources_mount(store, packs[i], path, error));
  }
  const char *loose[] = {"routes", "faces", "collision", "fonts"};
  for (unsigned i = 0; i < 4; ++i)
    CHECK(bk_resources_mount_directory(store, loose[i], argv[1], 16 * 1024 * 1024, error));
  CHECK(renderer = bk_renderer_create(320, 240, stderr, error));
  BkRenderStats baseline = bk_renderer_stats(renderer);
  BkAudioSink sink = {&submitted, 48000, 240, 960, submit_pcm, poll_pcm};
  CHECK(audio = bk_audio_create(&sink, error));
  CHECK(files = bk_capture_files_create(argv[2], error));
  BkSceneServices services = {store, renderer, NULL, audio, files};
  CHECK(scene = bk_play_session_create_development(&services, NULL, error));
  CHECK(present_frame(scene, renderer, audio, error));
  PlaySession *s = bk_scene_custom_context(scene);
  BkScreenshot *owner = s->screenshot;
  CHECK(owner);
  s->capture_output.clock = (BkCaptureClock){NULL, read_time};
  s->game_state.random = UINT32_C(0x4af5e1);
  for (unsigned group = 0; group < 5; ++group) {
    CHECK(s->game && s->flow.current == 2);
    CHECK(present_frame(scene, renderer, audio, error));
    CHECK(writes == group); /*pure redraw must not duplicate the last file*/
    CHECK(bk_screenshot_request(owner, 1, (group + 1) % 5, &s->viewport, error));
    CHECK(schedule(s, 1, 0, error));
    CHECK(s->retire_game && s->game && s->screenshot == owner);
    CHECK(tick_frame(scene, renderer, audio, error));
    CHECK(!s->game && !s->retire_game && s->flow.current == 1);
    CHECK(s->screenshot == owner && writes == group);
    s->game_state.group = group; /*explicit menu-selection boundary fixture*/
    s->game_state.area = 0;
    CHECK(tick_frame(scene, renderer, audio, error));
    CHECK(s->game_state.album_group == group && writes == group);
    /* A failed new borrower must never destroy/cancel the process request. */
    CHECK(!bk_game_preview_create_entry(&services, &s->game_state, &s->progress,
                                        5, 0, 8, s->elapsed, owner, error));
    error[0] = 0;
    CHECK(schedule(s, 2, 0, error));
    CHECK(tick_frame(scene, renderer, audio, error));
    CHECK(s->game && s->flow.current == 2 && s->screenshot == owner);
    unsigned count = 0;
    while (writes == group && count++ < 3600)
      CHECK(tick_frame(scene, renderer, audio, error));
    CHECK(count < 3600 && writes == group + 1);
    CHECK(hash_photo(files, argv[2], group, group, error));
    int32_t photos[5];
    CHECK(bk_capture_files_count_photos(files, photos, error));
    for (unsigned i = 0; i < 5; ++i) CHECK(photos[i] == (int32_t)(i <= group));
  }
  bk_scene_destroy(scene); scene = NULL;
  BkRenderStats final = bk_renderer_stats(renderer);
  CHECK(final.live_allocations == baseline.live_allocations &&
        final.live_bytes == baseline.live_bytes);
  printf("PASS capture lifecycle: 5 retire/reenter +5 failed borrowers, %u frames, "
         "5 live-group photos, files=%016llx, GPU owners retired\n", frames,
         (unsigned long long)file_hash);
  result = 0;
done:
  bk_scene_destroy(scene);
  bk_audio_destroy(audio);
  bk_capture_files_destroy(files);
  bk_renderer_destroy(renderer);
  bk_resources_destroy(store);
  return result;
}
