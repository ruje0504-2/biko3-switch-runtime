#include "scene/ending_secondary_controller.h"
#include "scene/ending_secondary_presentation.h"
#include "scene/system_audio.h"
#include "core/input.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Actual independent secondary resources, real controller/presentation and
 * consumed PCM. Loader outputs and gamepad gestures are explicit fixtures.
 * No controller state, clip time or audio status is forced after entry.
 * This does not exercise the outer application loader/GPU/UI lifecycle. */
#define CHECK(x) do { if (!(x)) { \
  fprintf(stderr, "secondary controller probe line %d: %s: %s\n", \
          __LINE__, #x, e); goto done; } } while (0)
typedef struct { uint64_t submitted, consumed, hash; } Sink;
typedef struct {
  uint32_t now, calls;
  BkInput input;
  BkSystemAudio *confirm, *hover;
} Input;
typedef struct {
  BkEndingAudio *audio;
  unsigned group;
} Speech;
static uint64_t hash(uint64_t value, const void *bytes, size_t count) {
  const uint8_t *p = bytes;
  for (size_t i = 0; i < count; ++i)
    value = (value ^ p[i]) * UINT64_C(1099511628211);
  return value;
}
static int submit(void *p, const int16_t *pcm, size_t frames, char e[256]) {
  (void)e;
  Sink *s = p;
  s->hash = hash(s->hash, pcm, frames * 2 * sizeof(*pcm));
  s->submitted += frames;
  return 1;
}
static int poll(void *p, uint64_t *frames, char e[256]) {
  (void)e;
  *frames = ((Sink *)p)->consumed;
  return 1;
}
static int clock_read(void *p, uint32_t *now, char e[256]) {
  (void)e;
  *now = ((Input *)p)->now;
  return 1;
}
static int key(void *p, unsigned code, unsigned mode, uint32_t *out,
                 char e[256]) {
  Input *i = p;
  if (mode > 3) {
    snprintf(e, 256, "unknown physical key mode %u", mode);
    return 0;
  }
  ++i->calls;
  uint32_t mask = code == 0 ? BK_BUTTON_CONFIRM : code == 1 ? BK_BUTTON_BACK : 0;
  *out = mode == 0 ? !((i->input.held | i->input.pressed | i->input.released) & mask)
           : mode == 1 ? !!(i->input.pressed & mask)
           : mode == 2 ? !!(i->input.held & mask)
                       : !!(i->input.released & mask);
  return 1;
}
static int speech_load(void *p, unsigned slot, const char *name, char e[256]) {
  return bk_ending_audio_load_speech(((Speech *)p)->audio, slot, name, e);
}
static int focus_sound(void *p, unsigned slot, int32_t volume, char e[256]) {
  (void)volume;
  Input *i = p;
  if (slot != 0 && slot != 3) {
    snprintf(e, 256, "unexpected focus toolbar sound %u", slot);
    return 0;
  }
  return bk_system_audio_restart(slot ? i->hover : i->confirm, e);
}
static int speech_play(void *p, unsigned slot, int32_t flags, int32_t volume,
                         char e[256]) {
  Speech *s = p;
  BkEndingAudioCall call = {.operation = BK_ENDING_AUDIO_RESTART,
                            .slot = slot, .flags = flags, .volume = volume};
  int ignored;
  return bk_ending_audio_call(s->audio, s->group, 0, 0, &call, &ignored, e);
}
static uint64_t matrices(BkActorPose *pose) {
  const BkModel *model = bk_actor_pose_model(pose);
  uint64_t value = UINT64_C(14695981039346656037);
  for (uint32_t i = 0; i < model->frame_count; ++i) {
    value = hash(value, bk_actor_pose_local(pose, i), 64);
    value = hash(value, bk_actor_pose_frame(pose, i), 64);
    value = hash(value, bk_actor_pose_parent_world(pose, i), 64);
  }
  return value;
}
static int geometry(BkEndingSecondaryAssets *a, const BkMenuCamera *camera,
                       unsigned width, unsigned height, float worlds[39][16],
                       uint8_t present[39], float projection[16], float viewport[16],
                       BkEndingUiPickBindings *out, char e[256]) {
  BkActorForest *forest = bk_ending_secondary_assets_forest(a);
  if (!bk_camera_projection(projection,
         &(BkCameraLens){camera->fov, .75f, .5f, 126384})) return 0;
  memset(viewport, 0, 64);
  viewport[0] = viewport[12] = width * .5f;
  viewport[5] = viewport[13] = height * .5f; /*projection flips Y once*/
  viewport[10] = viewport[15] = 1;
  for (unsigned i = 0; i < 39; ++i) {
    uint32_t frame = bk_ending_secondary_assets_node(a, i);
    present[i] = frame != BK_MODEL_NONE;
    if (present[i]) {
      const float *world = bk_actor_forest_world(forest,
          bk_actor_forest_node(forest, 0, frame));
      if (!world) { snprintf(e, 256, "missing old published target"); return 0; }
      memcpy(worlds[i], world, 64);
    }
  }
  *out = (BkEndingUiPickBindings){worlds, present, 39, camera->pose.position,
      bk_actor_forest_view(forest), projection, viewport, 0};
  return 1;
}
/* Test-driver hit search only. Use a private copy so planning a gesture
 * cannot change any gameplay state, retained coordinates or sprite fields. */
static int hit_at(const BkEndingSecondaryControllerScene *s, float x, float y,
                     int wanted, char e[256]) {
  BkEndingFrameState frame = s->state->frame;
  int32_t kind = *s->action_kind, column = *s->action_column;
  int32_t targets[39][2], alternate[2], result;
  memcpy(targets, s->state->targets, sizeof(targets));
  memcpy(alternate, s->state->alternate, sizeof(alternate));
  BkEndingUiSprite ring = s->ui->sprites[50];
  const float *world = bk_actor_forest_world(
      bk_ending_secondary_assets_forest(s->assets),
      (uint32_t)s->state->retained.normal.word_719b40);
  BkEndingSecondaryPickBindings b = {&frame, &kind, &column, targets, alternate,
                                     &ring, s->geometry, world};
  if (!bk_ending_secondary_pick(&b, (float[2]){x, y}, 0, &result, e)) return -1;
  return result == wanted;
}
static int gesture_point(const BkEndingSecondaryControllerScene *s, int wanted,
                            int32_t point[2], char e[256]) {
  int32_t center[2] = {0};
  if (wanted) {
    BkEndingUiPickBindings g = *s->geometry;
    uint8_t present = 1;
    if (wanted == 2) {
      const float *world = bk_actor_forest_world(
          bk_ending_secondary_assets_forest(s->assets),
          (uint32_t)s->state->retained.normal.word_719b40);
      if (!world) return 0;
      g.world = (const float (*)[16])world;
      g.present = &present;
      g.count = 1;
    }
    if (!bk_ending_ui_project_target(&g, 0, center, e)) return 0;
    /*Try actual projected centers first, then every integer pixel nearby.
     * Coarse scanning alone can miss a small distant hit circle. */
    for (int radius = 0; radius <= 64; ++radius)
      for (int dy = -radius; dy <= radius; ++dy)
        for (int dx = -radius; dx <= radius; ++dx) {
          if (abs(dx) != radius && abs(dy) != radius) continue;
          int64_t x = (int64_t)center[0] + dx, y = (int64_t)center[1] + dy;
          if (x < 0 || y < 0 || x >= s->width || y >= s->height) continue;
          int hit = hit_at(s, (float)x, (float)y, wanted, e);
          if (hit < 0) return 0;
          if (hit) { point[0] = (int32_t)x; point[1] = (int32_t)y; return 1; }
        }
  }
  for (unsigned y = 4; y < s->height; y += 8)
    for (unsigned x = 4; x < s->width; x += 8) {
      int hit = hit_at(s, (float)x, (float)y, wanted, e);
      if (hit < 0) return 0;
      if (hit) { point[0] = (int32_t)x; point[1] = (int32_t)y; return 1; }
    }
  snprintf(e, 256, "driver cannot find on-screen event%d target: center=%d,%d "
      "content=%u,%u camera=%g,%g,%g fov=%g", wanted, center[0], center[1],
      s->width, s->height, s->camera->pose.position[0], s->camera->pose.position[1],
      s->camera->pose.position[2], s->camera->fov);
  return 0;
}

int main(int argc, char **argv) {
  if (argc < 2 || argc > 3) return 2;
  int control_only = argc == 3 && !strcmp(argv[2], "--control-only");
  char e[256] = {0}, path[1024];
  int rc = 1;
  FILE *catalog = argc == 3 && !control_only ? fopen(argv[2], "w") : NULL;
  BkResourceStore *store = bk_resources_create(e);
  BkEndingSecondaryAssets *assets = NULL;
  BkEndingAudio *audio = NULL;
  BkAudio *mixer = NULL;
  BkSystemAudio *confirm = NULL, *hover = NULL;
  BkEndingState *state = calloc(1, sizeof(*state));
  uint64_t all_pcm = UINT64_C(14695981039346656037), all_geometry = all_pcm;
  unsigned speech_cases = 0, pauses = 0, wraps = 0;
  unsigned completed = 0, total = 0, menus = 0, frozen = 0, visited = 0;
  CHECK(store && state && (argc != 3 || control_only || catalog));
  const char *packs[] = {"bk3_09", "bk3_04", "bk3_03", "fambom", "bk3_06", "bk3_02"};
  for (unsigned i = 0; i < sizeof(packs) / sizeof(*packs); ++i) {
    snprintf(path, sizeof(path), "%s/%s.pp", argv[1], packs[i]);
    CHECK(bk_resources_mount(store, packs[i], path, e));
  }
  /* Every actual47D9EE identity reaches the mixer. Native one-shots finish
   * only after sample consumption. Repeated loop cues also exercise low-byte
   * flags, wrap, pause and position-preserving resume. Catalog records are
   * checked separately against Python's RIFF/WAVE decoder. */
  for (unsigned group = 0; !control_only && group < 5; ++group) {
    Sink sink = {.hash = UINT64_C(14695981039346656037)};
    BkAudioSink output = {&sink, 48000, 480, 1920, submit, poll};
    mixer = bk_audio_create(&output, e);
    CHECK(mixer);
    audio = bk_ending_audio_create(store, mixer, 0, e);
    CHECK(audio);
    Speech speech = {audio, group};
    BkEndingSecondarySpeechOps ops = {&speech, speech_load, speech_play};
    for (unsigned trial = 0; trial < 21; ++trial) {
      int loop = trial >= 18;
      unsigned cue = loop ? (unsigned[]){4, 8, 14}[trial - 18] : trial + 1;
      unsigned slot = trial & 1;
      int32_t volume = trial % 3 ? -1000 : 0;
      CHECK(bk_ending_audio_stop(audio, e));
      sink.consumed = sink.submitted;
      CHECK(bk_audio_poll(mixer, e));
      CHECK(bk_ending_secondary_speech(group, (int32_t)cue, slot, loop ? 257 : 256,
          state->speech_names, &volume, &ops, e));
      CHECK(bk_audio_fill(mixer, e));
      BkAudioCursor cursor;
      CHECK(bk_audio_cursor(mixer, slot, &cursor) && cursor.pcm && cursor.playing);
      size_t source_frames = bk_pcm_frames(cursor.pcm);
      uint32_t channels = bk_pcm_channels(cursor.pcm), rate = bk_pcm_rate(cursor.pcm);
      uint64_t fingerprint = hash(UINT64_C(14695981039346656037),
          bk_pcm_samples(cursor.pcm), source_frames * channels * sizeof(int16_t));
      if (!loop && catalog)
        fprintf(catalog, "%s %u %u %zu %016llx\n", state->speech_names[slot], rate,
            channels, source_frames, (unsigned long long)fingerprint);
      int playing = 1;
      unsigned pumps = 0, local_wraps = 0;
      size_t last_source = cursor.source_frame;
      for (; pumps < 10000 && playing && (!loop || local_wraps < 2); ++pumps) {
        uint64_t next = sink.consumed + 800;
        sink.consumed = next < sink.submitted ? next : sink.submitted;
        CHECK(bk_audio_poll(mixer, e) && bk_audio_fill(mixer, e));
        CHECK(bk_audio_cursor(mixer, slot, &cursor));
        if (loop && cursor.source_frame < last_source) ++local_wraps;
        last_source = cursor.source_frame;
        if (pumps == 3) {
          CHECK(bk_audio_pause(mixer, slot, e));
          CHECK(bk_audio_playing(mixer, slot, &playing) && !playing);
          sink.consumed = sink.submitted;
          CHECK(bk_audio_poll(mixer, e) && bk_audio_fill(mixer, e));
          CHECK(bk_audio_cursor(mixer, slot, &cursor));
          size_t paused_source = cursor.source_frame;
          for (unsigned hold = 0; hold < 3; ++hold) {
            sink.consumed = sink.submitted;
            CHECK(bk_audio_poll(mixer, e) && bk_audio_fill(mixer, e));
            CHECK(bk_audio_cursor(mixer, slot, &cursor));
            CHECK(cursor.source_frame == paused_source && !cursor.playing);
            ++pauses;
          }
          CHECK(bk_audio_resume(mixer, slot, loop, e));
          last_source = paused_source;
        }
        CHECK(bk_audio_playing(mixer, slot, &playing));
      }
      CHECK(pumps < 10000 && (loop ? playing && local_wraps >= 2 : !playing));
      wraps += local_wraps;
      ++speech_cases;
    }
    all_pcm = hash(all_pcm, &sink.hash, sizeof(sink.hash));
    bk_ending_audio_destroy(audio); audio = NULL;
    bk_audio_destroy(mixer); mixer = NULL;
  }
  if (catalog) { CHECK(!fclose(catalog)); catalog = NULL; }
  printf("PCM identities=%u paused_polls=%u loop_wraps=%u\n", speech_cases, pauses, wraps);
  fflush(stdout);

  BkEndingSecondaryControlState retained = bk_ending_secondary_control_initial();
  BkEndingSecondaryPresentationState shown = bk_ending_secondary_presentation_initial();
  BkEndingVoiceEnvelope voice = {0};
  uint32_t random = 0x47a5d0;
  for (unsigned group = 0; group < 5; ++group) {
    for (unsigned profile = 0; profile < 2; ++profile) {
      memset(state, 0, sizeof(*state));
      uint32_t clocks[4] = {100, 100, 100, 100};
      BkMenuCamera camera = {0};
      BkEndingCameraPresets presets;
      BkEndingCameraTransitions transitions = {0};
      CHECK(bk_menu_camera_dialogue(&camera));
      assets = bk_ending_secondary_assets_create(store, group, profile, clocks,
          &random, &camera, &presets, e);
      CHECK(assets && bk_ending_secondary_assets_load_background(assets, store, e));
      BkActorForest *forest = bk_ending_secondary_assets_forest(assets);
      const BkEndingSecondaryConfig *config = bk_ending_secondary_assets_config(assets);
      BkActorPose *primary = bk_ending_secondary_assets_pose(assets, 0);
      BkActorPose *background = bk_ending_secondary_assets_pose(assets, 3);
      unsigned width = profile ? 640 : 960, height = profile ? 480 : 720;
      BkEndingUi ui = {0};
      BkEndingStageUi stage = {0};
      uint8_t flags[6] = {0};
      float gauge = 0;
      CHECK(bk_ending_ui_initialize(&ui, width, flags, &gauge, e));
      CHECK(bk_ending_stage_ui_initialize(&ui, &stage, BK_ENDING_UI_SECONDARY,
          group, profile, width, e));
      state->frame = (BkEndingFrameState){.phase = 2, .group = (uint8_t)group,
          .state_721ee4 = 4, .camera_mode = 5, .camera_cached = -1};
      memcpy(state->frame.camera_table, config->camera_table, sizeof(config->camera_table));
      for (unsigned i = 0; i < 3; ++i)
        memcpy(state->control.targets[i], bk_ending_secondary_assets_target(assets, i), 12);
      memcpy(state->frame.camera_values, state->control.targets[0], 12);
      state->control.variant = 1;
      unsigned missing = bk_ending_secondary_assets_missing_visible(assets);
      state->control.toggles[1] = missing == 3;
      state->control.toggles[2] = missing != 3;
      state->control.toggles[3] = group != 1;
      state->control.toggles[4] = group == 1;
      state->control.toggles[5] = state->control.toggles[7] = 1;
      state->auxiliary.variant = (int32_t)profile;
      state->auxiliary.expression_a = config->expression_a;
      state->auxiliary.expression_b = config->expression_b;
      state->selected = group == 1 ? 1 : -1;
      state->retained.normal.follow_target = bk_actor_forest_node(forest, 0,
          bk_ending_secondary_assets_follow(assets));
      state->retained.normal.word_719b40 = (int32_t)bk_actor_forest_node(forest, 0,
          bk_ending_secondary_assets_anchor(assets));
      Sink sink = {.hash = UINT64_C(14695981039346656037)};
      BkAudioSink output = {&sink, 48000, 480, 1920, submit, poll};
      mixer = bk_audio_create(&output, e);
      CHECK(mixer);
      audio = bk_ending_audio_create(store, mixer, 0, e);
      CHECK(audio);
      confirm = bk_system_audio_create_slot(store, mixer, 57, 0, -600, e);
      hover = bk_system_audio_create_slot(store, mixer, 58, 3, -600, e);
      CHECK(confirm && hover);
      Input input = {.now = 1000, .confirm = confirm, .hover = hover};
      BkEndingControlRect rects[BK_ENDING_CONTROL_RECTS];
      CHECK(bk_ending_ui_control_rects(&ui, rects));
      float scale = (float)((double)width / 1280);
      int32_t effect_volume = -600;
      BkEndingUiPickBindings geo = {0};
      float worlds[39][16], projection[16], viewport[16];
      uint8_t present[39];
      int32_t action_kind = -1, action_column = -1, volume = -1000;
      int8_t previous = profile ? 0x18 : 8;
      BkEndingSecondaryControllerScene controller = {
          assets, audio, state, &retained, &shown.rate, &action_kind, &action_column,
          &camera, &transitions, &presets, &ui, &geo, width, height, &previous,
          &volume, &random, &input, key};
      BkEndingSecondaryPresentationScene display = {
          assets, audio, &shown, &voice, &random, &input, clock_read};
      unsigned frame = 0, operation = 0, held = 0, local_menu = 0, focus_steps = 0;
      int idle_clicked = 0, entered = 0;
      CHECK(bk_actor_forest_camera_publish(forest, e));
      for (; frame < 16000 && !state->frame.curtain_wanted; ++frame) {
        input.now = 1000 + (uint32_t)((uint64_t)frame * 1000 / 60);
        input.input = (BkInput){0};
        int playing0, playing1;
        CHECK(bk_audio_playing(mixer, 0, &playing0) && bk_audio_playing(mixer, 1, &playing1));
        if (playing0 && !entered && held < 12) {
          ++held; ++frozen;
        } else {
          uint64_t next = sink.consumed + 800;
          sink.consumed = next < sink.submitted ? next : sink.submitted;
        }
        CHECK(bk_audio_poll(mixer, e));
        CHECK(geometry(assets, &camera, width, height, worlds, present,
            projection, viewport, &geo, e));
        geo.ring_width = ui.sprites[50].rect[2];
        BkEndingFrameInput captured = {0};
        int32_t point[2] = {0, 0};
        int before = state->frame.state_721ee4;
        if (before >= 0 && before < 8) visited |= 1u << (unsigned)before;
        if (before == 1) {
          entered = 1;
          if (!playing1 && !idle_clicked) {
            CHECK(gesture_point(&controller, 0, point, e));
            input.input.held = input.input.pressed = BK_BUTTON_CONFIRM;
            idle_clicked = 1;
          } else if (!playing1 && operation == 4 && focus_steps < 122) {
            /*The ending's alternate target can lie outside the initial
             * node0 view. Use its real toolbar to move focus through the
             * midpoint to A_okosi, then let the native camera settle. */
            if (focus_steps < 2) {
              point[0] = (int32_t)(rects[3].x + rects[3].width * .5f);
              point[1] = (int32_t)(rects[3].y + rects[3].height * .5f);
              input.input.held = input.input.pressed = BK_BUTTON_CONFIRM;
              BkEndingControlBindings cb = {&state->frame, &camera, &presets,
                  {worlds[0] + 12, bk_actor_forest_world(forest,
                    (uint32_t)state->retained.normal.word_719b40) + 12,
                   worlds[5] + 12, worlds[13] + 12}, rects, &scale, &effect_volume};
              BkEndingControlOps co = {&input, key, focus_sound, NULL, NULL};
              BkEndingFrameInput focus_input = {0};
              memcpy(focus_input.words + 9, point, sizeof(point));
              CHECK(bk_ending_control_step(&state->control, &cb, &focus_input, &co, e));
            }
            ++focus_steps;
          } else if (!playing1 && operation < 5) {
            CHECK(gesture_point(&controller, operation == 4 ? 2 : 1, point, e));
            input.input.held = input.input.pressed = BK_BUTTON_CONFIRM;
          }
        } else if (before == 3) {
          int choice = operation == 4 ? 4 : (int)operation;
          unsigned index = 0;
          while (index < 3 && state->choices[index] != choice) ++index;
          CHECK(index < 3);
          memcpy(point, state->points[index], sizeof(point));
          input.input.released = BK_BUTTON_CONFIRM;
          ++operation;
          ++menus; ++local_menu;
        }
        memcpy(captured.words + 9, point, sizeof(point));
        uint64_t primary_held = matrices(primary), background_held = matrices(background);
        CHECK(bk_ending_secondary_controller_scene_step(&controller, &captured, 1.f/60, e));
        CHECK(matrices(primary) == primary_held && matrices(background) == background_held);
        CHECK(bk_ending_secondary_presentation_scene_step(&display, &state->frame,
            &state->auxiliary, &retained.automatic, state->control.toggles, 1.f/60, e));
        const uint32_t tracks[] = {1, 2};
        float offset[3];
        memcpy(offset, state->frame.camera_values, sizeof(offset));
        if ((state->frame.camera_mode == 0 &&
             !((state->frame.state_721ee0 == 3 || state->frame.state_721ee0 == 4) &&
               state->frame.state_721ee4 == 4)) || state->frame.camera_mode == 4) {
          CHECK(bk_ending_camera_assets_step(bk_ending_secondary_assets_cameras(assets),
              forest, tracks, &camera, state->frame.camera_mode == 4
                  ? BK_ENDING_CAMERA_FIXED : BK_ENDING_CAMERA_ORBIT,
              offset, (float[2]){0}, 0, BK_FRAME_NONE, 1.f/60, e));
        } else CHECK(state->frame.camera_mode == 5 || state->frame.camera_mode == 0);
        const BkFrameVisit *walk;
        uint32_t count;
        CHECK(bk_actor_forest_draw(forest, bk_ending_secondary_assets_root(assets, 3),
                                  &walk, &count, e));
        primary_held = matrices(primary);
        all_geometry = hash(all_geometry, &primary_held, sizeof(primary_held));
        CHECK(bk_audio_fill(mixer, e));
        ++total;
        if (frame && frame % 2000 == 0) {
          BkClipState clip;
          CHECK(bk_actor_pose_state(primary, &clip));
          printf("progress group=%u profile=%u frames=%u state=%d sub=%u clip=%u menus=%u\n",
              group, profile, frame, state->frame.state_721ee4,
              state->retained.stage3.byte_6bbe34, clip.slot, local_menu);
          fflush(stdout);
        }
      }
      CHECK(frame < 16000 && entered && operation == 5 && local_menu == 5);
      CHECK(state->frame.transition_action == (profile ? 0x31 : 6));
      CHECK(state->frame.state_721ee4 == 4 && state->retained.stage3.byte_6bbe34 == 0);
      CHECK(retained.remaining == 15 && retained.alternate == 1);
      CHECK(!state->retained.stage3.words_6bbe2c[0] && !state->retained.stage3.words_6bbe2c[1]);
      CHECK(profile || state->next_mode == 1);
      all_pcm = hash(all_pcm, &sink.hash, sizeof(sink.hash));
      ++completed;
      printf("profile group=%u background=%u previous=%d frames=%u keys=%u menus=%u\n",
          group, profile, previous, frame, input.calls, local_menu);
      fflush(stdout);
      bk_system_audio_destroy(confirm); confirm = NULL;
      bk_system_audio_destroy(hover); hover = NULL;
      bk_ending_audio_destroy(audio); audio = NULL;
      bk_audio_destroy(mixer); mixer = NULL;
      bk_ending_secondary_assets_destroy(assets); assets = NULL;
    }
  }
  CHECK(completed == 10 && visited == 255 && menus == 50 && frozen == 120);
  CHECK(control_only || (speech_cases == 105 && pauses == 315 && wraps >= 30));
  printf("PASS secondary controller entries=%u frames=%u menus=%u states=%u "
         "frozen_pcm=%u speeches=%u paused_polls=%u wraps=%u geometry=%016llx pcm=%016llx\n",
      completed, total, menus, visited, frozen, speech_cases, pauses, wraps,
      (unsigned long long)all_geometry, (unsigned long long)all_pcm);
  rc = 0;
done:
  if (catalog) fclose(catalog);
  bk_system_audio_destroy(confirm);
  bk_system_audio_destroy(hover);
  bk_ending_audio_destroy(audio);
  bk_audio_destroy(mixer);
  bk_ending_secondary_assets_destroy(assets);
  bk_resources_destroy(store);
  free(state);
  return rc;
}
