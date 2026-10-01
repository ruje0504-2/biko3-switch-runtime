/* App-level integration: real opening -> phase1 -> movement/camera/photo.
 * No state writes or phase overrides. Runs the same factory used on Switch. */
#include "app/game_preview.h"
#include "app/pause_preview.h"
#include "save/capture_file.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define REQUIRE(expr)                                                          \
  do {                                                                         \
    if (!(expr)) {                                                             \
      fprintf(stderr, "game session line%d: %s\n", __LINE__, error);           \
      goto done;                                                               \
    }                                                                          \
  } while (0)
int main(int argc, char **argv) {
  if (argc != 4) {
    fprintf(stderr, "usage: game-session-probe DATA CAPTURES OUTPUT.rgba\n");
    return 2;
  }
  char error[256] = {0}, path[2048];
  int status = 1;
  BkResourceStore *store = bk_resources_create(error);
  BkRenderer *renderer = NULL;
  BkCaptureFiles *files = NULL;
  BkScene *scene = NULL, *pause = NULL;
  BkBlob pause_bmp = {0};
  BkImage pause_image = {0};
  uint8_t *pixels = NULL;
  REQUIRE(store);
  const char *packs[] = {"bk3_00", "bk3_01", "bk3_02", "bk3_03", "bk3_04",
                         "bk3_05", "bk3_06", "bk3_07", "bk3_15", "bk3_16", "bk3_20"};
  for (unsigned i = 0; i < sizeof(packs) / sizeof(packs[0]); ++i) {
    REQUIRE(snprintf(path, sizeof(path), "%s/%s.pp", argv[1], packs[i]) <
            (int)sizeof(path));
    REQUIRE(bk_resources_mount(store, packs[i], path, error));
  }
  const char *loose[] = {"routes", "faces", "collision", "fonts"};
  for (unsigned i = 0; i < 4; ++i)
    REQUIRE(bk_resources_mount_directory(store, loose[i], argv[1],
                                         16 * 1024 * 1024, error));
  renderer = bk_renderer_create(1280, 720, stderr, error);
  files = bk_capture_files_create(argv[2], error);
  REQUIRE(renderer && files);
  BkSceneServices services = {store, renderer, stderr, NULL, files, NULL};
  scene = bk_game_preview_create(&services, error);
  REQUIRE(scene);
  unsigned active_frames = 0, handovers = 0, total = 0;
  uint8_t old_phase = bk_game_preview_state(scene)->camera.phase;
  for (; total < 3600 && active_frames < 60; ++total) {
    const BkGameFrameState *s = bk_game_preview_state(scene);
    BkInput input = {0};
    if (s->camera.phase == 0 && total % 30 == 29)
      input.pressed = BK_BUTTON_CONFIRM;
    if (s->camera.phase == 1) {
      if (active_frames == 3 || active_frames == 30)
        input.pressed = BK_BUTTON_PHOTO;
      if (active_frames == 10 || active_frames == 20)
        input.pressed = BK_BUTTON_GAME_CAMERA;
      if (active_frames >= 35 && active_frames < 50)
        input.held = BK_BUTTON_UP;
      ++active_frames;
    }
    BkSceneFrame frame = {.clock = {.steps = 1, .step_seconds = 1.0 / 60},
                          .input = input};
    REQUIRE(bk_scene_step(scene, 1.0 / 60, &input, error));
    REQUIRE(bk_renderer_begin(renderer, error));
    REQUIRE(bk_scene_draw(scene, &frame, error));
    REQUIRE(bk_renderer_end(renderer, error));
    s = bk_game_preview_state(scene);
    if (s->camera.phase != old_phase) {
      fprintf(stderr, "phase%u -> %u at frame%u\n", old_phase, s->camera.phase,
              total);
      handovers += s->camera.phase == 1;
      old_phase = s->camera.phase;
    }
  }
  REQUIRE(handovers == 1 && active_frames == 60);
  const BkGameFrameState *s = bk_game_preview_state(scene);
  REQUIRE(s->hotkeys.photo_count == 2 && s->hotkeys.photos[0] == 2);
  REQUIRE(s->camera.transition == 0);
  BkInput pause_key = {.pressed = BK_BUTTON_PAUSE};
  BkSceneFrame empty_frame = {0};
  REQUIRE(bk_scene_step(scene, 1.0 / 60, &pause_key, error));
  REQUIRE(bk_renderer_begin(renderer, error));
  REQUIRE(bk_scene_draw(scene, &empty_frame, error));
  REQUIRE(bk_renderer_end(renderer, error));
  REQUIRE(bk_game_preview_state(scene)->hotkeys.menu_request == 1);
  REQUIRE(bk_capture_file_read_pause(files, 16 * 1024 * 1024, &pause_bmp,
                                     error) == BK_RESOURCE_OK);
  REQUIRE(bk_image_decode(pause_bmp.data, pause_bmp.size, &pause_image, error));
  REQUIRE(pause_image.width == 960 && pause_image.height == 720);
  bk_blob_free(&pause_bmp);
  bk_image_free(&pause_image);
  BkGameFrameState held = *bk_game_preview_state(scene);
  services.audio = bk_game_preview_audio(scene);
  pause = bk_pause_preview_create(&services, error);
  REQUIRE(pause);
  unsigned pause_frames = 0;
  for (; pause_frames < 240; ++pause_frames) {
    BkInput input = {0};
    if (pause_frames == 70 || pause_frames == 72)
      input.pressed = BK_BUTTON_DOWN;
    if (pause_frames == 74)
      input.pressed = BK_BUTTON_CONFIRM;
    REQUIRE(bk_audio_poll(services.audio, error));
    REQUIRE(bk_scene_step(pause, 1.0 / 60, &input, error));
    REQUIRE(bk_audio_fill(services.audio, error));
    REQUIRE(bk_renderer_begin(renderer, error));
    REQUIRE(bk_scene_draw(pause, &empty_frame, error));
    REQUIRE(bk_renderer_end(renderer, error));
    REQUIRE(!memcmp(&held, bk_game_preview_state(scene), sizeof(held)));
    if (bk_pause_preview_finished_context(bk_scene_custom_context(pause)))
      break;
  }
  REQUIRE(pause_frames < 240);
  REQUIRE(bk_pause_preview_resume_context(bk_scene_custom_context(pause)));
  REQUIRE(bk_capture_file_read_pause(files, 16 * 1024 * 1024, &pause_bmp,
                                     error) == BK_RESOURCE_MISSING);
  REQUIRE(bk_game_preview_resume(scene, error));
  bk_scene_destroy(pause);
  pause = NULL;
  BkInput empty_input = {0};
  REQUIRE(bk_scene_step(scene, 1.0 / 60, &empty_input, error));
  REQUIRE(bk_renderer_begin(renderer, error));
  REQUIRE(bk_scene_draw(scene, &empty_frame, error));
  REQUIRE(bk_renderer_end(renderer, error));
  REQUIRE(bk_game_preview_state(scene)->hotkeys.menu_request == 0);
  size_t size = 1280 * 720 * 4;
  pixels = malloc(size);
  REQUIRE(pixels && bk_renderer_readback(renderer, pixels, size, error));
  FILE *out = fopen(argv[3], "wb");
  REQUIRE(out);
  int written = fwrite(pixels, 1, size, out) == size;
  int closed = fclose(out) == 0;
  REQUIRE(written && closed);
  printf("PASS game-session frames=%u phase1=%u photos=%d outcome=%u "
         "pause-frames=%u\n",
         total, active_frames, s->hotkeys.photo_count, s->interaction.outcome,
         pause_frames);
  status = 0;
done:
  bk_blob_free(&pause_bmp);
  bk_image_free(&pause_image);
  bk_scene_destroy(pause);
  free(pixels);
  bk_scene_destroy(scene);
  bk_capture_files_destroy(files);
  bk_renderer_destroy(renderer);
  bk_resources_destroy(store);
  return status;
}
