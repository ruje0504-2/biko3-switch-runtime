/* Real application input/loading/presentation. Saved unlocks and recorded
 * lanes are explicit fixtures; no runtime cheat API or substituted loader.
 * The short replay lanes contain the native end marker21. This checks app
 * return ownership, not natural recording or the full replay route suite. */
#include "../runtime/app/play_session.c"
#include <stdint.h>
#ifndef BK_APP_PROBE_WALL_STEP
#define BK_APP_PROBE_WALL_STEP .05
#endif

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "gallery app line%d (%s): %s\n", \
    __LINE__, #x, error); goto done; } } while (0)
typedef struct {
  uint64_t submitted, consumed, hash, nonzero;
} Sink;
static unsigned frames, redraws, entries, pictures, returns;
static uint64_t state_hash = UINT64_C(14695981039346656037);
static uint64_t hash_bytes(uint64_t h, const void *bytes, size_t n) {
  const uint8_t *p = bytes;
  for (size_t i = 0; i < n; ++i) h = (h ^ p[i]) * UINT64_C(1099511628211);
  return h;
}
static int submit(void *p, const int16_t *pcm, size_t n, char e[256]) {
  (void)e;
  Sink *s = p;
  s->hash = hash_bytes(s->hash, pcm, n * 4);
  for (size_t i = 0; i < n * 2; ++i) s->nonzero += pcm[i] != 0;
  s->submitted += n;
  return 1;
}
static int poll(void *p, uint64_t *n, char e[256]) {
  (void)e;
  *n = ((Sink *)p)->consumed;
  return 1;
}
static int present(BkScene *scene, BkRenderer *r, BkAudio *a, char e[256]) {
  return bk_audio_fill(a, e) && bk_renderer_begin(r, e) &&
      bk_scene_draw(scene, &(BkSceneFrame){0}, e) && bk_renderer_end(r, e) &&
      bk_play_session_after_present(scene, e);
}
static int again(BkScene *scene, BkRenderer *r, BkAudio *a, char e[256]) {
  PlaySession *s = bk_scene_custom_context(scene);
  uint8_t before[80 * 48 * 4], after[sizeof(before)];
  uint32_t random = s->game_state.random;
  double clock = s->elapsed;
  BkEndingState state = s->ending_state;
  BkEndingProcess process = s->ending_process;
  if (!bk_renderer_readback(r, before, sizeof(before), e) ||
      !present(scene, r, a, e) ||
      !bk_renderer_readback(r, after, sizeof(after), e)) return 0;
  if (memcmp(before, after, sizeof(before)) || s->elapsed != clock ||
      s->game_state.random != random || memcmp(&state, &s->ending_state, sizeof(state)) ||
      memcmp(&process, &s->ending_process, sizeof(process))) {
    snprintf(e, 256, "redraw changed pixels or live process state"); return 0;
  }
  ++redraws;
  return 1;
}
static int tick(BkScene *scene, BkRenderer *r, BkAudio *a, Sink *sink,
                 BkInput in, char e[256]) {
  PlaySession *s = bk_scene_custom_context(scene);
  sink->consumed += 800;
  if (sink->consumed > sink->submitted) sink->consumed = sink->submitted;
  if (!bk_audio_poll(a, e) ||
      !bk_play_session_step_at(scene, 1. / 60.,
                               s->elapsed + BK_APP_PROBE_WALL_STEP, &in, e) ||
      !present(scene, r, a, e)) return 0;
  int32_t values[] = {s->flow.current, s->flow.previous, s->flow.target,
      s->ending_state.frame.phase, s->ending_state.frame.group,
      s->ending_state.frame.state_721ee0, s->ending_state.frame.transition_action,
      s->ending_state.control.variant, s->common.curtain.stage, s->common.blocked};
  state_hash = hash_bytes(state_hash, values, sizeof(values));
  ++frames;
  if ((s->front && s->shown == 0x18 && s->flow.current == 0x50) || s->retire_ending)
    return again(scene, r, a, e);
  return 1;
}
static BkInput pointer(float x, float y) {
  /* 80x48 output, centered64x48 content. Coordinates are authored1280x960. */
  return (BkInput){.pointer_active = 1, .pointer_x = 8 + x * .05f,
      .pointer_y = y * .05f, .pointer_motion_x = .1f};
}
static int wait_frames(BkScene *scene, BkRenderer *r, BkAudio *a, Sink *sink,
                         unsigned n, BkInput in, char e[256]) {
  for (unsigned i = 0; i < n; ++i) if (!tick(scene, r, a, sink, in, e)) return 0;
  return 1;
}
static int click(BkScene *scene, BkRenderer *r, BkAudio *a, Sink *sink,
                   float x, float y, char e[256]) {
  BkInput in = pointer(x, y);
  if (!wait_frames(scene, r, a, sink, 2, in, e)) return 0;
  in.pressed = in.held = BK_BUTTON_CONFIRM;
  return tick(scene, r, a, sink, in, e);
}
static int await_flow(BkScene *scene, BkRenderer *r, BkAudio *a, Sink *sink,
                       unsigned flow, unsigned limit, char e[256]) {
  PlaySession *s = bk_scene_custom_context(scene);
  for (unsigned i = 0; i < limit; ++i) {
    if (s->flow.current == flow) return 1;
    if (!tick(scene, r, a, sink, pointer(4, 920), e)) return 0;
  }
  snprintf(e, 256, "flow%02x did not reach%02x phase%d state%d action%d", s->flow.current,
      flow, s->ending_state.frame.phase, s->ending_state.frame.state_721ee0,
      s->ending_state.frame.transition_action);
  return 0;
}
static int settle(BkScene *scene, BkRenderer *r, BkAudio *a, Sink *sink, char e[256]) {
  PlaySession *s = bk_scene_custom_context(scene);
  for (unsigned i = 0; i < 400; ++i) {
    if (i && !s->common.blocked && !s->common.curtain.stage) return 1;
    if (!tick(scene, r, a, sink, pointer(4, 920), e)) return 0;
  }
  snprintf(e, 256, "menu curtain did not settle"); return 0;
}
static int stick_click(BkScene *scene, BkRenderer *r, BkAudio *a, Sink *sink,
                         float x, float y, char e[256]) {
  PlaySession *s = bk_scene_custom_context(scene);
  for (unsigned i = 0; i < 240; ++i) {
    double dx = x * .05 - s->cursor.sprite.rect[0];
    double dy = y * .05 - s->cursor.sprite.rect[1];
    double length = hypot(dx, dy);
    if (length < .01) {
      if (!tick(scene, r, a, sink, (BkInput){0}, e)) return 0;
      return tick(scene, r, a, sink,
          (BkInput){.pressed = BK_BUTTON_CONFIRM, .held = BK_BUTTON_CONFIRM}, e);
    }
    double magnitude = .18 + .82 * fmin(1, length / (32. / 60.));
    BkInput input = {.move_x = (float)(dx / length * magnitude),
                     .move_y = (float)(-dy / length * magnitude)};
    if (!tick(scene, r, a, sink, input, e)) return 0;
  }
  snprintf(e, 256, "gallery stick failed to reach actual menu button"); return 0;
}
int main(int argc, char **argv) {
  if (argc != 3) { fprintf(stderr, "usage: ending-gallery-app-probe DATA OUTPUT\n"); return 2; }
  char error[256] = {0}, path[2048];
  int result = 1;
  BkResourceStore *store = NULL;
  BkRenderer *renderer = NULL;
  BkAudio *audio = NULL;
  BkUnlockFile *unlocks = NULL;
  BkScene *scene = NULL;
  Sink sink = {.hash = UINT64_C(14695981039346656037)};
  CHECK(store = bk_resources_create(error));
  const char *packs[] = {"bk3_00", "bk3_02", "bk3_03", "bk3_04", "bk3_06",
      "bk3_08", "bk3_09", "bk3_10", "bk3_11", "bk3_12", "bk3_13", "bk3_18", "fambom"};
  for (unsigned i = 0; i < sizeof(packs) / sizeof(*packs); ++i) {
    CHECK(snprintf(path, sizeof(path), "%s/%s.pp", argv[1], packs[i]) < (int)sizeof(path));
    CHECK(bk_resources_mount(store, packs[i], path, error));
  }
  CHECK(renderer = bk_renderer_create(80, 48, stdout, error));
  BkRenderStats baseline = bk_renderer_stats(renderer);
  BkAudioSink output = {&sink, 48000, 480, 1920, submit, poll};
  CHECK(audio = bk_audio_create(&output, error));
  CHECK(unlocks = bk_unlock_file_create(argv[2], error));
  BkUnlockTable saved, read;
  for (unsigned g = 0; g < 5; ++g) {
    uint8_t row[8];
    for (unsigned i = 0; i < 8; ++i) row[i] = (uint8_t)(2 + g * 20 + i);
    CHECK(bk_unlock_file_store(unlocks, g, row, &saved, error));
  }
  BkSceneServices services = {store, renderer, stdout, audio, NULL};
  CHECK(scene = bk_play_session_create_with_storage(&services, NULL, unlocks, error));
  CHECK(present(scene, renderer, audio, error));
  PlaySession *s = bk_scene_custom_context(scene);
  BkGalleryMenuSelection selected;
  s->game_state.random = UINT32_C(0x4c59c018);
  CHECK(!bk_front_end_gallery_result(s->front, &selected, error)); error[0] = 0;
  for (unsigned g = 0; g < 5; ++g) {
    s->ending_records.groups[g].count = (int32_t)(7 + g);
    s->ending_records.groups[g].actions[3] = (int32_t)(12 + g);
    for (unsigned lane = 0; lane < 2; ++lane)
      for (unsigned i = 0; i < BK_ENDING_RECORD_CAPACITY; ++i)
        s->ending_records.groups[g].retained[lane][i] = 21;
  }
  uint64_t record_hash = hash_bytes(0, &s->ending_records, sizeof(s->ending_records));
  const unsigned order[] = {6, 7, 0, 1, 2, 3, 4, 5};
  const float action_y[] = {240, 304, 368, 547, 612, 672, 432, 738};
  const int phases[] = {1, 2, 5, 3, 4, 6, 8, 8};
  for (unsigned g = 0; g < 5; ++g) {
    for (unsigned n = 0; n < 8; ++n) {
      unsigned action = order[n];
      if (s->flow.current == 1) {
        CHECK(wait_frames(scene, renderer, audio, &sink, 64, pointer(4, 920), error));
        CHECK(click(scene, renderer, audio, &sink, 1084, 262, error));
        CHECK(await_flow(scene, renderer, audio, &sink, 0x18, 200, error));
        CHECK(wait_frames(scene, renderer, audio, &sink, 64, pointer(4, 920), error));
        CHECK(!bk_front_end_gallery_result(s->front, &selected, error)); error[0] = 0;
        /*Explicit inherited title-idle state; moving the actual stick must
         *restore the cursor even though native gallery only polls position.*/
        s->cursor.wanted = 0;
        s->cursor.sprite.fade.alpha = 0;
        s->cursor.sprite.fade.stage = 0;
        CHECK(stick_click(scene, renderer, audio, &sink, 92, 206 + g * 150, error));
        CHECK(s->cursor.wanted && s->cursor.sprite.fade.alpha > 0);
      }
      CHECK(s->flow.current == 0x18);
      if (n == 0) {
        for (unsigned p = 0; p < 5; ++p) {
          CHECK(click(scene, renderer, audio, &sink, p % 2 ? 840 : 1080,
                        288 + ((p + 1) / 2) * 200, error));
          CHECK(settle(scene, renderer, audio, &sink, error));
          CHECK(again(scene, renderer, audio, error));
          BkInput back = pointer(4, 920); back.pressed = BK_BUTTON_BACK;
          CHECK(tick(scene, renderer, audio, &sink, back, error));
          CHECK(again(scene, renderer, audio, error));
          CHECK(settle(scene, renderer, audio, &sink, error));
          ++pictures;
        }
      }
      CHECK(click(scene, renderer, audio, &sink, 612, action_y[action], error));
      CHECK(await_flow(scene, renderer, audio, &sink, 0x10, 200, error));
      CHECK(bk_front_end_gallery_result(s->front, &selected, error));
      CHECK(selected.group == (int32_t)g && selected.selection == (int32_t)(action >= 6 ? 6 : action));
      CHECK(selected.variant == (int32_t)((action >= 3 && action <= 5) || action == 7));
      CHECK(s->ending && s->ending_first_present && s->flow.previous == 0x18);
      CHECK(bk_ending_normal_scene_state(s->ending) == &s->ending_state &&
            s->ending_state.frame.phase == phases[action] && s->ending_state.frame.group == g);
      CHECK(tick(scene, renderer, audio, &sink, pointer(4, 920), error));
      CHECK(again(scene, renderer, audio, error));
      ++entries;
      printf("gallery-app group%u action%u entered phase%d\n", g, action, phases[action]); fflush(stdout);
      if (action >= 6) {
        CHECK(await_flow(scene, renderer, audio, &sink, 0x18, 6000, error));
        CHECK(s->ending_unlock_valid && s->ending_unlock_group == g);
        CHECK(wait_frames(scene, renderer, audio, &sink, 64, pointer(4, 920), error));
        ++returns;
      } else {
        CHECK(wait_frames(scene, renderer, audio, &sink, 90, pointer(1200, 730), error));
        CHECK(click(scene, renderer, audio, &sink, 1200, 730, error));
        CHECK(s->ending_state.frame.phase == 9 && s->ending_state.control.pause_selection == 45);
        CHECK(wait_frames(scene, renderer, audio, &sink, 32, pointer(480, 520), error));
        CHECK(click(scene, renderer, audio, &sink, 480, 520, error));
        CHECK(await_flow(scene, renderer, audio, &sink, 1, 200, error));
      }
      CHECK(record_hash == hash_bytes(0, &s->ending_records, sizeof(s->ending_records)));
      CHECK(bk_unlock_file_read(unlocks, &read, error) == BK_RESOURCE_OK);
      CHECK(!memcmp(&read, &saved, sizeof(saved)));
    }
  }
  CHECK(entries == 40 && pictures == 25 && returns == 10 && sink.nonzero);
  /*This fixture has no writable capture storage. The real flow48 loader
   *must reject that missing service; special-session-probe covers success.*/
  CHECK(wait_frames(scene, renderer, audio, &sink, 64, pointer(4, 920), error));
  CHECK(click(scene, renderer, audio, &sink, 1084, 262, error));
  CHECK(await_flow(scene, renderer, audio, &sink, 0x18, 200, error));
  CHECK(settle(scene, renderer, audio, &sink, error));
  CHECK(click(scene, renderer, audio, &sink, 92, 806, error));
  CHECK(click(scene, renderer, audio, &sink, 840, 288, error));
  CHECK(!await_flow(scene, renderer, audio, &sink, 0x48, 250, error));
  CHECK(strstr(error, "special entry requires writable capture storage") &&
        s->game_state.group == 4 && s->game_state.area == 0 && !s->game);
  error[0] = 0;
  bk_scene_destroy(scene); scene = NULL;
  bk_unlock_file_destroy(unlocks); unlocks = NULL;
  /*A separate new-user storage root has no unlock table. Read-only clicks
   *must neither unlock entries nor manufacture a replay recording.*/
  CHECK(snprintf(path, sizeof(path), "%s/locked", argv[2]) < (int)sizeof(path));
  CHECK(unlocks = bk_unlock_file_create(path, error));
  CHECK(scene = bk_play_session_create_with_storage(&services, NULL, unlocks, error));
  CHECK(present(scene, renderer, audio, error));
  s = bk_scene_custom_context(scene);
  s->game_state.random = UINT32_C(0x4c59c018);
  CHECK(wait_frames(scene, renderer, audio, &sink, 64, pointer(4, 920), error));
  CHECK(click(scene, renderer, audio, &sink, 1084, 262, error));
  CHECK(await_flow(scene, renderer, audio, &sink, 0x18, 200, error));
  CHECK(settle(scene, renderer, audio, &sink, error));
  for (unsigned i = 0; i < 8; ++i) {
    CHECK(click(scene, renderer, audio, &sink, 612, action_y[i], error));
    CHECK(!s->common.blocked && s->flow.current == 0x18 && !s->ending);
  }
  CHECK(click(scene, renderer, audio, &sink, 1080, 288, error));
  CHECK(!s->common.blocked && s->flow.current == 0x18);
  CHECK(bk_unlock_file_read(unlocks, &read, error) == BK_RESOURCE_MISSING);
  CHECK(click(scene, renderer, audio, &sink, 1104, 912, error));
  CHECK(await_flow(scene, renderer, audio, &sink, 1, 200, error));
  bk_scene_destroy(scene); scene = NULL;
  BkRenderStats after = bk_renderer_stats(renderer);
  CHECK(after.live_allocations == baseline.live_allocations && after.live_bytes == baseline.live_bytes);
  printf("PASS gallery-app entries%u pictures%u replay-returns%u locked9 flow48-reject1 frames%u redraws%u state%016llx PCM%016llx samples%llu nonzero%llu\n",
      entries, pictures, returns, frames, redraws, (unsigned long long)state_hash,
      (unsigned long long)sink.hash, (unsigned long long)sink.submitted * 2,
      (unsigned long long)sink.nonzero);
  result = 0;
done:
  bk_scene_destroy(scene);
  bk_unlock_file_destroy(unlocks);
  bk_audio_destroy(audio);
  bk_renderer_destroy(renderer);
  bk_resources_destroy(store);
  return result;
}
