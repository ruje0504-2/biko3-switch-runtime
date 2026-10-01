#include "media/audio.h"
#include "render/renderer.h"
#include "resource/store.h"
#include "scene/ending_normal_session.h"
#include "scene/scene.h"
#include <stdint.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x)                                                               \
  do {                                                                         \
    if (!(x)) {                                                                \
      fprintf(stderr, "line%d: %s\n", __LINE__, error);                      \
      goto done;                                                               \
    }                                                                          \
  } while (0)

typedef struct {
  uint64_t submitted;
} SinkState;

static int submit(void *context, const int16_t *samples, size_t frames,
                  char error[256]) {
  SinkState *s = context;
  (void)samples;
  (void)error;
  s->submitted += frames;
  return 1;
}

static int poll(void *context, uint64_t *consumed, char error[256]) {
  SinkState *s = context;
  (void)error;
  *consumed = s->submitted;
  return 1;
}

static int tick(BkRenderer *renderer, BkAudio *audio, BkScene *scene,
                 BkInput input, char error[256]) {
  BkSceneFrame frame = {{1, 1.0 / 60.0, 0}, input};
  return bk_audio_poll(audio, error) &&
         bk_scene_step(scene, 1.0 / 60.0, &input, error) &&
         bk_audio_fill(audio, error) && bk_renderer_begin(renderer, error) &&
         bk_scene_draw(scene, &frame, error) &&
         bk_renderer_end(renderer, error) &&
         bk_ending_normal_scene_after_present(scene, error);
}

