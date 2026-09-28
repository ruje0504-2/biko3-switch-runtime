/* Explicit area-completion fixtures: real menu, retained world reload and GPU.
 * Does not claim the fixtures were reached by natural mission gameplay. */
#include "app/game_preview.h"
#include "app/retry_preview.h"
#include "save/capture_file.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define REQUIRE(x)                                                             \
  do {                                                                         \
    if (!(x)) {                                                                \
      fprintf(stderr, "area-flow line%d: %s\n", __LINE__, error);              \
      goto done;                                                               \
    }                                                                          \
  } while (0)
static int submit(void *p, const int16_t *samples, size_t frames, char e[256]) {
  (void)samples;
  (void)e;
  *(uint64_t *)p += frames;
  return 1;
}
static int poll(void *p, uint64_t *consumed, char e[256]) {
  (void)e;
  *consumed = *(uint64_t *)p;
  return 1;
}
static unsigned rain_frames, snow_frames, dry_frames;
static int weather(BkScene *game, char e[256]) {
  const BkGameFrameState *g = bk_game_preview_state(game);
  BkGameWeatherSnapshot w;
  if (!g || !bk_game_preview_weather(game, &w))
    return 0;
  int rain = g->group == 4 && g->area <= 6;
  int snow = g->group == 0 && g->area <= 6;
  if (w.rain_draw.count != (rain ? 16u : 0u) ||
      w.rain.present != (rain ? 0xffffu : 0u) || w.snow_present != snow ||
      (snow && (!w.snow_instances || w.snow_clock.source <= 0))) {
    snprintf(e, 256, "weather profile mismatch group%u area%u", g->group,
             g->area);
    return 0;
  }
  uint32_t seed = w.random_before;
  for (unsigned i = 0; i < w.rain_draw.count; i++) {
    /* Independent original CRT recurrence; viewport960x720 => scale.75. */
    seed = seed * 214013u + 2531011u;
    unsigned y = ((seed >> 16) & 32767) % 960;
    seed = seed * 214013u + 2531011u;
    int x = (int)(((seed >> 16) & 32767) % 720) - 100;
    if (w.rain_draw.sprites[i].slot != i || w.rain_draw.sprites[i].x != x ||
        w.rain_draw.sprites[i].y != y)
      return 0;
  }
  if (seed != w.random_after)
    return 0;
  rain_frames += rain;
  snow_frames += snow;
  dry_frames += !rain && !snow;
  return 1;
}
static int tick(BkScene *s, BkRenderer *r, BkAudio *a, BkInput in,
                char e[256]) {
  return bk_audio_poll(a, e) && bk_scene_step(s, 1.0 / 60, &in, e) &&
         bk_audio_fill(a, e) && bk_renderer_begin(r, e) &&
         bk_scene_draw(s, &(BkSceneFrame){0}, e) && bk_renderer_end(r, e);
}
typedef struct {
  BkScene *game;
  BkCommonHudState *common;
  unsigned calls;
} Handover;
static int release(void *p, uint8_t flow, char e[256]) {
  Handover *h = p;
  if (flow != 0x20 || h->calls++) {
    snprintf(e, 256, "unexpected release%u", flow);
    return 0;
  }
  if (!bk_game_preview_advance_area(h->game, 2, e))
    return 0;
  bk_common_hud_entry_reset(h->common);
  return 1;
}
int main(int argc, char **argv) {
  if (argc != 4) {
    fprintf(stderr, "usage: area-flow-probe DATA CAPTURES OUTPUT.rgba\n");
    return 2;
  }
  char error[256] = {0}, path[2048];
  int status = 1;
  unsigned transitions = 0, frames = 0;
  BkResourceStore *store = bk_resources_create(error);
  BkRenderer *renderer = NULL;
  BkCaptureFiles *files = NULL;
  BkAudio *audio = NULL;
  BkScene *game = NULL, *menu = NULL;
  uint64_t submitted = 0;
  uint8_t *pixels = NULL;
  REQUIRE(store);
  const char *packs[] = {"bk3_00", "bk3_01", "bk3_02", "bk3_03",
                         "bk3_04", "bk3_05", "bk3_06", "bk3_07",
                         "bk3_15", "bk3_16", "bk3_20"};
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
  BkAudioSink sink = {&submitted, 48000, 240, 960, submit, poll};
  audio = bk_audio_create(&sink, error);
  REQUIRE(renderer && files && audio);
  BkSceneServices services = {store, renderer, stderr, audio, files};
  for (unsigned group = 0; group < 5; ++group) {
    BkGameFrameState state;
    BkEntryProgress progress;
    REQUIRE(bk_scene_game_frame_boot_state(&state, &progress, 123));
    game = bk_game_preview_create_entry(&services, &state, &progress, group, 0,
                                        8, 1, error);
    REQUIRE(game);
    for (unsigned i = 0; i < 4; ++i)
      REQUIRE(tick(game, renderer, audio, (BkInput){0}, error));
    REQUIRE(weather(game, error));
    BkCommonHudState common;
    bk_common_hud_initialize(&common);
    BkMenuCursor cursor = {0};
    REQUIRE(bk_menu_cursor_initialize(&cursor, 960, 720));
    BkCheckpointPrompt prompt = {0};
    uint8_t overlay = 0, hover = 0;
    for (unsigned area = 1; area < 9; ++area) {
      REQUIRE(bk_game_preview_suspend_area(game, error));
      BkFlowTransition flow = {0x20, 2, 0x20, 2};
      common.curtain = (BkFadeSprite){1, 2, 3};
      common.blocked = 0;
      BkPauseBindings bindings = {&common, &flow, &cursor, &overlay, &hover};
      Handover handover = {game, &common, 0};
      BkFlowTransitionOps ops = {&handover, release};
      menu = bk_checkpoint_preview_create(
          &services, &prompt, &bindings, &ops, &state.camera.phase,
          bk_game_preview_hud_reserve(game), 1 + frames / 60.0, error);
      REQUIRE(menu);
      for (unsigned i = 0; i < 90; ++i) {
        REQUIRE(tick(menu, renderer, audio, (BkInput){0}, error));
        ++frames;
      }
      REQUIRE(tick(menu, renderer, audio, (BkInput){.pressed = BK_BUTTON_RIGHT},
                   error));
      REQUIRE(tick(menu, renderer, audio, (BkInput){0}, error));
      REQUIRE(tick(menu, renderer, audio,
                   (BkInput){.pressed = BK_BUTTON_CONFIRM}, error));
      unsigned wait = 0;
      for (; wait < 120 && flow.current == 0x20; ++wait) {
        REQUIRE(tick(menu, renderer, audio, (BkInput){0}, error));
        ++frames;
      }
      REQUIRE(flow.current == 0x50 && flow.target == 2 && flow.mode == 3 &&
              handover.calls == 1);
      REQUIRE(state.area == area && *bk_game_preview_hud_reserve(game) == 1);
      REQUIRE(state.camera.phase == (area == 8 ? 3 : 2));
      bk_scene_destroy(menu);
      menu = NULL;
      for (unsigned i = 0; i < 8; ++i) {
        REQUIRE(tick(game, renderer, audio, (BkInput){0}, error));
        ++frames;
        REQUIRE(weather(game, error));
      }
      BkGameWeatherSnapshot held_weather, redrawn_weather;
      REQUIRE(bk_game_preview_weather(game, &held_weather));
      BkGameFrameState held_state = state;
      REQUIRE(bk_renderer_begin(renderer, error) &&
              bk_scene_draw(game, &(BkSceneFrame){0}, error) &&
              bk_renderer_end(renderer, error));
      REQUIRE(bk_game_preview_weather(game, &redrawn_weather) &&
              !memcmp(&held_weather, &redrawn_weather, sizeof(held_weather)) &&
              !memcmp(&held_state, &state, sizeof(state)));
      REQUIRE(state.camera.phase == (area == 8 ? 1 : 2));
      ++transitions;
      fprintf(stderr, "area fixture group%u area%u ready\n", group, area);
      if (group == 0 && area == 1) {
        pixels = malloc(1280 * 720 * 4);
        REQUIRE(pixels &&
                bk_renderer_readback(renderer, pixels, 1280 * 720 * 4, error));
        FILE *out = fopen(argv[3], "wb");
        REQUIRE(out);
        size_t written = fwrite(pixels, 1, 1280 * 720 * 4, out);
        int closed = fclose(out);
        REQUIRE(written == 1280 * 720 * 4 && closed == 0);
        free(pixels);
        pixels = NULL;
      }
    }
    bk_scene_destroy(game);
    game = NULL;
    for (unsigned voice = 0; voice < BK_AUDIO_VOICES; ++voice)
      REQUIRE(bk_audio_clear(audio, voice, error));
  }
  printf("PASS area-flow fixtures=40 real-menu-handover=%u frames=%u "
         "retained-actors; no natural-mission claim\n",
         transitions, frames);
  printf("PASS weather rain=%u snow=%u dry=%u shared-RNG/area-reload/redraw\n",
         rain_frames, snow_frames, dry_frames);
  status = 0;
done:
  free(pixels);
  bk_scene_destroy(menu);
  bk_scene_destroy(game);
  bk_audio_destroy(audio);
  bk_capture_files_destroy(files);
  bk_renderer_destroy(renderer);
  bk_resources_destroy(store);
  return status;
}
