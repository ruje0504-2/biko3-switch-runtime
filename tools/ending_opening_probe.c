#include "scene/ending_opening.h"
#include "scene/ending_presentation.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
/* Real resource composition of the recovered state4 parent and4df6c0.
 * Entry values/input/consumption are explicit fixtures; this is not a test
 * of the unrecovered parent states, common UI or full application lifecycle.
 */
#define CHECK(x)                                                               \
  do {                                                                         \
    if (!(x)) {                                                                \
      fprintf(stderr, "opening probe line %d: %s\n", __LINE__, e);            \
      goto done;                                                               \
    }                                                                          \
  } while (0)
typedef struct {
  uint64_t submitted, consumed, hash;
} Sink;
typedef struct {
  uint32_t now, frame, calls, skip_code, skip_frame;
} Input;
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
  if (mode != 1) {
    snprintf(e, 256, "unexpected opening key mode");
    return 0;
  }
  ++i->calls;
  *out = code == i->skip_code && i->frame >= i->skip_frame ? 257 : 256;
  return 1;
}
static uint32_t root(BkEndingNormalAssets *a, unsigned actor) {
  const BkModel *m = bk_actor_pose_model(bk_ending_normal_assets_pose(a, actor));
  if (!m)
    return BK_FRAME_NONE;
  for (uint32_t i = 0; i < m->frame_count; ++i)
    if (m->frames[i].parent_index == BK_MODEL_NONE)
      return bk_actor_forest_node(bk_ending_normal_assets_forest(a), actor, i);
  return BK_FRAME_NONE;
}
static uint64_t matrices(BkActorPose *pose, int local) {
  const BkModel *model = bk_actor_pose_model(pose);
  uint64_t value = UINT64_C(14695981039346656037);
  for (uint32_t i = 0; i < model->frame_count; ++i) {
    if (local)
      value = hash(value, bk_actor_pose_local(pose, i), 16 * sizeof(float));
    else {
      value = hash(value, bk_actor_pose_frame(pose, i), 16 * sizeof(float));
      value = hash(value, bk_actor_pose_parent_world(pose, i), 16 * sizeof(float));
    }
  }
  return value;
}
int main(int argc, char **argv) {
  if (argc != 2)
    return 2;
  char e[256] = {0}, path[1024];
  BkResourceStore *store = bk_resources_create(e);
  BkEndingNormalAssets *assets = NULL;
  BkEndingAudio *audio = NULL;
  BkAudio *mixer = NULL;
  BkMaterialPose *materials = NULL;
  int rc = 1;
  unsigned total = 0, completed = 0, held = 0, keys = 0;
  unsigned speech_wait[2] = {0}, stages[5] = {0};
  uint64_t geometry = UINT64_C(14695981039346656037), pcm = geometry;
  BkEndingOpeningRetained retained = {1};
  BkEndingPresentationRetained presentation_retained = {0};
  uint32_t random = 0x4db608;
  CHECK(store);
  const char *packs[] = {"bk3_08", "bk3_04", "bk3_03", "fambom", "bk3_02", "bk3_06"};
  for (unsigned i = 0; i < sizeof(packs) / sizeof(*packs); ++i) {
    snprintf(path, sizeof(path), "%s/%s.pp", argv[1], packs[i]);
    CHECK(bk_resources_mount(store, packs[i], path, e));
  }
  for (unsigned group = 0; group < 5; ++group) {
    for (unsigned profile = 0; profile < 4; ++profile) {
      unsigned variant = profile & 1;
      uint32_t clocks[4] = {100, 100, 100, 100};
      BkMenuCamera camera = {0};
      CHECK(bk_menu_camera_dialogue(&camera));
      BkEndingCameraPresets presets;
      BkEndingCameraTransitions transitions = {0};
      assets = bk_ending_normal_assets_create(store, group, variant, clocks,
                                               &random, &camera, &presets, e);
      CHECK(assets && bk_ending_normal_assets_load_background(assets, store, e));
      materials = bk_material_pose_create(bk_actor_pose_model(
          bk_ending_normal_assets_pose(assets, 0)), e);
      CHECK(materials);
      Sink sink = {.hash = UINT64_C(14695981039346656037)};
      BkAudioSink output = {&sink, 48000, 480, 1920, submit, poll};
      mixer = bk_audio_create(&output, e);
      CHECK(mixer);
      audio = bk_ending_audio_create(store, mixer, 0, e);
      CHECK(audio);
      BkEndingFrameState frame = {.group = (uint8_t)group, .phase = 1,
                                   .state_721ee0 = 4, .camera_mode = 5};
      BkEndingControlState control = {0};
      const BkEndingNormalConfig *config = bk_ending_normal_assets_config(assets);
      memcpy(frame.camera_table, config->camera_table, sizeof(frame.camera_table));
      for (unsigned i = 0; i < 3; ++i)
        memcpy(control.targets[i], bk_ending_normal_assets_target(assets, i), 12);
      memcpy(frame.camera_values, control.targets[0], sizeof(frame.camera_values));
      control.toggles[1] = control.toggles[5] = 1;
      control.toggles[3] = group != 1;
      control.toggles[7] = profile != 1;
      BkEndingAuxiliaryState aux = {.variant = (int32_t)variant,
          .expression_a = config->expression_a, .expression_b = config->expression_b};
      int32_t substate = 0, wait = 0, next = profile == 2, volume = -600;
      int32_t selected = group == 1, ready = 0, flip = 0, disabled[64] = {0};
      uint32_t primary = root(assets, 0), secondary = root(assets, 1);
      uint32_t background = root(assets, 4), absent = 0;
      CHECK(primary != BK_FRAME_NONE && secondary != BK_FRAME_NONE && background != BK_FRAME_NONE);
      int8_t previous = profile == 1 ? 0x18 : 8;
      char name[32] = {0};
      BkEndingOpeningBindings bindings = {&frame, &control, &aux, &camera,
          &substate, &wait, &next, &previous, &volume, name, &retained};
      Input input = {.now = profile == 3 ? UINT32_MAX - 500 : 1000,
                     .skip_code = profile == 3 ? 1 : 0,
                     .skip_frame = profile & 1 ? 10 : UINT32_MAX};
      BkEndingOpeningScene scene = {assets, audio, &transitions, &presets,
                                     &selected, {&input, key}};
      BkEndingPresentationScene presentation = {assets, audio, materials,
          disabled, bk_bom_assets_count(bk_ending_normal_assets_bom(assets)),
          &presentation_retained, &random, &input, clock_read};
      CHECK(presentation.bom_count <= 64);
      /* Direct manipulation is not used by state4; absent is the actual
       * post-leave value, not a guessed BOM reference for later state3. */
      BkEndingPresentationBindings display = {&frame, &aux, &ready,
          control.toggles, &primary, &background, &secondary, &absent, &absent,
          &flip};
      BkEndingFrameInput pointer = {0};
      BkActorForest *forest = bk_ending_normal_assets_forest(assets);
      unsigned hold_remaining = 48, profile_frames = 0;
      BkClipState main_initial = {0}, main_after = {0};
      CHECK(bk_actor_pose_state(bk_ending_normal_assets_pose(assets, 2), &main_initial));
      CHECK(bk_audio_fill(mixer, e));
      while (frame.state_721ee0 == 4 && profile_frames < 6000) {
        int active[2] = {0};
        for (unsigned slot = 0; slot < 2; ++slot) {
          BkEndingAudioCall call = {.operation = BK_ENDING_AUDIO_STATUS, .slot = slot};
          CHECK(bk_ending_audio_call(audio, group, variant, 0, &call, active + slot, e));
        }
        int hold = substate == 3 && active[0] && hold_remaining;
        if (hold) {
          --hold_remaining;
          ++held;
        } else {
          uint64_t consume = sink.consumed + 800;
          sink.consumed = consume < sink.submitted ? consume : sink.submitted;
        }
        CHECK(bk_audio_poll(mixer, e));
        int32_t before_stage = substate;
        CHECK(before_stage >= 0 && before_stage < 5);
        ++stages[before_stage];
        uint64_t old_world[5], old_local[5];
        BkClipState old_clip[5] = {0};
        for (unsigned actor = 0; actor < 5; ++actor) {
          BkActorPose *pose = bk_ending_normal_assets_pose(assets, actor);
          old_world[actor] = matrices(pose, 0);
          old_local[actor] = matrices(pose, 1);
          CHECK(bk_actor_pose_state(pose, old_clip + actor));
        }
        const float *old = bk_actor_forest_world(forest,
            bk_actor_forest_node(forest, 0, bk_ending_normal_assets_node(assets, 5)));
        CHECK(old);
        float target[3];
        memcpy(target, old + 12, sizeof(target));
        float seconds = profile == 3 && profile_frames % 17 == 0 ? 0
                         : profile == 3 && profile_frames % 31 == 0 ? .25f
                         : 1.f / 60;
        input.frame = profile_frames;
        input.now += hold ? 1000 : 17;
        CHECK(bk_ending_opening_scene_step(&scene, &bindings, seconds, e));
        if (hold)
          CHECK(frame.state_721ee0 == 4 && substate == 3 && wait);
        for (unsigned actor = 0; actor < 5; ++actor) {
          BkActorPose *pose = bk_ending_normal_assets_pose(assets, actor);
          CHECK(matrices(pose, 0) == old_world[actor]);
          if (actor != 3 || before_stage != 1) {
            BkClipState after = {0};
            CHECK(bk_actor_pose_state(pose, &after));
            CHECK(!memcmp(old_clip + actor, &after, sizeof(after)));
            CHECK(matrices(pose, 1) == old_local[actor]);
          }
        }
        if ((before_stage == 1 && substate == 2) || before_stage == 4)
          CHECK(!memcmp(frame.camera_values, target, sizeof(target)));
        if (before_stage == 1 && substate == 2)
          CHECK(!memcmp(control.saved_camera, camera.matrix, sizeof(camera.matrix)));
        if (before_stage == 2 && substate == 3 && !next) {
          char expected[32];
          snprintf(expected, sizeof(expected), "PH%u0001.wav", group + 1);
          CHECK(!strcmp(name, expected) && wait && !aux.pending);
        }
        if (before_stage == 3)
          for (unsigned slot = 0; slot < 2; ++slot)
            speech_wait[slot] += active[slot] != 0;
        CHECK(bk_ending_presentation_scene_step(&presentation, &display,
                                                &pointer, seconds, e));
        CHECK(bk_actor_pose_state(bk_ending_normal_assets_pose(assets, 2), &main_after));
        CHECK(!memcmp(&main_initial, &main_after, sizeof(main_after)));
        if (frame.camera_mode == 0) {
          float offset[3];
          memcpy(offset, frame.camera_values, sizeof(offset));
          const uint32_t tracks[2] = {2, 3};
          CHECK(bk_ending_camera_assets_step(
              bk_ending_normal_assets_cameras(assets), forest, tracks, &camera,
              BK_ENDING_CAMERA_ORBIT, offset, (float[2]){0, 0}, 0,
              BK_FRAME_NONE, seconds, e));
        }
        const uint32_t roots[3] = {primary, secondary, background};
        for (unsigned i = 0; i < 3; ++i) {
          const BkFrameVisit *visits = NULL;
          uint32_t count = 0;
          CHECK(bk_actor_forest_draw(forest, roots[i], &visits, &count, e));
        }
        for (unsigned actor = 0; actor < 5; ++actor) {
          uint64_t value = matrices(bk_ending_normal_assets_pose(assets, actor), 0);
          geometry = hash(geometry, &value, sizeof(value));
        }
        CHECK(isfinite(camera.fov) && camera.fov > 0);
        CHECK(bk_audio_fill(mixer, e));
        ++profile_frames;
        ++total;
      }
      CHECK(frame.state_721ee0 == 0 && substate == 3 && !wait);
      CHECK(profile == 2 || hold_remaining == 0);
      CHECK(!next || (retained.fov == 1 && !name[0]));
      ++completed;
      keys += input.calls;
      pcm = hash(pcm, &sink.hash, sizeof(sink.hash));
      printf("profile group=%u variant=%u previous=%d next=%d frames=%u keys=%u\n",
             group, variant, previous, next, profile_frames, input.calls);
      CHECK(bk_ending_audio_stop(audio, e));
      bk_ending_audio_destroy(audio); audio = NULL;
      bk_audio_destroy(mixer); mixer = NULL;
      bk_material_pose_destroy(materials); materials = NULL;
      bk_ending_normal_assets_destroy(assets); assets = NULL;
    }
  }
  CHECK(completed == 20 && speech_wait[0] && speech_wait[1] && held == 720);
  printf("PASS ending opening entries=%u frames=%u held_pcm_frames=%u keys=%u "
         "stages=%u,%u,%u,%u,%u speech_wait=%u,%u geometry=%016llx pcm=%016llx\n",
         completed, total, held, keys, stages[0], stages[1], stages[2], stages[3],
         stages[4], speech_wait[0], speech_wait[1],
         (unsigned long long)geometry, (unsigned long long)pcm);
  rc = 0;
done:
  if (audio)
    bk_ending_audio_stop(audio, e);
  bk_ending_audio_destroy(audio);
  bk_audio_destroy(mixer);
  bk_material_pose_destroy(materials);
  bk_ending_normal_assets_destroy(assets);
  bk_resources_destroy(store);
  return rc;
}
