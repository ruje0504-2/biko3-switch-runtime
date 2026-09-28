#include "scene/selection_session.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define W 640
#define H 480
#define BYTES (W * H * 4)
#define CHECK(v)                                                               \
  do {                                                                         \
    if (!(v))                                                                  \
      goto done;                                                               \
  } while (0)
typedef struct {
  uint64_t submitted, consumed, nonzero, hash;
} Sink;
static int submit(void *p, const int16_t *pcm, size_t frames, char e[256]) {
  (void)e;
  Sink *s = p;
  for (size_t i = 0; i < frames * 2; ++i) {
    uint16_t v = (uint16_t)pcm[i];
    s->nonzero += v != 0;
    s->hash = (s->hash ^ (v & 255)) * UINT64_C(1099511628211);
    s->hash = (s->hash ^ (v >> 8)) * UINT64_C(1099511628211);
  }
  s->submitted += frames;
  return 1;
}
static int poll(void *p, uint64_t *consumed, char e[256]) {
  (void)e;
  *consumed = ((Sink *)p)->consumed;
  return 1;
}
typedef struct {
  float point[2], delta[2];
} Pointer;
static int position(void *p, float out[2], char e[256]) {
  (void)e;
  memcpy(out, ((Pointer *)p)->point, 8);
  return 1;
}
static int motion(void *p, float out[2], char e[256]) {
  (void)e;
  memcpy(out, ((Pointer *)p)->delta, 8);
  return 1;
}
static int warp(void *p, float x, float y, char e[256]) {
  (void)e;
  ((Pointer *)p)->point[0] = x;
  ((Pointer *)p)->point[1] = y;
  return 1;
}
int main(int argc, char **argv) {
  if (argc != 3)
    return 2;
  char error[256] = {0}, path[1024];
  BkResourceStore *store = bk_resources_create(error);
  Sink sink = {.hash = UINT64_C(14695981039346656037)};
  BkAudio *audio = bk_audio_create(
      &(BkAudioSink){&sink, 48000, 480, 1920, submit, poll}, error);
  BkRenderer *r = NULL;
  BkSelectionSession *session = NULL;
  BkSystemAudio *sounds[8] = {0};
  uint8_t *pixels = malloc(BYTES), *repeat = malloc(BYTES);
  unsigned frames = 0, replacements = 0, redraws = 0, ended = 0;
  unsigned movie_frames = 0;
  int movie = getenv("BK_SELECTION_MOVIE_TEST") != NULL;
  int rc = 1;
  uint64_t rgba = UINT64_C(14695981039346656037);
  CHECK(store && audio && pixels && repeat);
  const char *packs[] = {"bk3_00", "bk3_01", "bk3_02", "bk3_03",
                         "bk3_04", "bk3_06", "bk3_18"};
  for (unsigned i = 0; i < (movie ? 7u : 6u); ++i) {
    snprintf(path, sizeof(path), "%s/%s.pp", argv[1], packs[i]);
    CHECK(bk_resources_mount(store, packs[i], path, error));
  }
  CHECK(bk_resources_mount_directory(store, "faces", argv[1], 20480, error));
  r = bk_renderer_create(W, H, stderr, error);
  CHECK(r);
  for (unsigned i = 1; i <= 4; ++i) {
    sounds[i] =
        bk_system_audio_create_slot(store, audio, 48 + i, i, -600, error);
    CHECK(sounds[i]);
  }
  for (unsigned run = 0; run < 2; ++run) {
    BkSelectionUi ui = {0};
    BkMenuCamera camera = {0};
    BkVoiceEnvelope envelope = {0};
    for (unsigned i = 0; i < 16; ++i)
      camera.pose.world[i] = camera.matrix[i] = i % 5 == 0;
    BkCommonHudState common = {.curtain = {0, 2, 0}};
    BkFlowTransition flow = {0x38, 1, 0, 0};
    BkMenuCursor cursor = {0};
    assert(bk_menu_cursor_initialize(&cursor, W, H));
    uint8_t hover = 0, unlocked[5][8] = {{0}};
    if (movie)
      memset(unlocked, 1, sizeof(unlocked));
    uint32_t random = 1234;
    int32_t photos[5] = {2, 5, 8, 10, 14}, group = run ? 4 : 0, area = 0,
            count = 0;
    Pointer pointer = {0};
    BkSelectionSessionConfig cfg = {
        .ui = &ui,
        .bindings = {&common, &flow, &cursor, &hover, photos, &group, &area,
                     &count},
        .camera = &camera,
        .envelope = &envelope,
        .random = &random,
        .unlocked = unlocked,
        .pointer = {&pointer, position, motion, warp},
        .music_voice = 60,
        .speech_voice = 61,
        .width = W,
        .height = H,
        .music_volume = -900,
        .voice_volume = -700,
        .loading_seconds = .016f,
        .clocks = {1000, 1001, 1002, 1003},
        .movie_clock_ms = 1000};
    memcpy(cfg.sounds, sounds, sizeof(sounds));
    session = bk_selection_session_create(r, store, audio, &cfg, error);
    CHECK(session);
    BkActorPose *previous =
        bk_selection_world_pose(bk_selection_session_world(session), 3);
    for (unsigned tick = 0; tick < 260; ++tick) {
      unsigned segment = tick / 36, phase = tick % 36,
               slot = 27 + 2 * (segment % 5);
      uint32_t buttons = 0;
      if (tick < 180) {
        if (phase == 4)
          buttons = BK_PAUSE_CONFIRM;
        if (phase >= 8 && phase < 18) {
          slot = 40;
          if (phase == 10)
            buttons = BK_PAUSE_CONFIRM;
        }
        if (phase >= 18 && phase < 24) {
          slot = 20;
          if (phase == 20)
            buttons = BK_PAUSE_CONFIRM;
        }
        if (phase >= 24) {
          slot = 17;
          if (phase == 26)
            buttons = BK_PAUSE_CONFIRM;
        }
      } else {
        slot = run ? 25 : 23;
        if (tick == 184)
          buttons = BK_PAUSE_CONFIRM;
      }
      BkSelectionSprite *p = &ui.sprites[slot];
      pointer.point[0] = p->rect[0] + p->rect[2] / 2;
      pointer.point[1] = p->rect[1] + p->rect[3] / 2;
      pointer.delta[0] = tick % 9 == 0 ? 1 : 0;
      BkSelectionSessionInput in = {.ui = {buttons, 1100 + frames * 100, .1f,
                                           W / 1280.f, -900, -700, 0, 1}};
      in.movie_clock_ms = in.movie_restart_clock_ms = in.reload_movie_clock_ms =
          (int32_t)in.ui.now_ms;
      for (unsigned i = 0; i < 3; ++i)
        in.face_clocks[i] = in.ui.now_ms + i + 1;
      for (unsigned i = 0; i < 4; ++i)
        in.reload_clocks[i] = in.ui.now_ms + i + 4;
      sink.consumed = sink.submitted;
      CHECK(bk_audio_poll(audio, error));
      CHECK(bk_selection_session_step(session, &in, error));
      movie_frames +=
          bk_selection_actor_assets_needs_movie(bk_selection_world_body(
              bk_selection_session_world(session))) != 0;
      assert(!bk_selection_session_step(session, &in, error));
      BkActorPose *now =
          bk_selection_world_pose(bk_selection_session_world(session), 3);
      if (now != previous) {
        ++replacements;
        previous = now;
        assert(ui.selected == (int)(segment % 5));
      }
      CHECK(bk_audio_fill(audio, error));
      CHECK(bk_renderer_begin(r, error));
      CHECK(bk_selection_session_draw(session, error));
      CHECK(bk_renderer_end(r, error));
      CHECK(bk_selection_session_after_present(session, error));
      CHECK(bk_renderer_readback(r, pixels, BYTES, error));
      for (unsigned i = 0; i < BYTES; ++i)
        rgba = (rgba ^ pixels[i]) * UINT64_C(1099511628211);
      if (tick == 50) {
        BkSelectionUi saved = ui;
        uint32_t rng = random;
        CHECK(bk_renderer_begin(r, error));
        CHECK(bk_selection_session_draw(session, error));
        CHECK(bk_renderer_end(r, error));
        CHECK(bk_renderer_readback(r, repeat, BYTES, error));
        assert(!memcmp(pixels, repeat, BYTES));
        assert(!memcmp(&ui, &saved, sizeof(ui)) && rng == random);
        ++redraws;
        if (!run) {
          FILE *f = fopen(argv[2], "wb");
          assert(f && fwrite(pixels, 1, BYTES, f) == BYTES && !fclose(f));
        }
      }
      ++frames;
      if (bk_selection_session_released(session)) {
        assert(flow.current == 0x50 && flow.target == (run ? 1 : 8));
        ++ended;
        break;
      }
    }
    assert(bk_selection_session_released(session));
    if (!run)
      assert(group == 4 && area == 0 && count == photos[4]);
    bk_selection_session_destroy(session);
    session = NULL;
  }
  assert(ended == 2 && replacements == 8 && redraws == 2 && sink.nonzero);
  assert(!movie || movie_frames > 0);
  printf("selection session PASS: %u frames, %u real replacements, %u inert "
         "redraws, %u exits; RGBA %016llx PCM %016llx nonzero %llu "
         "movie_frames%u\n",
         frames, replacements, redraws, ended, (unsigned long long)rgba,
         (unsigned long long)sink.hash, (unsigned long long)sink.nonzero,
         movie_frames);
  rc = 0;
done:
  if (rc)
    fprintf(stderr, "%s\n", error);
  bk_selection_session_destroy(session);
  for (unsigned i = 0; i < 8; ++i)
    bk_system_audio_destroy(sounds[i]);
  bk_renderer_destroy(r);
  bk_audio_destroy(audio);
  bk_resources_destroy(store);
  free(pixels);
  free(repeat);
  return rc;
}
