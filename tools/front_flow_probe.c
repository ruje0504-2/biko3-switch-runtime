/* Original title/selection/dialogue/game/pause/title application flow.
 * Only input and wall-clock replay; no game/UI state writes.
 */
#include "app/play_session.h"
#include "core/game_clock.h"
#include "save/capture_file.h"
#include <math.h>
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
static int offline_submit(void *p, const int16_t *samples, size_t frames,
                          char e[256]) {
  (void)samples;
  (void)e;
  *(uint64_t *)p += frames;
  return 1;
}
static int offline_poll(void *p, uint64_t *consumed, char e[256]) {
  (void)e;
  *consumed = *(uint64_t *)p;
  return 1;
}
static int present(BkScene *scene, BkRenderer *renderer, BkAudio *audio,
                   char e[256]) {
  return bk_audio_fill(audio, e) && bk_renderer_begin(renderer, e) &&
         bk_scene_draw(scene, &(BkSceneFrame){0}, e) &&
         bk_renderer_end(renderer, e) &&
         bk_play_session_after_present(scene, e);
}
static int tick(BkScene *scene, BkRenderer *renderer, BkAudio *audio,
                BkInput input, char e[256]) {
  if (getenv("BK_NATIVE_GAME_CLOCK")) {
    static BkGameClock clock;
    static unsigned frames;
    unsigned now = (unsigned)lround(++frames * 1000.0 / 60);
    bk_game_clock_poll(&clock, now, now);
    double wall = 1 + now / 1000.0;
    if (!bk_audio_poll(audio, e) ||
        (clock.seconds > 0 &&
         (!bk_play_session_step_at(scene, clock.seconds, wall, &input, e) ||
          fabs(bk_play_session_wall_seconds(scene) - wall) > 1e-12)))
      return 0;
    return present(scene, renderer, audio, e);
  }
  return bk_audio_poll(audio, e) && bk_scene_step(scene, 1.0 / 60, &input, e) &&
         present(scene, renderer, audio, e);
}
int main(int argc, char **argv) {
  if (argc != 4) {
    fprintf(stderr, "usage: front-flow-probe DATA CAPTURES OUTPUT.rgba\n");
    return 2;
  }
  int controller = getenv("BK_FRONT_CONTROLLER") != NULL;
  unsigned target_group = getenv("BK_FRONT_RAIN") ? 4 : 0;
  if (target_group)
    controller = 0; /* Authored pointer targets for this additional profile. */
  char error[256] = {0}, path[2048];
  int status = 1;
  BkResourceStore *store = bk_resources_create(error);
  BkRenderer *renderer = NULL;
  BkCaptureFiles *files = NULL;
  BkScene *scene = NULL;
  BkAudio *audio = NULL;
  BkUnlockFile *unlock_file = NULL;
  uint64_t submitted = 0;
  BkBlob pause_bmp = {0};
  BkImage pause_image = {0};
  uint8_t *pixels = NULL;
  REQUIRE(store);
  const char *packs[] = {"bk3_00", "bk3_01", "bk3_02", "bk3_03",
                         "bk3_04", "bk3_05", "bk3_06", "bk3_07",
                         "bk3_15", "bk3_16", "bk3_20", "bk3_18"};
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
  BkAudioSink sink = {&submitted, 48000,          240,
                      960,        offline_submit, offline_poll};
  audio = bk_audio_create(&sink, error);
  REQUIRE(audio);
  BkSceneServices services = {store, renderer, stderr, audio, files, NULL};
  if (getenv("BK_FRONT_UNLOCKS")) {
    /* Explicit fixture of already completed gallery flags, never a runtime
     * unlock shortcut. Reopen the file owner before creating the session. */
    REQUIRE(snprintf(path, sizeof(path), "%s/unlock-fixture", argv[2]) <
            (int)sizeof(path));
    unlock_file = bk_unlock_file_create(path, error);
    REQUIRE(unlock_file);
    BkUnlockTable saved, read;
    const uint8_t flags[8] = {1, 2, 128, 255, 3, 4, 5, 6};
    for (unsigned group = 0; group < 5; group++)
      REQUIRE(bk_unlock_file_store(unlock_file, group, flags, &saved, error));
    bk_unlock_file_destroy(unlock_file);
    unlock_file = bk_unlock_file_create(path, error);
    REQUIRE(unlock_file &&
            bk_unlock_file_read(unlock_file, &read, error) == BK_RESOURCE_OK &&
            !memcmp(&saved, &read, sizeof(saved)));
  }
  scene =
      bk_play_session_create_with_storage(&services, NULL, unlock_file, error);
  REQUIRE(scene && present(scene, renderer, audio, error));
  const BkCommonHudState *common = bk_play_session_common(scene);
  REQUIRE(bk_play_session_flow(scene)->current == 1);
  /* Authored title new-game rectangle in the960x720 centered viewport. */
  unsigned original_title = 0, select_frames = 0, dialogue_frames = 0,
           loading_frames = 0;
  for (; original_title < 600 && bk_play_session_flow(scene)->current == 1;
       original_title++) {
    BkInput in = {.pointer_active = 1,
                  .pointer_x = 160 + 1116 * .75f,
                  .pointer_y = 53 * .75f};
    if (controller) {
      in.pointer_active = 0;
      if (original_title == 140)
        in.pressed = BK_BUTTON_DOWN;
      if (original_title == 142)
        in.pressed = BK_BUTTON_UP;
    }
    if (original_title > 160 && original_title % 20 == 19)
      in.pressed = BK_BUTTON_CONFIRM;
    REQUIRE(tick(scene, renderer, audio, in, error));
  }
  REQUIRE(bk_play_session_flow(scene)->current == 0x50);
  while (bk_play_session_flow(scene)->current == 0x50 && loading_frames++ < 600)
    REQUIRE(tick(scene, renderer, audio,
                 (BkInput){.pressed = BK_BUTTON_CONFIRM}, error));
  REQUIRE(bk_play_session_flow(scene)->current == 0x38);
  for (; select_frames < 600 && bk_play_session_flow(scene)->current == 0x38;
       select_frames++) {
    /* Click all five authored character selectors, ending on group0. */
    unsigned segment = select_frames / 40,
             group = segment < 4 ? segment + 1 : target_group;
    BkInput in = {.pointer_active = 1};
    if (select_frames < 200) {
      in.pointer_x = 160 + 56 * .75f;
      in.pointer_y = (124 + 67 * group) * .75f;
      if (select_frames % 40 == 24)
        in.pressed = BK_BUTTON_CONFIRM;
    } else {
      in.pointer_x = 160 + 832 * .75f;
      in.pointer_y = 908 * .75f;
      if (select_frames % 20 == 19)
        in.pressed = BK_BUTTON_CONFIRM;
    }
    if (controller) {
      in = (BkInput){0};
      if (select_frames < 200) {
        if (select_frames % 40 == 10 ||
            (segment == 4 && select_frames % 40 == 12))
          in.pressed = BK_BUTTON_DOWN;
        if (select_frames % 40 == 24)
          in.pressed = BK_BUTTON_CONFIRM;
      } else {
        if (select_frames == 210)
          in.pressed = BK_BUTTON_UP;
        /* Native navigation skips the empty column1 in the info layout. */
        if (select_frames == 214)
          in.pressed = BK_BUTTON_RIGHT;
        if (select_frames >= 239 && select_frames % 20 == 19)
          in.pressed = BK_BUTTON_CONFIRM;
      }
    }
    REQUIRE(tick(scene, renderer, audio, in, error));
  }
  REQUIRE(bk_play_session_flow(scene)->current == 0x50 &&
          bk_play_session_state(scene)->group == target_group);
  while (bk_play_session_flow(scene)->current == 0x50 &&
         loading_frames++ < 1200)
    REQUIRE(tick(scene, renderer, audio,
                 (BkInput){.pressed = BK_BUTTON_CONFIRM}, error));
  REQUIRE(bk_play_session_flow(scene)->current == 8);
  for (; dialogue_frames < 10000 && bk_play_session_flow(scene)->current == 8;
       dialogue_frames++) {
    BkInput in = {0};
    if (dialogue_frames % 6 == 5)
      in.pressed = BK_BUTTON_CONFIRM;
    REQUIRE(tick(scene, renderer, audio, in, error));
  }
  REQUIRE(bk_play_session_flow(scene)->current == 0x50);
  while (bk_play_session_flow(scene)->current == 0x50 &&
         loading_frames++ < 1800)
    REQUIRE(tick(scene, renderer, audio, (BkInput){0}, error));
  REQUIRE(bk_play_session_flow(scene)->current == 2 &&
          bk_play_session_flow(scene)->previous == 8);

  unsigned opening_frames = 0;
  for (;
       opening_frames < 3600 && bk_play_session_state(scene)->camera.phase != 1;
       ++opening_frames) {
    BkInput input = {0};
    if (opening_frames % 30 == 29)
      input.pressed = BK_BUTTON_CONFIRM;
    REQUIRE(tick(scene, renderer, audio, input, error));
  }
  REQUIRE(opening_frames < 3600 && common->curtain.alpha == 0);
  for (unsigned i = 0; i < 60; ++i) {
    BkInput input = {0};
    if (i == 3 || i == 30)
      input.pressed = BK_BUTTON_PHOTO;
    if (i == 10 || i == 20)
      input.pressed = BK_BUTTON_GAME_CAMERA;
    if (i >= 35 && i < 50)
      input.held = BK_BUTTON_UP;
    REQUIRE(tick(scene, renderer, audio, input, error));
  }
  REQUIRE(bk_play_session_state(scene)->hotkeys.photo_count == 2);
  REQUIRE(tick(scene, renderer, audio, (BkInput){.pressed = BK_BUTTON_PAUSE},
               error));
  REQUIRE(bk_play_session_flow(scene)->current == 4);
  REQUIRE(bk_play_session_state(scene)->hotkeys.menu_request == 0);
  REQUIRE(bk_capture_file_read_pause(files, 16 * 1024 * 1024, &pause_bmp,
                                     error) == BK_RESOURCE_OK);
  REQUIRE(bk_image_decode(pause_bmp.data, pause_bmp.size, &pause_image, error));
  REQUIRE(pause_image.width == 960 && pause_image.height == 720);
  bk_blob_free(&pause_bmp);
  bk_image_free(&pause_image);
  BkGameFrameState held = *bk_play_session_state(scene);
  BkCommonHudState common_held = *common;
  /* A zero-update redraw must neither recreate pause nor advance the curtain.
   */
  REQUIRE(present(scene, renderer, audio, error));
  REQUIRE(!memcmp(common, &common_held, sizeof(common_held)));
  unsigned pause_frames = 0;
  for (; pause_frames < 240 && bk_play_session_flow(scene)->current == 4;
       ++pause_frames) {
    BkInput input = {0};
    if (pause_frames == 70 || pause_frames == 72)
      input.pressed = BK_BUTTON_DOWN;
    if (pause_frames == 74)
      input.pressed = BK_BUTTON_CONFIRM;
    REQUIRE(tick(scene, renderer, audio, input, error));
    /* Native51a77c changes only background music volume. Timer, random
     * state and every other gameplay byte must remain suspended. */
    BkGameFrameState paused_state = *bk_play_session_state(scene);
    paused_state.background.music_volume = held.background.music_volume;
    REQUIRE(!memcmp(&held, &paused_state, sizeof(held)));
    REQUIRE(common == bk_play_session_common(scene));
  }
  REQUIRE(bk_play_session_flow(scene)->current == 2);
  REQUIRE(bk_capture_file_read_pause(files, 16 * 1024 * 1024, &pause_bmp,
                                     error) == BK_RESOURCE_MISSING);
  REQUIRE(tick(scene, renderer, audio, (BkInput){0}, error));
  REQUIRE(tick(scene, renderer, audio, (BkInput){.pressed = BK_BUTTON_PAUSE},
               error));
  REQUIRE(bk_play_session_flow(scene)->current == 4);
  unsigned title_frames = 0;
  int saw_loading = 0;
  for (; title_frames < 260 && bk_play_session_flow(scene)->current != 1;
       ++title_frames) {
    BkInput input = {0};
    /* The native row survives menu destruction: prior resume left it at2. */
    if (title_frames == 70)
      input.pressed = BK_BUTTON_DOWN;
    if (title_frames == 72 || title_frames == 76)
      input.pressed = BK_BUTTON_CONFIRM;
    if (title_frames == 74)
      input.pressed = BK_BUTTON_LEFT;
    REQUIRE(tick(scene, renderer, audio, input, error));
    saw_loading |= bk_play_session_flow(scene)->current == 0x50;
  }
  REQUIRE(saw_loading && bk_play_session_flow(scene)->current == 1);
  REQUIRE(tick(scene, renderer, audio, (BkInput){0}, error));
  for (unsigned voice = 0; voice <= 38; ++voice) {
    int playing = 1;
    REQUIRE(bk_audio_playing(audio, voice, &playing) && !playing);
  }
  REQUIRE(bk_capture_file_read_pause(files, 16 * 1024 * 1024, &pause_bmp,
                                     error) == BK_RESOURCE_MISSING);
  REQUIRE(bk_play_session_state(scene)->hotkeys.photos[target_group] == 2);
  for (unsigned i = 0; i < 150; i++)
    REQUIRE(tick(scene, renderer, audio, (BkInput){0}, error));
  size_t size = 1280 * 720 * 4;
  pixels = malloc(size);
  REQUIRE(pixels && bk_renderer_readback(renderer, pixels, size, error));
  FILE *out = fopen(argv[3], "wb");
  REQUIRE(out);
  int written = fwrite(pixels, 1, size, out) == size;
  int closed = fclose(out) == 0;
  REQUIRE(written && closed);
  printf("PASS original-front-flow title=%u selection=%u dialogue=%u "
         "loading=%u opening=%u pause=%u return-title=%u photos=%d\n",
         original_title, select_frames, dialogue_frames, loading_frames,
         opening_frames, pause_frames, title_frames,
         bk_play_session_state(scene)->hotkeys.photo_count);
  status = 0;
done:
  bk_blob_free(&pause_bmp);
  bk_image_free(&pause_image);
  free(pixels);
  bk_scene_destroy(scene);
  bk_unlock_file_destroy(unlock_file);
  bk_audio_destroy(audio);
  bk_capture_files_destroy(files);
  bk_renderer_destroy(renderer);
  bk_resources_destroy(store);
  return status;
}