int main(int argc, char **argv) {
  if (argc < 2 || argc > 4)
    return 2;
  const unsigned variant = argc >= 3 ? (unsigned)strtoul(argv[2], NULL, 10) : 0;
  const int secondary = argc == 4 && !strcmp(argv[3], "--secondary");
  const int selected2 = argc == 4 && !strcmp(argv[3], "--selected2");
  const int selected5 = argc == 4 && !strcmp(argv[3], "--selected5");
  const int confirmation = argc == 4 && !strcmp(argv[3], "--confirmation");
  const int story = argc == 4 && (!strcmp(argv[3], "--story") || confirmation);
  if (argc == 4 && !secondary && !selected2 && !selected5 &&
      (!story || (confirmation && variant != 0)))
    return 2;
  if (variant > 1)
    return 2;
  char error[256] = {0}, path[1024];
  int result = 1;
  BkRenderer *renderer = NULL;
  BkResourceStore *store = NULL;
  BkAudio *audio = NULL;
  BkAudioClip *guard = NULL;
  BkScene *scene = NULL;
  BkEndingRecords *records = NULL;
  uint8_t unlocked[5][8] = {{0}};
  SinkState sink_state = {0};
  const char *packs[] = {"bk3_00", "bk3_02", "bk3_03", "bk3_04",
                         "bk3_06", "bk3_08", "bk3_09", "bk3_10", "bk3_11",
                         "bk3_13", "bk3_18", "fambom"};
  BkSceneServices services;
  CHECK(renderer = bk_renderer_create(64, 48, stdout, error));
  CHECK(store = bk_resources_create(error));
  for (unsigned i = 0; i < sizeof(packs) / sizeof(*packs); ++i) {
    snprintf(path, sizeof(path), "%s/%s.pp", argv[1], packs[i]);
    CHECK(bk_resources_mount(store, packs[i], path, error));
  }
  BkAudioSink sink = {&sink_state, 48000, 240, 960, submit, poll};
  CHECK(audio = bk_audio_create(&sink, error));
  services = (BkSceneServices){store, renderer, stdout, audio, NULL, NULL};
  BkRenderStats baseline = bk_renderer_stats(renderer);
  if (story) {
    CHECK(records = calloc(1, sizeof(*records)));
    for (unsigned group = 0; group < 5; ++group)
      for (unsigned flag = 0; flag < 8; ++flag)
        unlocked[group][flag] = (uint8_t)(3 + group * 31 + flag * 11);
    char rejected[256] = {0};
    CHECK(!bk_ending_normal_scene_create_story(&services, 0, 0, records,
                                               NULL, NULL, rejected));
    CHECK(strstr(rejected, "saved unlock table are required"));
  }
  CHECK(scene = selected2 || selected5 ? bk_ending_selected_scene_create_gallery(
                             &services, 0, variant, selected2 ? 2 : 5,
                             unlocked, NULL, error)
                      : secondary ? bk_ending_secondary_scene_create_gallery(
                             &services, 0, variant, unlocked, NULL, error)
                      : story ? bk_ending_normal_scene_create_story(
                             &services, 0, variant, records, unlocked, NULL, error)
                      : bk_ending_normal_scene_create(&services, 0, variant,
                                                      error));
  {
    BkSceneFrame first = {{0, 0, 0}, {0}};
    CHECK(bk_audio_poll(audio, error));
    CHECK(bk_audio_fill(audio, error));
    CHECK(bk_renderer_begin(renderer, error));
    CHECK(bk_scene_draw(scene, &first, error));
    CHECK(bk_renderer_end(renderer, error));
    CHECK(bk_ending_normal_scene_after_present(scene, error));
  }
  for (unsigned frame = 0; frame < 64; ++frame) {
    BkSceneFrame sample = {{1, 1.0 / 60.0, 0}, {0}};
    CHECK(bk_audio_poll(audio, error));
    CHECK(bk_scene_step(scene, 1.0 / 60.0, &sample.input, error));
    CHECK(bk_audio_fill(audio, error));
    CHECK(bk_renderer_begin(renderer, error));
    CHECK(bk_scene_draw(scene, &sample, error));
    CHECK(bk_renderer_end(renderer, error));
    CHECK(bk_ending_normal_scene_after_present(scene, error));
  }
  unsigned confirmation_frames = 0;
  {
    const double expected = 65.0 * (double)(float)(1.0 / 60.0);
    CHECK(bk_ending_normal_scene_wall_seconds(scene) == expected);
    CHECK(bk_ending_normal_scene_milliseconds(scene) == 1083);
    BkEndingState before = *bk_ending_normal_scene_state(scene);
    const double invalid[] = {NAN, INFINITY, expected - 1.0};
    for (unsigned i = 0; i < sizeof(invalid) / sizeof(*invalid); ++i) {
      char rejected[256] = {0};
      CHECK(!bk_ending_normal_scene_step_at(scene, 1.0 / 60.0, invalid[i],
                                            &(BkInput){0}, rejected));
      CHECK(bk_ending_normal_scene_wall_seconds(scene) == expected &&
            bk_ending_normal_scene_milliseconds(scene) == 1083 &&
            !memcmp(&before, bk_ending_normal_scene_state(scene), sizeof(before)));
    }
  }
  if (confirmation) {
    /* Drive actual moving toolbar/UI via input only, never overwrite the
     * production phase to make a modal appear. At64px width scale=.05. */
    for (unsigned attempt = 0; attempt < 3; ++attempt) {
      BkInput mouse = {.pointer_active = 1, .pointer_x = 60,
                        .pointer_y = attempt == 2 ? 38.5f : 36.5f};
      for (unsigned n = 0; n < 48; ++n) {
        CHECK(tick(renderer, audio, scene, mouse, error));
        ++confirmation_frames;
      }
      mouse.pressed = mouse.held = BK_BUTTON_CONFIRM;
      CHECK(tick(renderer, audio, scene, mouse, error));
      ++confirmation_frames;
      const BkEndingState *state = bk_ending_normal_scene_state(scene);
      CHECK(state && state->frame.phase == 9 &&
            state->control.previous_phase == 1 &&
            state->control.pause_selection == (attempt == 2 ? 47 : 45));
      uint8_t retained3 = state->control.pause_flags[3];
      uint8_t retained5 = state->control.pause_flags[5];
      for (unsigned n = 0; n < 32; ++n) {
        CHECK(tick(renderer, audio, scene, (BkInput){0}, error));
        CHECK(bk_ending_normal_scene_state(scene)->frame.phase == 9);
        ++confirmation_frames;
      }
      BkInput decision = attempt == 0
                             ? (BkInput){.pressed = BK_BUTTON_BACK}
                             : (BkInput){.pressed = BK_BUTTON_CONFIRM,
                                          .pointer_active = 1,
                                          .pointer_x = attempt == 1 ? 40 : 24,
                                          .pointer_y = 26};
      CHECK(tick(renderer, audio, scene, decision, error));
      ++confirmation_frames;
      state = bk_ending_normal_scene_state(scene);
      if (attempt < 2) {
        CHECK(state->frame.phase == 1 && !state->control.pause_selection);
        CHECK(!state->control.pause_flags[0] && !state->control.pause_flags[1] &&
              !state->control.pause_flags[2] && !state->control.pause_flags[4]);
        CHECK(state->control.pause_flags[3] == retained3 &&
              state->control.pause_flags[5] == retained5);
        int playing;
        CHECK(bk_audio_playing(audio, 60, &playing) && playing);
      } else {
        CHECK(state->frame.phase == 9 && state->frame.curtain_wanted == 1 &&
              state->frame.transition_action == 47);
        /* Test the next frame's gate/import as well: UI must not restore
         * the stale request byte. The still-unimplemented full scene exit
         * is not invoked or claimed by this confirmation-input probe. */
        CHECK(tick(renderer, audio, scene, (BkInput){0}, error));
        ++confirmation_frames;
        CHECK(state->frame.curtain_wanted == 1 &&
              state->frame.transition_action == 47);
      }
    }
  }
  {
    BkRenderStats stats = bk_renderer_stats(renderer);
    const BkEndingState *state = bk_ending_normal_scene_state(scene);
    /*Normal action variants both use4cf318; the explicit secondary option
     * dispatches gallery selection1 through the independent4d00fa loader. */
    CHECK(state && state->frame.phase ==
          (confirmation ? 9 : selected2 ? 5 : selected5 ? 6 :
           secondary ? 2 : story && variant ? 3 : 1));
    if (story) {
      /* Group0's recovered stage may update its own first six flags. Other
       * groups and the two final flags must retain saved non-boolean bytes. */
      CHECK(!memcmp(state->working[1], unlocked[1], 4 * 8));
      CHECK(state->working[0][6] == unlocked[0][6] &&
            state->working[0][7] == unlocked[0][7]);
      uint8_t saved = state->working[1][0];
      unlocked[1][0] ^= 255;
      CHECK(state->working[1][0] == saved);
      unlocked[1][0] ^= 255;
    }
    CHECK(stats.draws > 0 && stats.skin_dispatches > 0 && sink_state.submitted);
    BkEndingState retained = *state;
    uint8_t before[64 * 48 * 4], after[64 * 48 * 4];
    CHECK(bk_renderer_readback(renderer, before, sizeof(before), error));
    /* Explicit ownership fixture: occupy every mixer slot so both omitted
     * ending slots and accidental clearing of another owner are observable.
     * These test sounds are not claimed to be natural ending events. */
    CHECK(guard = bk_audio_clip_load(store, "bk3_02", "se002.wav", error));
    for (unsigned voice = 0; voice < BK_AUDIO_VOICES; ++voice)
      CHECK(bk_audio_play(audio, voice, guard, 1, -2300, 0, error));
    CHECK(bk_audio_poll(audio, error));
    CHECK(bk_audio_fill(audio, error));
    BkAudioStats queued = bk_audio_stats(audio);
    CHECK(queued.submitted > queued.consumed);
    CHECK(bk_ending_normal_scene_stop(scene, error));
    /* Logical release clears only resource identities before another entry
     * can bind the shared process state. Deferred destruction must be inert. */
    retained.retained.normal.follow_target = 0;
    retained.retained.normal.direct_reference = 0;
    retained.retained.normal.direct_node = 0;
    retained.retained.normal.word_719b40 = 0;
    CHECK(!memcmp(&retained, bk_ending_normal_scene_state(scene), sizeof(retained)));
    for (unsigned voice = 0; voice < BK_AUDIO_VOICES; ++voice) {
      int playing;
      int owned = voice < 48 || (voice >= 57 && voice <= 60);
      CHECK(bk_audio_playing(audio, voice, &playing));
      CHECK(playing == !owned);
    }
    CHECK(bk_audio_stats(audio).submitted == queued.submitted &&
          bk_audio_stats(audio).consumed == queued.consumed);
    {
      char rejected[256] = {0};
      CHECK(!bk_scene_step(scene, 1.0 / 60.0, &(BkInput){0}, rejected));
      CHECK(strstr(rejected, "ending scene is stopped"));
    }
    /* A new owner may acquire the voices before deferred GPU destruction.
     * Repeated logical release must not clear those new playback epochs. */
    for (unsigned voice = 0; voice < BK_AUDIO_VOICES; ++voice)
      CHECK(bk_audio_play(audio, voice, guard, 1, -1700, 0, error));
    CHECK(bk_ending_normal_scene_stop(scene, error));
    for (unsigned voice = 0; voice < BK_AUDIO_VOICES; ++voice) {
      int playing;
      CHECK(bk_audio_playing(audio, voice, &playing) && playing);
    }
    BkSceneFrame redraw = {{0, 0, 0}, {0}};
    CHECK(bk_renderer_begin(renderer, error));
    CHECK(bk_scene_draw(scene, &redraw, error));
    CHECK(bk_renderer_end(renderer, error));
    CHECK(bk_renderer_readback(renderer, after, sizeof(after), error));
    CHECK(!memcmp(before, after, sizeof(before)));
    CHECK(bk_ending_normal_scene_after_present(scene, error));
    CHECK(!memcmp(&retained, bk_ending_normal_scene_state(scene),
                  sizeof(retained)));
    bk_scene_destroy(scene);
    scene = NULL;
    for (unsigned voice = 0; voice < BK_AUDIO_VOICES; ++voice) {
      int playing;
      CHECK(bk_audio_playing(audio, voice, &playing) && playing);
    }
    BkRenderStats released = bk_renderer_stats(renderer);
    CHECK(released.live_allocations == baseline.live_allocations &&
          released.live_bytes == baseline.live_bytes);
    printf("ending scene PASS entry=%s variant=%u frames=64 draws=%llu skin=%llu "
           "audio_frames=%llu allocations=%llu; 52 stopped/12 retained voices, "
           "64 reused voices survive retirement, final RGBA exact; "
           "confirmation_frames=%u\n",
           secondary ? "secondary-gallery" : story ? "story" : "normal-gallery", variant,
           (unsigned long long)stats.draws,
           (unsigned long long)stats.skin_dispatches,
           (unsigned long long)sink_state.submitted,
           (unsigned long long)stats.live_allocations, confirmation_frames);
  }
  result = 0;
done:
  bk_scene_destroy(scene);
  bk_audio_clip_release(guard);
  bk_audio_destroy(audio);
  bk_resources_destroy(store);
  bk_renderer_destroy(renderer);
  free(records);
  return result;
}
