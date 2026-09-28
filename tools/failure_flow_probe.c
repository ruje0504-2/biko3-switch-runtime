/* Real failure -> retry -> gameplay -> failure -> title. BK_CAR_IMPACT
 * explicitly injects the post-contact trigger; otherwise input only. */
#include "app/play_session.h"
#include "core/game_clock.h"
#include <math.h>
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
    if (!bk_audio_poll(audio, e) ||
        (clock.seconds > 0 &&
         !bk_play_session_step_at(scene, clock.seconds, 1 + now / 1000.0,
                                  &input, e)))
      return 0;
    return present(scene, renderer, audio, e);
  }
  return bk_audio_poll(audio, e) && bk_scene_step(scene, 1.0 / 60, &input, e) &&
         present(scene, renderer, audio, e);
}
int main(int argc, char **argv) {
  if (argc != 4) {
    fprintf(stderr, "usage: failure-flow-probe DATA CAPTURES OUTPUT.rgba\n");
    return 2;
  }
  char error[256] = {0}, path[2048];
  int status = 1;
  BkRenderStats empty_renderer = {0};
  BkResourceStore *store = bk_resources_create(error);
  BkRenderer *renderer = NULL;
  BkCaptureFiles *files = NULL;
  BkScene *scene = NULL;
  BkAudio *audio = NULL;
  uint64_t submitted = 0;
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
  empty_renderer = bk_renderer_stats(renderer);
  files = bk_capture_files_create(argv[2], error);
  REQUIRE(renderer && files);
  BkAudioSink sink = {&submitted, 48000,          240,
                      960,        offline_submit, offline_poll};
  audio = bk_audio_create(&sink, error);
  REQUIRE(audio);
  BkSceneServices services = {store, renderer, stderr, audio, files};
  scene = bk_play_session_create_development(&services, NULL, error);
  REQUIRE(scene && present(scene, renderer, audio, error));
  const BkCommonHudState *common = bk_play_session_common(scene);
  REQUIRE(common->curtain.alpha == 1);
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
  unsigned gameplay_frames[2] = {0}, menu_frames[2] = {0};
  uint8_t outcomes[2] = {0};
  unsigned retry_opening = 0;
  pixels = malloc(1280 * 720 * 4);
  REQUIRE(pixels);
  for (unsigned choice = 0; choice < 2; ++choice) {
    if (getenv("BK_CAR_IMPACT")) {
      /* Explicit post-contact fixture from4f4306: real script/animation,
       * failure camera/reload/HUD and retry remain fully connected. */
      BkGameFrameState *g = (BkGameFrameState *)bk_play_session_state(scene);
      BkPlayerTrigger *t = &g->player.interaction.trigger;
      memcpy(t->origin, g->player.spatial.movement.position, 12);
      memcpy(t->target, t->origin, 12);
      t->origin[3] = t->target[3] = choice ? 180 : 270;
      t->target[choice ? 2 : 0] += 100;
      g->player.spatial.movement.action = g->player_actions[10];
      g->player.completion_mode = g->player.interaction.script_phase = 1;
      fprintf(stderr, "Injected car post-contact trigger choice%u action%d\n",
              choice, g->player.spatial.movement.action);
    }
    unsigned *played = &gameplay_frames[choice];
    for (; *played < 6000 && bk_play_session_flow(scene)->current != 0x40;
         ++*played) {
      REQUIRE(
          tick(scene, renderer, audio, (BkInput){.held = BK_BUTTON_UP}, error));
      if (*played % 600 == 0)
        fprintf(stderr, "failure%u wait frame%u flow%u outcome%u\n",
                choice, *played, bk_play_session_flow(scene)->current,
                bk_play_session_state(scene)->interaction.outcome);
    }
    REQUIRE(bk_play_session_flow(scene)->current == 0x40);
    outcomes[choice] = bk_play_session_state(scene)->interaction.outcome;
    if (getenv("BK_CAR_IMPACT"))
      REQUIRE(outcomes[choice] == 1);
    REQUIRE(outcomes[choice] >= 1 && outcomes[choice] <= 6);
    for (unsigned i = 0; i < 720; ++i)
      REQUIRE(tick(scene, renderer, audio, (BkInput){0}, error));
    REQUIRE(bk_play_session_flow(scene)->current == 0x40);
    REQUIRE(bk_play_session_common(scene)->curtain.alpha == 0);
    if (!choice) {
      REQUIRE(bk_renderer_readback(renderer, pixels, 1280 * 720 * 4, error));
      FILE *output = fopen(argv[3], "wb");
      REQUIRE(output);
      size_t written = fwrite(pixels, 1, 1280 * 720 * 4, output);
      int closed = fclose(output);
      REQUIRE(written == 1280 * 720 * 4 && closed == 0);
    }
    unsigned i = 0;
    for (; i < 360 && bk_play_session_flow(scene)->current != 0x68; ++i) {
      BkInput in = {0};
      if (bk_play_session_flow(scene)->current == 0x40 && i % 90 == 0)
        in.pressed = BK_BUTTON_CONFIRM;
      REQUIRE(tick(scene, renderer, audio, in, error));
    }
    REQUIRE(bk_play_session_flow(scene)->current == 0x68);
    REQUIRE(bk_play_session_state(scene)->interaction.outcome == 0);
    for (i = 0; i < 90; ++i)
      REQUIRE(tick(scene, renderer, audio, (BkInput){0}, error));
    REQUIRE(tick(scene, renderer, audio,
                 (BkInput){.pressed = BK_BUTTON_CONFIRM}, error));
    for (i = 0; i < 90; ++i)
      REQUIRE(tick(scene, renderer, audio, (BkInput){0}, error));
    if (choice)
      REQUIRE(tick(scene, renderer, audio,
                   (BkInput){.pressed = BK_BUTTON_RIGHT}, error));
    REQUIRE(tick(scene, renderer, audio, (BkInput){0}, error));
    if (!choice) {
      REQUIRE(bk_renderer_readback(renderer, pixels, 1280 * 720 * 4, error));
      REQUIRE(snprintf(path, sizeof(path), "%s.retry.rgba", argv[3]) <
              (int)sizeof(path));
      FILE *output = fopen(path, "wb");
      REQUIRE(output);
      size_t written = fwrite(pixels, 1, 1280 * 720 * 4, output);
      int closed = fclose(output);
      REQUIRE(written == 1280 * 720 * 4 && closed == 0);
    }
    REQUIRE(tick(scene, renderer, audio,
                 (BkInput){.pressed = BK_BUTTON_CONFIRM}, error));
    unsigned *menu = &menu_frames[choice];
    for (; *menu < 360 &&
           bk_play_session_flow(scene)->current != (choice ? 1 : 2);
         ++*menu)
      REQUIRE(tick(scene, renderer, audio, (BkInput){0}, error));
    REQUIRE(bk_play_session_flow(scene)->current == (choice ? 1 : 2));
    if (!choice) {
      REQUIRE(bk_play_session_flow(scene)->previous == 0x68);
      REQUIRE(bk_play_session_state(scene)->camera.phase == 2);
      for (; retry_opening < 3600 &&
             bk_play_session_state(scene)->camera.phase != 1;
           ++retry_opening) {
        BkInput in = {0};
        if (retry_opening % 30 == 29)
          in.pressed = BK_BUTTON_CONFIRM;
        REQUIRE(tick(scene, renderer, audio, in, error));
      }
      REQUIRE(retry_opening < 3600 &&
              bk_play_session_state(scene)->interaction.outcome == 0);
    } else {
      for (i = 0; i < 90; ++i)
        REQUIRE(tick(scene, renderer, audio, (BkInput){0}, error));
    }
  }
  printf("PASS %s-failure-retry opening=%u game=%u/%u outcomes=%u/%u "
         "menu=%u/%u retry-opening=%u final=title\n",
         getenv("BK_CAR_IMPACT") ? "car-contact-fixture" : "natural",
         opening_frames, gameplay_frames[0], gameplay_frames[1], outcomes[0],
         outcomes[1], menu_frames[0], menu_frames[1], retry_opening);
  status = 0;
done:
  free(pixels);
  bk_scene_destroy(scene);
  if (!status) {
    BkRenderStats end = bk_renderer_stats(renderer);
    if (end.live_allocations != empty_renderer.live_allocations ||
        end.live_bytes != empty_renderer.live_bytes) {
      fprintf(stderr, "failure-flow GPU allocation leak after scene teardown\n");
      status = 1;
    } else
      fprintf(stderr, "PASS GPU allocations returned to renderer baseline\n");
  }
  bk_audio_destroy(audio);
  bk_capture_files_destroy(files);
  bk_renderer_destroy(renderer);
  bk_resources_destroy(store);
  return status;
}
