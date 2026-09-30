/* Actual app inventory -> third-route input -> phase4/6. Incoming completed
 * story and pickup bytes are explicit fixtures, not a full story/pickup run. */
#include "app/front_end.h"
#include "ending_record_natural_input.h"
static unsigned inventory_group;
static int inventory_result(const BkFrontEnd *front, BkDialogueResult *r, char e[256]) {
  if (!bk_front_end_result(front, r, e)) return 0;
  r->group = (int32_t)inventory_group; r->kind = 1;
  return 1;
}
#define bk_front_end_result inventory_result
#define BK_APP_PROBE_WALL_STEP (1. / 60.)
#define main inventory_gallery_regression_main
#include "ending_gallery_app_probe.c"
#undef main
#undef bk_front_end_result
#undef CHECK
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"inventory app line%d (%s): %s\n",\
    __LINE__, #x, error); goto done; } } while (0)

int main(int argc, char **argv) {
  if (argc != 5) {
    fprintf(stderr, "usage: ending-inventory-app-probe DATA OUTPUT GROUP ITEM\n");
    return 2;
  }
  inventory_group = (unsigned)strtoul(argv[3], NULL, 10);
  unsigned item = (unsigned)strtoul(argv[4], NULL, 10);
  if (inventory_group >= 5 || item > 255) return 2;
  char error[256] = {0}, path[2048];
  int result = 1;
  BkResourceStore *store = NULL;
  BkRenderer *renderer = NULL;
  BkAudio *audio = NULL;
  BkScene *scene = NULL;
  Sink sink = {.hash = UINT64_C(14695981039346656037)};
  CHECK(store = bk_resources_create(error));
  const char *packs[] = {"bk3_00", "bk3_01", "bk3_02", "bk3_03", "bk3_04", "bk3_05",
      "bk3_06", "bk3_08", "bk3_09", "bk3_10", "bk3_11", "bk3_12", "bk3_13", "bk3_16", "bk3_18", "fambom"};
  for (unsigned i = 0; i < sizeof(packs) / sizeof(*packs); ++i) {
    snprintf(path, sizeof(path), "%s/%s.pp", argv[1], packs[i]);
    CHECK(bk_resources_mount(store, packs[i], path, error));
  }
  CHECK(bk_resources_mount_directory(store, "faces", argv[1], 20480, error));
  CHECK(bk_resources_mount_directory(store, "fonts", argv[1], 16 * 1024 * 1024, error));
  CHECK(renderer = bk_renderer_create(80, 48, stdout, error));
  BkRenderStats baseline = bk_renderer_stats(renderer);
  BkAudioSink output = {&sink, 48000, 480, 1920, submit, poll};
  CHECK(audio = bk_audio_create(&output, error));
  BkSceneServices services = {store, renderer, stdout, audio, NULL};
  CHECK(scene = bk_play_session_create(&services, error));
  PlaySession *s = bk_scene_custom_context(scene);
  CHECK(present(scene, renderer, audio, error));
  CHECK(wait_frames(scene, renderer, audio, &sink, 64, pointer(4, 920), error));
  s->game_state.random = 0x50caa2;
  uint8_t expected[5] = {(uint8_t)item, 1, 2, 0x80, 0xfe};
  memcpy(s->game_state.pickup.collected, expected, sizeof(expected));
  s->game_state.group = inventory_group; s->game_state.area = 0;
  CHECK(schedule(s, 0x10, 0, error)); s->flow.previous = 8;
  s->common.curtain = (BkFadeSprite){1, 2, 3}; s->common.action = s->common.blocked = 0;
  CHECK(await_flow(scene, renderer, audio, &sink, 0x10, 200, error));
  CHECK(tick(scene, renderer, audio, &sink, pointer(4, 920), error));
  CHECK(settle(scene, renderer, audio, &sink, error));
  CHECK(!memcmp(s->game_state.pickup.collected, expected, sizeof(expected)));
  CHECK(record_probe_inventory(s->ending, s->game_state.pickup.collected, 0, error));
  CHECK(s->ending_state.control.toggles[4] == 1 && s->ending_state.control.toggles[6] == 0);
  BkRecordNaturalInput driver = {0};
  unsigned cursor_checks = 0;
  while (s->ending_state.frame.phase == 3 && driver.frames < 30000) {
    BkInput input;
    CHECK(record_probe_third_input(s->ending, &driver, &input, error));
    if (!cursor_checks && s->ending_state.stage3_state == 1 &&
        s->ending_state.auxiliary.progress >= .39f && driver.has_aim == 1) {
      /* Change the actual application owner while the scene remains loaded.
       * Track the animated target through real frames until sprite7/8 fades
       * settle. A moving/occluded target may require actual camera input. */
      uint8_t values[] = {0, 1, 2, 255, (uint8_t)item};
      for (unsigned v = 0; v < sizeof(values); ++v) {
        s->game_state.pickup.collected[0] = values[v];
        unsigned n;
        for (n = 0; n < 2400; ++n) {
          driver.has_aim = -1;
          CHECK(record_probe_third_input(s->ending, &driver, &input, error));
          /* Suppress confirming a branch while probing icons, but keep
           * genuine camera input if the animated target leaves the view. */
          input.pressed &= ~BK_BUTTON_CONFIRM;
          input.held &= ~BK_BUTTON_CONFIRM;
          input.released &= ~BK_BUTTON_CONFIRM;
          driver.previous_buttons = input.held;
          if (!tick(scene, renderer, audio, &sink, input, error)) {
            fprintf(stderr,"cursor value%u tick%u target%d unavailable%d/%d progress%g pointer%g/%g\n",
                values[v],n,s->ending_state.frame.camera_cached,
                s->ending_state.unavailable[0],s->ending_state.unavailable[1],
                s->ending_state.auxiliary.progress,input.pointer_x,input.pointer_y);
            CHECK(0);
          }
          char cursor_error[256];
          if (n >= 32 && record_probe_inventory(s->ending,
                  s->game_state.pickup.collected, 1, cursor_error)) break;
        }
        CHECK(n < 2400);
        CHECK(record_probe_inventory(s->ending, s->game_state.pickup.collected, 1, error));
        CHECK(again(scene, renderer, audio, error));
        ++cursor_checks;
      }
      CHECK(record_probe_third_input(s->ending, &driver, &input, error));
    }
    CHECK(tick(scene, renderer, audio, &sink, input, error));
  }
  CHECK(driver.frames < 30000 && cursor_checks == 5);
  CHECK(s->ending_state.frame.phase == (item == 1 ? 4 : 6));
  BkEndingRecord *record = &s->ending_records.groups[inventory_group];
  CHECK(record->count > 1 && record->count < BK_ENDING_RECORD_CAPACITY);
  CHECK(record->actions[record->count - 1] == (item == 1 ? 20 :
      12 + s->ending_state.auxiliary.selection));
  CHECK(wait_frames(scene, renderer, audio, &sink, 120, pointer(4, 920), error));
  CHECK(again(scene, renderer, audio, error));
  CHECK(!memcmp(s->game_state.pickup.collected, expected, sizeof(expected)));
  CHECK(bk_ending_normal_scene_stop(s->ending, error));
  CHECK(!memcmp(s->game_state.pickup.collected, expected, sizeof(expected)));
  printf("PASS inventory-app group%u item%u phase%d frames%u input%u cursors%u count%d state%016llx pcm%016llx\n",
      inventory_group, item, s->ending_state.frame.phase, frames, driver.frames, cursor_checks,
      record->count, (unsigned long long)state_hash, (unsigned long long)sink.hash);
  CHECK(record_probe_inventory_lifecycle(s->ending, error));
  bk_scene_destroy(scene); scene = NULL;
  BkRenderStats final = bk_renderer_stats(renderer);
  CHECK(final.live_allocations == baseline.live_allocations && final.live_bytes == baseline.live_bytes);
  result = 0;
done:
  bk_scene_destroy(scene); bk_audio_destroy(audio);
  bk_renderer_destroy(renderer); bk_resources_destroy(store);
  return result;
}
