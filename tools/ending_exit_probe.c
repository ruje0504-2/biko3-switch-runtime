/* Application lifecycle integration fixture. Include the real app owner to
 * inject ONLY the post-story flow16 handoff, without adding a runtime cheat
 * API or replacing a loader/scheduler/destructor. All subsequent modal and
 * exit actions use input through the real scene/app loop. This is not a
 * natural complete-ending walkthrough. */
#include "app/front_end.h"
/* Only the completed dialogue result is injected for the third-entry
 * fixture. The real title owner is still queried, and its stored result is
 * unchanged. All application loading, presentation and exit remain real. */
static unsigned probe_entry_kind;
static int probe_dialogue_result(const BkFrontEnd *front, BkDialogueResult *out,
                                 char error[256]) {
  if (!bk_front_end_result(front, out, error))
    return 0;
  out->kind = (int32_t)probe_entry_kind;
  return 1;
}
#define bk_front_end_result probe_dialogue_result
#include "../runtime/app/play_session.c"
#undef bk_front_end_result
#include "core/random.h"
#include <stdint.h>

#define CHECK(expr)                                                             \
  do {                                                                         \
    if (!(expr)) {                                                              \
      fprintf(stderr, "ending exit line%d (%s): %s\n", __LINE__, #expr, error); \
      goto done;                                                               \
    }                                                                          \
  } while (0)

static unsigned random_draws, ending_random_draws;

/* Independent CRT recurrence: each live owner may consume shared draws,
 * but entering a scene must never replace the sequence with another seed.
 * The bound covers resource warm-up as well as individual frame updates. */
static int probe_random_span(uint32_t before, uint32_t after,
                              unsigned *draws, char e[256]) {
  uint32_t value = before;
  for (unsigned n = 0; n <= 4096; ++n) {
    if (value == after) {
      *draws = n;
      return 1;
    }
    value = value * UINT32_C(214013) + UINT32_C(2531011);
  }
  snprintf(e, 256, "shared RNG was reseeded: %08x -> %08x", before, after);
  return 0;
}

static int probe_submit(void *p, const int16_t *samples, size_t frames,
                         char e[256]) {
  (void)samples;
  (void)e;
  *(uint64_t *)p += frames;
  return 1;
}
static int probe_poll(void *p, uint64_t *out, char e[256]) {
  (void)e;
  *out = *(uint64_t *)p;
  return 1;
}
static int probe_present(BkScene *scene, BkRenderer *renderer, BkAudio *audio,
                          char e[256]) {
  return bk_audio_fill(audio, e) && bk_renderer_begin(renderer, e) &&
         bk_scene_draw(scene, &(BkSceneFrame){0}, e) &&
         bk_renderer_end(renderer, e) &&
         bk_play_session_after_present(scene, e);
}
static int probe_tick(BkScene *scene, BkRenderer *renderer, BkAudio *audio,
                       BkInput input, char e[256]) {
  PlaySession *s = bk_scene_custom_context(scene);
  int update_ending = s->flow.current == 0x10 && !s->ending_first_present;
  uint32_t before_random = s->game_state.random;
  /* Explicitly distinguish real time from the game's scaled timestep. */
  double wall = bk_play_session_wall_seconds(scene) + .05;
  if (!bk_audio_poll(audio, e) ||
      !bk_play_session_step_at(scene, 1.0 / 60.0, wall, &input, e) ||
      !probe_present(scene, renderer, audio, e))
    return 0;
  unsigned draws;
  if (!probe_random_span(before_random, s->game_state.random, &draws, e))
    return 0;
  random_draws += draws;
  if (update_ending)
    ending_random_draws += draws;
  if (update_ending &&
      (!s->ending || bk_ending_normal_scene_wall_seconds(s->ending) != wall ||
       bk_ending_normal_scene_milliseconds(s->ending) !=
           (uint32_t)(uint64_t)(wall * 1000.0))) {
    snprintf(e, 256, "ending must consume outer wall clock independently of game dt");
    return 0;
  }
  return 1;
}
int main(int argc, char **argv) {
  if (argc < 3 || argc > 4 || (argc == 4 && strcmp(argv[3], "--third"))) {
    fprintf(stderr, "usage: ending-exit-probe DATA OUTPUT-DIRECTORY [--third]\n");
    return 2;
  }
  probe_entry_kind = argc == 4;
  char error[256] = {0}, path[2048];
  int result = 1;
  BkResourceStore *store = NULL;
  BkRenderer *renderer = NULL;
  BkAudio *audio = NULL;
  BkAudioClip *guard = NULL;
  BkScene *scene = NULL;
  BkUnlockFile *unlocks = NULL;
  uint64_t submitted = 0;
  uint8_t last[64 * 48 * 4], redraw[sizeof(last)];
  CHECK(store = bk_resources_create(error));
  const char *packs[] = {"bk3_00", "bk3_02", "bk3_03", "bk3_04",
                         "bk3_08", "bk3_11", "bk3_18", "fambom"};
  for (unsigned i = 0; i < sizeof(packs) / sizeof(*packs); ++i) {
    CHECK(snprintf(path, sizeof(path), "%s/%s.pp", argv[1], packs[i]) <
          (int)sizeof(path));
    CHECK(bk_resources_mount(store, packs[i], path, error));
  }
  CHECK(renderer = bk_renderer_create(64, 48, stdout, error));
  BkRenderStats baseline = bk_renderer_stats(renderer);
  BkAudioSink sink = {&submitted, 48000, 240, 960, probe_submit, probe_poll};
  CHECK(audio = bk_audio_create(&sink, error));
  CHECK(guard = bk_audio_clip_load(store, "bk3_02", "se002.wav", error));
  CHECK(unlocks = bk_unlock_file_create(argv[2], error));
  BkUnlockTable saved, read;
  for (unsigned group = 0; group < 5; ++group) {
    uint8_t row[8];
    for (unsigned i = 0; i < 8; ++i)
      row[i] = (uint8_t)(3 + group * 31 + i * 11);
    CHECK(bk_unlock_file_store(unlocks, group, row, &saved, error));
  }
  BkSceneServices services = {store, renderer, stdout, audio, NULL};
  CHECK(scene = bk_play_session_create_with_storage(&services, NULL, unlocks,
                                                    error));
  CHECK(probe_present(scene, renderer, audio, error));
  PlaySession *s = bk_scene_custom_context(scene);
  CHECK(s && s->flow.current == 1 && s->front);
  for (unsigned i = 0; i < 64; ++i)
    CHECK(probe_tick(scene, renderer, audio, (BkInput){0}, error));
  BkRenderStats title_baseline = bk_renderer_stats(renderer);
  s->ending_records.groups[3].count = 77;
  s->ending_records.groups[3].actions[9] = 31;
  s->ending_records.groups[0].retained[1][9999] = UINT32_C(0x87654321);
  /* Explicit process-state fixture at the post-story handoff. Do not reset
   * it on the second entry: title activity and external draws stay live. */
  s->game_state.random = UINT32_C(0x8b6f01c3);
  s->ending_auxiliary_cycle.scale = .035f;
  s->ending_auxiliary_cycle.countdown = 23;
  /*Dormant selected/replay globals have application lifetime even while a
   *normal/third story is loaded. Distinct expression addresses and the one
   *shared camera must survive title reentry and delayed old-scene release.*/
  s->ending_process.selected_control.expression_override = 8;
  s->ending_process.gallery_override = 3;
  s->ending_process.gallery_expression_latch = 1;
  s->ending_process.gallery_mouth_level = 4.25f;
  s->ending_process.selected_action.diagnostic_crossings = 1;
  s->ending_process.gallery_camera.orbit[2] = 12.5f;
  s->ending_process.gallery_selected.countdown = 4;
  BkEndingProcess retained_process = s->ending_process;
  unsigned loading_random_draws[2] = {0};
  unsigned confirmation_frames = 0, fade_frames = 0;
  for (unsigned attempt = 0; attempt < 2; ++attempt) {
    bk_random_next(&s->game_state.random);
    uint32_t entry_random = s->game_state.random;
    BkEndingAuxiliaryCycle entry_cycle = s->ending_auxiliary_cycle;
    BkDialogueResult entry;
    CHECK(bk_front_end_result(s->front, &entry, error));
    CHECK(entry.group == 0 && entry.kind == 0);
    /* Explicit post-story fixture: the application consumes row0 and the
     * selected kind at this boundary. This does not run the preceding story. */
    CHECK(schedule(s, 0x10, 0, error));
    s->flow.previous = 8;
    /* Enter at both signed and unsigned millisecond rollover boundaries.
     * The actual AVI service also consumes these samples under UBSan. */
    s->elapsed = (double)(attempt ? UINT32_MAX : INT32_MAX) / 1000.0 - .05;
    /* Model the completed incoming story curtain at this explicit handoff.
     * The ending loader must retain it instead of starting a private fade. */
    s->common.curtain = (BkFadeSprite){1, 2, 3};
    s->common.action = s->common.blocked = 0;
    CHECK(probe_tick(scene, renderer, audio, (BkInput){0}, error));
    CHECK(s->flow.current == 0x10 && s->ending && !s->retire_ending);
    CHECK(!memcmp(&retained_process, &s->ending_process, sizeof(retained_process)));
    CHECK(bk_ending_normal_scene_state(s->ending) == &s->ending_state &&
          s->ending_state.frame.phase == (probe_entry_kind ? 3 : 1) &&
          s->ending_state.control.variant == (probe_entry_kind ? 5 : 0));
    CHECK(probe_random_span(entry_random, s->game_state.random,
                             &loading_random_draws[attempt], error));
    CHECK(loading_random_draws[attempt] > 0);
    CHECK(s->ending_auxiliary_cycle.scale == entry_cycle.scale &&
          s->ending_auxiliary_cycle.countdown == entry_cycle.countdown);
    CHECK(bk_ending_normal_scene_wall_seconds(s->ending) == s->elapsed &&
          bk_ending_normal_scene_milliseconds(s->ending) ==
              (attempt ? UINT32_MAX : (uint32_t)INT32_MAX));
    CHECK(s->common.curtain.alpha == 1 && s->common.curtain.stage == 4);
    /* The first ending snapshot must be presented by the actual app loop,
     * not by a probe-only direct scene draw that masks an app ownership bug. */
    CHECK(s->ending_first_present);
    double loaded_wall = bk_ending_normal_scene_wall_seconds(s->ending);
    uint32_t loaded_random = s->game_state.random;
    BkEndingNormalControllerRetained loaded_controller = s->ending_normal_controller;
    BkEndingPresentationRetained loaded_presentation = s->ending_presentation;
    BkEndingTertiaryControllerRetained loaded_tertiary = s->ending_tertiary_controller;
    int32_t loaded_duck = s->ending_duck_transition;
    CHECK(probe_tick(scene, renderer, audio, (BkInput){0}, error));
    CHECK(!s->ending_first_present);
    CHECK(bk_ending_normal_scene_wall_seconds(s->ending) == loaded_wall &&
          s->game_state.random == loaded_random);
    CHECK(!memcmp(&loaded_controller, &s->ending_normal_controller,
                   sizeof(loaded_controller)) &&
          !memcmp(&loaded_presentation, &s->ending_presentation,
                   sizeof(loaded_presentation)) &&
          !memcmp(&loaded_tertiary, &s->ending_tertiary_controller,
                   sizeof(loaded_tertiary)) &&
          s->ending_duck_transition == loaded_duck);
    CHECK(bk_audio_play(audio, 63, guard, 1, -2300, 0, error));
    BkInput mouse = {.pointer_active = 1, .pointer_x = 60,
                      .pointer_y = attempt ? 38.5f : 36.5f};
    for (unsigned i = 0; i < 64; ++i) {
      CHECK(probe_tick(scene, renderer, audio, mouse, error));
      ++confirmation_frames;
    }
    mouse.pressed = mouse.held = BK_BUTTON_CONFIRM;
    CHECK(probe_tick(scene, renderer, audio, mouse, error));
    ++confirmation_frames;
    const BkEndingState *state = bk_ending_normal_scene_state(s->ending);
    CHECK(state && state->frame.phase == 9 &&
          state->control.pause_selection == (attempt ? 47 : 45));
    for (unsigned i = 0; i < 32; ++i) {
      CHECK(probe_tick(scene, renderer, audio, (BkInput){0}, error));
      ++confirmation_frames;
    }
    /* These retained fields are dormant in phase9. Seed them so a reset
     * of a duplicate field cannot masquerade as clearing the live owner. */
    BkEndingState *reset_fixture = (BkEndingState *)(uintptr_t)state;
    memset(&reset_fixture->retained, 0x5a, sizeof(reset_fixture->retained));
    memset(reset_fixture->saved_toggles, 0x37,
           sizeof(reset_fixture->saved_toggles));
    reset_fixture->ui_controller.hints.movement_ready = 37;
    reset_fixture->ui_controller.hints.variable_scroll = 12.5f;
    BkEndingRetained before_reset = reset_fixture->retained;
    /* Resource identities expire at logical stop. Independent process
     * fields remain intact for quit, and the native leave resets title. */
    before_reset.normal.follow_target = 0;
    before_reset.normal.direct_reference = 0;
    before_reset.normal.direct_node = 0;
    before_reset.normal.word_719b40 = 0;
    BkInput yes = {.pressed = BK_BUTTON_CONFIRM, .pointer_active = 1,
                    .pointer_x = 24, .pointer_y = 26};
    CHECK(probe_tick(scene, renderer, audio, yes, error));
    ++confirmation_frames;
    CHECK(s->common.blocked == 1 && s->common.action == (attempt ? 47 : 45));
    uint8_t working[8];
    memcpy(working, state->working[0], sizeof(working));
    unsigned fades = 0;
    while (s->flow.current == 0x10 && fades < 120) {
      CHECK(probe_tick(scene, renderer, audio, (BkInput){0}, error));
      ++fades;
    }
    fade_frames += fades;
    CHECK(fades > 0 && fades < 120 && s->flow.current == 0x50 &&
          s->flow.previous == 0x10 && s->flow.target == (attempt ? 0x58 : 1) &&
          s->flow.mode == 0 && s->ending && s->retire_ending);
    CHECK(s->common.curtain.alpha == 1 && s->common.curtain.stage == 3 &&
          s->common.action == 0 && !s->common.blocked);
    if (!attempt) {
      CHECK(!state->retained.normal.word_719b24 &&
            !state->retained.stage3.words_6bbe2c[1] &&
            state->retained.auxiliary.word_5546a0 == -1 &&
            state->retained.stage2.word_6a3c24 == 9 &&
            state->retained.stage4.value_54e310 == 1 &&
            !state->retained.final.workspace_6c7f80[9999]);
      CHECK(!memcmp(state->saved_toggles, (uint8_t[4]){0}, 4));
      CHECK(!state->ui_controller.hints.movement_ready);
    } else {
      CHECK(!memcmp(&state->retained, &before_reset, sizeof(before_reset)) &&
            !memcmp(state->saved_toggles,
                    (uint8_t[4]){0x37, 0x37, 0x37, 0x37}, 4));
      CHECK(state->ui_controller.hints.movement_ready == 37);
    }
    CHECK(state->ui_controller.hints.variable_scroll == 12.5f);
    CHECK(s->ending_unlock_valid && s->ending_unlock_group == 0 &&
          !memcmp(s->ending_unlock_flags, working, sizeof(working)));
    CHECK(s->ending_records.groups[3].count == 77 &&
          s->ending_records.groups[3].actions[9] == 31 &&
          s->ending_records.groups[0].retained[1][9999] == UINT32_C(0x87654321));
    for (unsigned voice = 0; voice < BK_AUDIO_VOICES; ++voice) {
      if (voice < 48 || (voice >= 57 && voice <= 60) || voice == 63) {
        int playing;
        CHECK(bk_audio_playing(audio, voice, &playing));
        CHECK(playing == (voice == 63));
      }
    }
    BkEndingState retained = *state;
    double retained_wall = bk_ending_normal_scene_wall_seconds(s->ending);
    uint32_t retained_ms = bk_ending_normal_scene_milliseconds(s->ending);
    /* Another process owner can consume a draw after logical release. A
     * final redraw/second stop must retain that new state exactly. */
    bk_random_next(&s->game_state.random);
    uint32_t retired_random = s->game_state.random;
    BkEndingAuxiliaryCycle retired_cycle = s->ending_auxiliary_cycle;
    BkEndingNormalControllerRetained retired_controller = s->ending_normal_controller;
    BkEndingPresentationRetained retired_presentation = s->ending_presentation;
    BkEndingTertiaryControllerRetained retired_tertiary = s->ending_tertiary_controller;
    s->ending_duck_transition = 1; /* A later process owner changed719c5c. */
    CHECK(bk_renderer_readback(renderer, last, sizeof(last), error));
    CHECK(probe_present(scene, renderer, audio, error));
    CHECK(bk_ending_normal_scene_wall_seconds(s->ending) == retained_wall &&
          bk_ending_normal_scene_milliseconds(s->ending) == retained_ms);
    CHECK(bk_renderer_readback(renderer, redraw, sizeof(redraw), error));
    CHECK(!memcmp(last, redraw, sizeof(last)) &&
          !memcmp(state, &retained, sizeof(retained)) &&
          s->flow.current == 0x50 && s->ending && s->retire_ending &&
          s->game_state.random == retired_random && s->ending_duck_transition == 1);
    CHECK(bk_audio_play(audio, 0, guard, 1, -1700, 0, error));
    CHECK(bk_ending_normal_scene_stop(s->ending, error));
    CHECK(!memcmp(&retained_process, &s->ending_process, sizeof(retained_process)));
    CHECK(s->game_state.random == retired_random);
    CHECK(probe_tick(scene, renderer, audio, (BkInput){0}, error));
    CHECK(!s->ending && !s->retire_ending &&
          s->flow.current == (attempt ? 0x58 : 1));
    CHECK(s->ending_auxiliary_cycle.scale == retired_cycle.scale &&
          s->ending_auxiliary_cycle.countdown == retired_cycle.countdown);
    CHECK(!memcmp(&retired_controller, &s->ending_normal_controller,
                   sizeof(retired_controller)) &&
          !memcmp(&retired_presentation, &s->ending_presentation,
                   sizeof(retired_presentation)) &&
          !memcmp(&retired_tertiary, &s->ending_tertiary_controller,
                   sizeof(retired_tertiary)) && s->ending_duck_transition == 1);
    int playing;
    CHECK(bk_audio_playing(audio, 0, &playing) && playing);
    CHECK(bk_audio_playing(audio, 63, &playing) && playing);
    CHECK(bk_unlock_file_read(unlocks, &read, error) == BK_RESOURCE_OK &&
          !memcmp(&saved, &read, sizeof(saved)));
    CHECK(bk_audio_clear(audio, 0, error));
    if (!attempt) {
      CHECK(!bk_play_session_finished(scene) && bk_front_end_active(s->front));
      for (unsigned i = 0; i < 64; ++i)
        CHECK(probe_tick(scene, renderer, audio, (BkInput){0}, error));
      BkRenderStats stats = bk_renderer_stats(renderer);
      CHECK(stats.live_allocations == title_baseline.live_allocations &&
            stats.live_bytes == title_baseline.live_bytes);
    } else {
      CHECK(bk_play_session_finished(scene));
    }
  }
  CHECK(ending_random_draws > 0);
  CHECK(!memcmp(&retained_process, &s->ending_process, sizeof(retained_process)));
  bk_scene_destroy(scene);
  scene = NULL;
  BkRenderStats final = bk_renderer_stats(renderer);
  CHECK(final.live_allocations == baseline.live_allocations &&
        final.live_bytes == baseline.live_bytes);
  printf("ending exit PASS kind%u: input->yes->flow50->title/reentry/flow58; "
         "confirmation_frames=%u fade_frames=%u audio_frames=%llu; "
         "shared curtain, final redraw exact,52 stopped/guard retained, "
         "unlock file unchanged, GPU baseline restored\n",
         probe_entry_kind, confirmation_frames, fade_frames, (unsigned long long)submitted);
  printf("shared RNG PASS: load_draws=%u/%u ending_draws=%u total_draws=%u; "
         "one sequence through title/reentry, first-present/redraw/stop "
         "retain RNG; auxiliary cycle and normal/opening/face process state "
         "survive first-present/redraw/retirement\n",
         loading_random_draws[0], loading_random_draws[1],
         ending_random_draws, random_draws);
  printf("PASS ending application kind%u entries2 confirmation%u fades%u "
         "pcm%llu rng%u/%u/%u/%u shared-state/curtain/retirement verified\n",
         probe_entry_kind, confirmation_frames, fade_frames,
         (unsigned long long)submitted, loading_random_draws[0],
         loading_random_draws[1], ending_random_draws, random_draws);
  result = 0;
done:
  bk_scene_destroy(scene);
  bk_unlock_file_destroy(unlocks);
  bk_audio_clip_release(guard);
  bk_audio_destroy(audio);
  bk_resources_destroy(store);
  bk_renderer_destroy(renderer);
  return result;
}
