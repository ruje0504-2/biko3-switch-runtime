/* Area completion/inventory are explicit boundary fixtures. Every subsequent
 * prompt/save/pause/load/return uses real app input, resources, files and GPU.
 * This is a lifecycle fixture, not a claim of natural mission completion. */
#include "app/play_session.h"
#include "core/game_clock.h"
#include "save/capture_file.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x)                                                               \
  do {                                                                         \
    if (!(x)) {                                                                \
      fprintf(stderr, "save-flow line%d: %s (%s)\n", __LINE__, #x, e);         \
      goto done;                                                               \
    }                                                                          \
  } while (0)
static unsigned frames;
static int submit(void *p, const int16_t *v, size_t n, char e[256]) {
  (void)v;
  (void)e;
  *(uint64_t *)p += n;
  return 1;
}
static int poll(void *p, uint64_t *n, char e[256]) {
  (void)e;
  *n = *(uint64_t *)p;
  return 1;
}
typedef struct {
  BkScene *scene;
  BkRenderer *renderer;
  BkAudio *audio;
} Run;
static int present(Run *r, char e[256]) {
  return bk_audio_fill(r->audio, e) && bk_renderer_begin(r->renderer, e) &&
         bk_scene_draw(r->scene, &(BkSceneFrame){0}, e) &&
         bk_renderer_end(r->renderer, e) &&
         bk_play_session_after_present(r->scene, e);
}
static int tick(Run *r, BkInput in, char e[256]) {
  ++frames;
  if (getenv("BK_NATIVE_GAME_CLOCK")) {
    static BkGameClock clock;
    unsigned now = (unsigned)lround(frames * 1000.0 / 60);
    bk_game_clock_poll(&clock, now, now);
    double wall = 1 + now / 1000.0;
    if (!bk_audio_poll(r->audio, e) ||
        (clock.seconds > 0 &&
         (!bk_play_session_step_at(r->scene, clock.seconds, wall, &in, e) ||
          fabs(bk_play_session_wall_seconds(r->scene) - wall) > 1e-12)))
      return 0;
    return present(r, e);
  }
  return bk_audio_poll(r->audio, e) &&
         bk_scene_step(r->scene, 1. / 60, &in, e) && present(r, e);
}
static int wait_flow(Run *r, uint8_t flow, char e[256]) {
  for (unsigned i = 0;
       i < 300 && bk_play_session_flow(r->scene)->current != flow; ++i)
    if (!tick(r, (BkInput){0}, e))
      return 0;
  if (bk_play_session_flow(r->scene)->current == flow)
    return 1;
  snprintf(e, 256, "waiting flow%x got%x", flow,
           bk_play_session_flow(r->scene)->current);
  return 0;
}
static int ready(Run *r, char e[256]) {
  for (unsigned i = 0;
       i < 3600 && (bk_play_session_state(r->scene)->camera.phase != 1 ||
                    bk_play_session_common(r->scene)->curtain.alpha != 0);
       ++i)
    if (!tick(r, (BkInput){.pressed = i % 30 == 29 ? BK_BUTTON_CONFIRM : 0}, e))
      return 0;
  return bk_play_session_flow(r->scene)->current == 2 &&
         bk_play_session_state(r->scene)->camera.phase == 1 &&
         bk_play_session_common(r->scene)->curtain.alpha == 0;
}
static int click(Run *r, float x, float y, char e[256]) {
  BkInput in = {
      .pointer_active = 1, .pointer_x = 160 + x * .75f, .pointer_y = y * .75f};
  if (!tick(r, in, e))
    return 0;
  in.pressed = BK_BUTTON_CONFIRM;
  return tick(r, in, e);
}
static int settle(Run *r, char e[256]) {
  for (unsigned i = 0; i < 75; ++i)
    if (!tick(r, (BkInput){0}, e))
      return 0;
  return 1;
}
/* All eight real checkpoint menus and subsequent opening/tracking updates.
 * Only each area's completion response is a boundary fixture. The initial
 * cross-character load is an explicit checkpoint file fixture. */
static int chain(Run *r, BkCheckpointFiles *files, unsigned group, char e[256]) {
#define CHAIN(x)                                                               \
  do {                                                                        \
    if (!(x)) {                                                               \
      fprintf(stderr, "save chain group%u area%u line%d: %s (%s)\n", group,    \
              bk_play_session_state(r->scene)->area, __LINE__, #x, e);         \
      return 0;                                                               \
    }                                                                         \
  } while (0)
  BkCheckpointBank bank;
  if (group) {
    const uint8_t items[8] = {0};
    BkCheckpointTime stamp = {2026, 9, 30, 12, 0, 0};
    CHAIN(bk_checkpoint_file_store(files, group, 0, 0, items, &stamp,
                                    123, &bank, e));
    CHAIN(tick(r, (BkInput){.pressed = BK_BUTTON_PAUSE}, e));
    CHAIN(wait_flow(r, 4, e) && settle(r, e));
    CHAIN(click(r, 640, 336, e) && wait_flow(r, 0x28, e) && settle(r, e));
    CHAIN(click(r, 514 + 168 * group, 170, e) && click(r, 750, 250, e) &&
          click(r, 496, 548, e));
    CHAIN(wait_flow(r, 2, e) && ready(r, e));
  }
  CHAIN(bk_play_session_state(r->scene)->group == group &&
        bk_play_session_state(r->scene)->area == 0);
  for (unsigned area = 1; area < 9; ++area) {
    BkGameFrameState *fixture = (BkGameFrameState *)bk_play_session_state(r->scene);
    fixture->interaction.response = 1;
    CHAIN(wait_flow(r, 0x20, e) && settle(r, e));
    CHAIN(click(r, 496, 548, e) && wait_flow(r, 0x28, e));
    CHAIN(bk_play_session_state(r->scene)->area == area && settle(r, e));
    CHAIN(click(r, 750, 250, e) && click(r, 496, 548, e));
    CHAIN(bk_checkpoint_file_read(files, group, &bank, e) == BK_RESOURCE_OK);
    CHAIN(bank.slots[0].area == area && bank.slots[0].stamp[0]);
    fprintf(stderr, "CHECKPOINT_STORED group%u area%u last_crossed%u\n",
            group, area, bk_play_session_state(r->scene)->npc.path.last_crossed);
    CHAIN(click(r, 1100, 908, e) && wait_flow(r, 2, e) && ready(r, e));
    for (unsigned i = 0; i < 30; ++i) CHAIN(tick(r, (BkInput){0}, e));
    CHAIN(bk_play_session_flow(r->scene)->current == 2 &&
          bk_play_session_state(r->scene)->area == area &&
          bk_play_session_state(r->scene)->camera.phase == 1);
    fprintf(stderr, "CHECKPOINT_CHAIN group%u area%u saved opening+tracking complete frames%u\n",
            group, area, frames);
    fflush(stderr);
  }
  printf("PASS checkpoint chain group%u saves8 openings8 frames%u\n", group, frames);
  return 1;
#undef CHAIN
}
int main(int argc, char **argv) {
  if (argc != 4) {
    fprintf(stderr, "save-flow-probe DATA OUTPUT_ROOT OUTPUT.rgba\n");
    return 2;
  }
  char e[256] = {0}, path[2048];
  int status = 1;
  BkRenderStats empty_renderer = {0};
  BkResourceStore *resources = bk_resources_create(e);
  BkCaptureFiles *captures = NULL;
  BkCheckpointFiles *files = NULL;
  Run run = {0};
  uint64_t submitted = 0;
  uint8_t *pixels = NULL;
  CHECK(resources);
  const char *packs[] = {"bk3_00", "bk3_01", "bk3_02", "bk3_03", "bk3_04",
                         "bk3_05", "bk3_06", "bk3_07", "bk3_15", "bk3_16", "bk3_20"};
  for (unsigned i = 0; i < sizeof(packs) / sizeof(packs[0]); ++i) {
    snprintf(path, sizeof(path), "%s/%s.pp", argv[1], packs[i]);
    CHECK(bk_resources_mount(resources, packs[i], path, e));
  }
  const char *loose[] = {"routes", "faces", "collision", "fonts"};
  for (unsigned i = 0; i < 4; ++i)
    CHECK(bk_resources_mount_directory(resources, loose[i], argv[1],
                                       16 * 1024 * 1024, e));
  captures = bk_capture_files_create(argv[2], e);
  files = bk_checkpoint_files_create(argv[2], e);
  run.renderer = bk_renderer_create(1280, 720, stderr, e);
  empty_renderer = bk_renderer_stats(run.renderer);
  BkAudioSink sink = {&submitted, 48000, 240, 960, submit, poll};
  run.audio = bk_audio_create(&sink, e);
  CHECK(captures && files && run.renderer && run.audio);
  BkSceneServices services = {resources, run.renderer, stderr, run.audio,
                              captures};
  run.scene = bk_play_session_create_development(&services, files, e);
  CHECK(run.scene && present(&run, e));
  CHECK(ready(&run, e));
  if (getenv("BK_SAVE_CHAIN_GROUP")) {
    char *end;
    unsigned long group = strtoul(getenv("BK_SAVE_CHAIN_GROUP"), &end, 10);
    CHECK(!*end && group < 5 && chain(&run, files, (unsigned)group, e));
    status = 0;
    goto done;
  }
  /* Only explicit fixture writes: completed first area and collected items. */
  BkGameFrameState *fixture =
      (BkGameFrameState *)bk_play_session_state(run.scene);
  const uint8_t inventory[8] = {1, 0, 1, 0, 1, 0, 0, 0};
  memcpy(fixture->pickup.collected, inventory, 5);
  fixture->interaction.response = 1;
  CHECK(wait_flow(&run, 0x20, e));
  CHECK(settle(&run, e));
  CHECK(click(&run, 496, 548, e) && wait_flow(&run, 0x28, e));
  CHECK(bk_play_session_state(run.scene)->area == 1 &&
        bk_play_session_state(run.scene)->camera.phase == 2);
  CHECK(settle(&run, e));
  CHECK(click(&run, 750, 250, e) && click(&run, 496, 548, e));
  BkCheckpointBank bank;
  CHECK(bk_checkpoint_file_read(files, 0, &bank, e) == BK_RESOURCE_OK);
  CHECK(bank.slots[0].area == 1 &&
        !memcmp(bank.slots[0].inventory, inventory, 8));
  if (getenv("BK_REPEAT_SAVE")) {
    for (unsigned i = 0; i < 3; ++i) {
      CHECK(settle(&run, e));
      CHECK(click(&run, 750, 250, e) && click(&run, 496, 548, e));
      CHECK(bk_checkpoint_file_read(files, 0, &bank, e) == BK_RESOURCE_OK);
      CHECK(bank.slots[0].area == 1 &&
            !memcmp(bank.slots[0].inventory, inventory, 8));
    }
    fprintf(stderr, "PASS three repeated menu saves to the same checkpoint\n");
  }
  CHECK(click(&run, 1100, 908, e) && wait_flow(&run, 2, e) && ready(&run, e));
  CHECK(bk_play_session_state(run.scene)->area == 1);
  CHECK(tick(&run, (BkInput){.pressed = BK_BUTTON_PAUSE}, e));
  CHECK(wait_flow(&run, 4, e) && settle(&run, e));
  CHECK(click(&run, 640, 336, e) && wait_flow(&run, 0x28, e) &&
        settle(&run, e));
  BkGameFrameState held = *bk_play_session_state(run.scene);
  CHECK(click(&run, 1100, 908, e) && wait_flow(&run, 4, e) && settle(&run, e));
  BkGameFrameState after = *bk_play_session_state(run.scene);
  after.background.music_volume = held.background.music_volume;
  CHECK(!memcmp(&after, &held, sizeof(held)));
  /* Four additional banks are explicit file fixtures for cross-group entry.
   * Group0 uses the actual menu-written record above. */
  for (unsigned g = 1; g < 5; ++g) {
    BkCheckpointTime time = {2026, 9, 28, 12, 34, 56};
    CHECK(bk_checkpoint_file_store(files, g, 0, g + 1, inventory, &time, 123,
                                   &bank, e));
  }
  for (unsigned g = 0; g < 5; ++g) {
    CHECK(click(&run, 640, 336, e) && wait_flow(&run, 0x28, e) &&
          settle(&run, e));
    CHECK(click(&run, 514 + 168 * g, 170, e) && click(&run, 750, 250, e) &&
          click(&run, 496, 548, e));
    CHECK(wait_flow(&run, 2, e));
    CHECK(bk_play_session_state(run.scene)->group == g &&
          bk_play_session_state(run.scene)->area == g + 1);
    for (unsigned i = 0; i < 5; ++i)
      CHECK(bk_play_session_state(run.scene)->pickup.collected[i] ==
            (g == 1 && i == 1 ? 1 : inventory[i]));
    CHECK(ready(&run, e));
    CHECK(tick(&run, (BkInput){0}, e));
    fprintf(stderr, "checkpoint entry group%u area%u phase1\n", g, g + 1);
    if (g == 0) {
      pixels = malloc(1280 * 720 * 4);
      CHECK(pixels &&
            bk_renderer_readback(run.renderer, pixels, 1280 * 720 * 4, e));
      FILE *out = fopen(argv[3], "wb");
      CHECK(out);
      size_t n = fwrite(pixels, 1, 1280 * 720 * 4, out);
      int closed = fclose(out);
      CHECK(n == 1280 * 720 * 4 && !closed);
      free(pixels);
      pixels = NULL;
    }
    CHECK(tick(&run, (BkInput){.pressed = BK_BUTTON_PAUSE}, e));
    CHECK(wait_flow(&run, 4, e) && settle(&run, e));
  }
  printf("PASS save-flow area-fixture->save->resume; pause->cancel->pause "
         "retained; five checkpoint entries->phase1 frames=%u\n",
         frames);
  status = 0;
done:
  free(pixels);
  bk_scene_destroy(run.scene);
  if (!status) {
    BkRenderStats end = bk_renderer_stats(run.renderer);
    if (end.live_allocations != empty_renderer.live_allocations ||
        end.live_bytes != empty_renderer.live_bytes) {
      fprintf(stderr, "save-flow GPU allocation leak after scene teardown\n");
      status = 1;
    } else
      fprintf(stderr, "PASS GPU allocations returned to renderer baseline\n");
  }
  bk_audio_destroy(run.audio);
  bk_renderer_destroy(run.renderer);
  bk_capture_files_destroy(captures);
  bk_checkpoint_files_destroy(files);
  bk_resources_destroy(resources);
  return status;
}
