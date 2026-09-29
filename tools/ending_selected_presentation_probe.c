#include "scene/ending_selected_presentation.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Explicit real-resource scheduling fixtures. Input modes and clip requests
 * below exercise recovered branches; this is not a natural playthrough.
 * PCM windows are independently executed by the original4ad5a4 oracle. */
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "selected presentation line %d: %s: %s\n", \
    __LINE__, #x, e); goto done; } } while (0)
typedef struct { uint64_t submitted, consumed, hash; } Sink;
typedef struct { uint32_t now, calls; int fail_at; } Clock;
static uint64_t digest(uint64_t h, const void *data, size_t size) {
  const uint8_t *p = data;
  for (size_t i = 0; i < size; ++i) h = (h ^ p[i]) * UINT64_C(1099511628211);
  return h;
}
static int submit(void *p, const int16_t *pcm, size_t n, char e[256]) {
  (void)e;
  Sink *s = p;
  s->hash = digest(s->hash, pcm, n * 2 * sizeof(*pcm));
  s->submitted += n;
  return 1;
}
static int poll(void *p, uint64_t *n, char e[256]) {
  (void)e;
  *n = ((Sink *)p)->consumed;
  return 1;
}
static int clock_read(void *p, uint32_t *now, char e[256]) {
  Clock *c = p;
  if ((int)c->calls == c->fail_at) {
    snprintf(e, 256, "injected presentation clock failure");
    return 0;
  }
  *now = c->now + c->calls++;
  return 1;
}
static uint32_t bits(float f) { uint32_t n; memcpy(&n, &f, 4); return n; }
static int window(FILE *out, BkAudio *mixer, unsigned group, unsigned variant,
                      unsigned step, BkEndingVoiceEnvelope before,
                      BkEndingVoiceEnvelope after, uint32_t random,
                      unsigned clocks, char e[256]) {
  BkAudioCursor cursor;
  int playing;
  if (!bk_audio_playing(mixer, 0, &playing) || !bk_audio_cursor(mixer, 0, &cursor))
    return 0;
  uint32_t bytes = cursor.pcm
      ? (uint32_t)(bk_pcm_frames(cursor.pcm) * bk_pcm_channels(cursor.pcm) * 2) : 4096;
  uint32_t offset = cursor.pcm
      ? (uint32_t)(cursor.source_frame * bk_pcm_channels(cursor.pcm) * 2) : 0;
  int sample = cursor.playing && cursor.pcm && cursor.buffered &&
      bytes >= 443 && offset < bytes - 443;
  uint32_t words[14] = {group, variant, step, (uint32_t)playing,
      (uint32_t)cursor.playing, (uint32_t)!sample, bytes, offset,
      bits(before.target), bits(before.smoothed), bits(after.target),
      bits(after.smoothed), random, clocks};
  uint8_t raw[496] = {0};
  for (unsigned i = 0; i < 14; ++i)
    for (unsigned j = 0; j < 4; ++j) raw[i*4+j] = (uint8_t)(words[i] >> (j*8));
  if (sample) {
    const int16_t *pcm = bk_pcm_samples(cursor.pcm) + offset / 2;
    for (unsigned i = 0; i < 220; ++i) {
      raw[56+i*2] = (uint8_t)pcm[i];
      raw[57+i*2] = (uint8_t)((uint16_t)pcm[i] >> 8);
    }
  }
  if (fwrite(raw, sizeof(raw), 1, out) != 1) {
    snprintf(e, 256, "cannot write consumed PCM record");
    return 0;
  }
  return 1;
}
int main(int argc, char **argv) {
  if (argc < 4 || argc > 5) return 2;
  unsigned limit = argc == 5 ? (unsigned)strtoul(argv[4], NULL, 10) : 30;
  if (!limit || limit > 30) return 2;
  int rc = 1;
  char e[256] = {0}, path[1024];
  BkResourceStore *store = NULL;
  BkEndingSelectedAssets *assets = NULL, *old = NULL;
  BkEndingAudio *audio = NULL;
  BkAudio *mixer = NULL;
  BkEndingState *state = calloc(1, sizeof(*state));
  FILE *records = NULL, *report = NULL;
  uint64_t geometry = UINT64_C(14695981039346656037), face_hash = geometry;
  uint64_t state_hash = geometry, pcm_hash = geometry;
  unsigned entries = 0, frames = 0, requests = 0, voiced = 0, fixed = 0;
  unsigned automatic = 0, controlled = 0, frozen = 0, retained = 0, replaced = 0;
  unsigned rejected = 0, prefix_failures = 0;
  BkEndingVoiceEnvelope voice = {0};
  BkEndingSelectedControlState controller = bk_ending_selected_control_initial();
  BkEndingSelectedCycle cycle = bk_ending_selected_cycle_initial();
  BkEndingAuxiliaryCycle shared = bk_ending_auxiliary_cycle_initial();
  uint32_t random = 0x494015;
  CHECK(state && (store = bk_resources_create(e)));
  const char *packs[] = {"bk3_10", "bk3_13", "bk3_03", "bk3_04", "fambom",
                         "bk3_02", "bk3_06"};
  for (unsigned i = 0; i < sizeof(packs) / sizeof(*packs); ++i) {
    snprintf(path, sizeof(path), "%s/%s.pp", argv[1], packs[i]);
    CHECK(bk_resources_mount(store, packs[i], path, e));
  }
  CHECK(records = fopen(argv[3], "wb"));
  CHECK(fwrite("BK3EP001", 8, 1, records) == 1);
  for (unsigned profile = 0; profile < limit; ++profile) {
    unsigned group = profile / 6, variant = profile / 3 % 2, selection = profile % 3;
    BkEndingSelectedConfig config;
    CHECK(bk_ending_selected_config(&config, group, variant, selection));
    bk_ending_state_initialize(state);
    Sink sink = {.hash = UINT64_C(14695981039346656037)};
    BkAudioSink output = {&sink, 48000, 480, 1920, submit, poll};
    CHECK(mixer = bk_audio_create(&output, e));
    CHECK(audio = bk_ending_audio_create_entry(store, mixer, 0, group, variant, -1600, e));
    CHECK(bk_ending_audio_bind(audio, 0, "bk3_06", "PH11201.wav", e));
    BkEndingAudioCall play = {.operation = BK_ENDING_AUDIO_RESTART, .slot = 0,
                               .flags = 1, .volume = -700};
    int playing;
    CHECK(bk_ending_audio_call(audio, group, variant, selection, &play, &playing, e));
    CHECK(bk_audio_fill(mixer, e));
    for (unsigned reload = 0; reload < 2; ++reload) {
      old = assets; assets = NULL;
      BkEndingBackgroundAssets *background = old ? bk_ending_selected_assets_background(old) : NULL;
      BkEndingSelectedLoad load = {group, variant, selection, reload ? 2 : 0,
                                    config.primary, background};
      BkMenuCamera camera = {0};
      for (unsigned i = 0; i < 4; ++i) camera.pose.world[i*5] = camera.matrix[i*5] = 1;
      BkEndingCameraPresets presets;
      uint32_t clocks[4] = {100, 101, 102, 103};
      BkEndingSelectedControlState held_controller = controller;
      BkEndingSelectedCycle held_cycle = cycle;
      BkEndingAuxiliaryCycle held_shared = shared;
      BkEndingVoiceEnvelope held_voice = voice;
      CHECK(assets = bk_ending_selected_assets_create(store, &load, clocks, &random,
                                                      &camera, &presets, e));
      CHECK(bk_ending_selected_assets_load_background(assets, store, e));
      CHECK(!memcmp(&controller, &held_controller, sizeof(controller)) &&
            !memcmp(&cycle, &held_cycle, sizeof(cycle)) &&
            !memcmp(&shared, &held_shared, sizeof(shared)) &&
            !memcmp(&voice, &held_voice, sizeof(voice)));
      if (reload) {
        CHECK(bk_ending_selected_assets_background(assets) == background);
        ++retained;
      }
      replaced += bk_ending_selected_assets_replaced_background(assets) != 0;
      bk_ending_selected_assets_destroy(old); old = NULL;
      BkActorForest *forest = bk_ending_selected_assets_forest(assets);
      BkActorPose *primary = bk_ending_selected_assets_pose(assets, 0);
      state->frame.group = (uint8_t)group;
      state->frame.phase = config.phase;
      state->control.variant = (uint8_t)config.event;
      state->auxiliary.variant = (int32_t)variant;
      state->auxiliary.selection = (int32_t)selection;
      uint32_t secondary = 0;
      if (group == 2 && variant) {
        uint32_t node = bk_ending_selected_assets_special(assets);
        if (node != BK_MODEL_NONE) secondary = bk_actor_forest_node(forest, 0, node);
      }
      int32_t scheduled = 0, voice_volume = -700, effect_volume = -1000;
      Clock clock = {.now = 1000, .fail_at = -1};
      BkEndingSelectedPresentationScene scene = {assets, audio, &voice, &controller,
          &cycle, &shared, &random, &scheduled, &voice_volume, &effect_volume,
          &secondary, &clock, clock_read};
      BkClipState tracks[2], after;
      for (unsigned i = 0; i < 2; ++i)
        CHECK(bk_actor_pose_state(bk_ending_selected_assets_pose(assets, i+1), tracks+i));
      for (unsigned step = 0; step < 120; ++step) {
        unsigned block = step / 10;
        unsigned first = group == 1 && config.event == 2 ? 7 : 6;
        unsigned second = (group == 0 && config.event == 4) ||
            (group == 1 && (config.event == 2 || config.event == 7 ||
                           config.event == 3 || config.event == 4)) ? 11 : 10;
        const unsigned clips[] = {1, 1, 6, 7, 13, 14, 15, 16, 17, 18, first, second};
        const int32_t modes[] = {0, 0, 1, 2, 4, 6, 4, 6, 5, 7, 2, 2};
        state->frame.auxiliary_mode = block >= 10;
        state->auxiliary.gate = block ? 3 : 1;
        state->ui_controller.auxiliary.mode = modes[block];
        state->auxiliary.progress = block == 10 ? .65f : .85f;
        state->auxiliary.expression_a = 6;
        state->auxiliary.expression_b = step % 31 < 16 ? 3 : 4;
        state->face_mode = step % 19 < 7;
        state->control.toggles[0] = step % 17 < 3;
        state->control.toggles[1] = step % 23 < 4;
        state->control.toggles[3] = step % 13 < 5;
        state->control.toggles[5] = step % 11 >= 5;
        scheduled = block == 3;
        if (!(step % 10)) {
          CHECK(bk_actor_pose_select(primary, clips[block], 0, e));
          ++requests;
          if (block >= 10) {
            BkClipTiming timing;
            CHECK(bk_actor_pose_timing(primary, clips[block], &timing));
            BkClipEdit edit = {.slot = clips[block], .fields = BK_CLIP_EDIT_SOURCE,
                                .source = timing.end - .01f};
            CHECK(bk_actor_pose_edit_clips(primary, &edit, 1, e));
            CHECK(bk_audio_pause(mixer, 0, e));
          }
        }
        int freeze = step % 24 >= 16 && step % 24 < 20;
        if (freeze) ++frozen;
        else sink.consumed += 480;
        if (sink.consumed > sink.submitted) sink.consumed = sink.submitted;
        CHECK(bk_audio_poll(mixer, e));
        clock.now = reload ? UINT32_MAX - 500u + step * 17u : 1000 + step * 17;
        clock.calls = 0;
        BkEndingVoiceEnvelope before = voice;
        CHECK(bk_ending_selected_presentation_scene_step(&scene, state, 1.f/60, e));
        CHECK(bk_actor_pose_state(primary, &after));
        int force = group == 4 && variant &&
            (after.slot == 9 || after.slot == 10 || after.slot == 11 || after.slot == 13 ||
             (selection != 1 && (after.slot == 15 || after.slot == 16 || after.slot == 17 ||
                                after.slot == 18 || after.slot == 20 || after.slot == 21)));
        if (force) { CHECK(!memcmp(&before, &voice, sizeof(voice))); ++fixed; }
        else {
          CHECK(window(records, mixer, group, variant, reload*120+step,
                         before, voice, random, clock.calls, e));
          ++voiced;
        }
        for (unsigned i = 0; i < 2; ++i) {
          CHECK(bk_actor_pose_state(bk_ending_selected_assets_pose(assets, i+1), &after));
          CHECK(!memcmp(tracks+i, &after, sizeof(after)));
        }
        for (unsigned i = 0; i < 4; ++i) {
          size_t count = 0;
          const float *world = bk_actor_pose_world(bk_ending_selected_assets_pose(assets, i), &count);
          CHECK(world);
          for (size_t j = 0; j < count; ++j) CHECK(isfinite(world[j]));
          geometry = digest(geometry, world, count * sizeof(*world));
        }
        face_hash = digest(face_hash, bk_ending_selected_assets_face_state(assets), sizeof(BkFaceState));
        const uint32_t words[] = {random, bits(shared.scale), (uint32_t)shared.countdown,
            (uint32_t)cycle.slow, (uint32_t)cycle.sampled, (uint32_t)cycle.speech_blocked,
            bits(cycle.speech_elapsed), (uint32_t)cycle.countdown,
            (uint32_t)state->retained.auxiliary.word_5546a0,
            (uint32_t)state->retained.auxiliary.word_6ea354,
            (uint32_t)state->auxiliary.expression_a, (uint32_t)state->auxiliary.expression_b,
            (uint32_t)state->eye_lower, (uint32_t)controller.expression_override};
        state_hash = digest(state_hash, words, sizeof(words));
        automatic += block >= 10;
        controlled += block >= 1 && block <= 3;
        CHECK(bk_audio_fill(mixer, e));
        ++frames;
      }
      BkEndingState saved = *state;
      uint32_t held_random = random;
      held_cycle = cycle; held_shared = shared; held_controller = controller; held_voice = voice;
      BkClipState before[4];
      for (unsigned i = 0; i < 4; ++i)
        CHECK(bk_actor_pose_state(bk_ending_selected_assets_pose(assets, i), before+i));
      clock.calls = 0; clock.fail_at = 0;
      CHECK(!bk_ending_selected_presentation_scene_step(&scene, state, 1.f/60, e));
      CHECK(!memcmp(state, &saved, sizeof(saved)) && random == held_random &&
            !memcmp(&cycle, &held_cycle, sizeof(cycle)) &&
            !memcmp(&shared, &held_shared, sizeof(shared)) &&
            !memcmp(&controller, &held_controller, sizeof(controller)) &&
            !memcmp(&voice, &held_voice, sizeof(voice)));
      for (unsigned i = 0; i < 4; ++i) {
        CHECK(bk_actor_pose_state(bk_ending_selected_assets_pose(assets, i), &after));
        CHECK(!memcmp(before+i, &after, sizeof(after)));
      }
      ++rejected;
      BkEndingSelectedPresentationScene missing = scene;
      missing.secondary_node = NULL;
      CHECK(!bk_ending_selected_presentation_scene_step(&missing, state, 1.f/60, e));
      CHECK(!memcmp(state, &saved, sizeof(saved)) && random == held_random);
      ++rejected;
      if (reload) {
        uint32_t root = bk_ending_selected_assets_root(assets, 3), actor, frame, hidden;
        CHECK(bk_actor_forest_visibility(forest, root, 0, e));
        state->control.toggles[0] = 1;
        state->frame.auxiliary_mode = 0;
        clock.calls = 0; clock.fail_at = 1;
        CHECK(!bk_ending_selected_presentation_scene_step(&scene, state, .25f, e));
        CHECK(bk_actor_forest_binding(forest, root, &actor, &frame));
        CHECK(bk_actor_pose_hidden(bk_ending_selected_assets_pose(assets, actor), frame, &hidden));
        CHECK(hidden == 1); /*Native model/hide prefix precedes the later face clock.*/
        ++prefix_failures;
      }
      ++entries;
    }
    pcm_hash = digest(pcm_hash, &sink.hash, sizeof(sink.hash));
    bk_ending_selected_assets_destroy(assets); assets = NULL;
    bk_ending_audio_destroy(audio); audio = NULL;
    bk_audio_destroy(mixer); mixer = NULL;
  }
  CHECK(entries == limit*2 && frames == entries*120 && requests == entries*12 &&
        voiced+fixed == frames && retained == limit && rejected == entries*2 &&
        prefix_failures == limit && automatic == entries*20 && controlled == entries*30);
  CHECK(report = fopen(argv[2], "w"));
  CHECK(fprintf(report, "{\"passed\":true,\"profiles\":%u,\"entries\":%u,\"frames\":%u,"
      "\"requests\":%u,\"pcm_frames\":%u,\"fixed_mouth_frames\":%u,"
      "\"automatic_frames\":%u,\"controlled_frames\":%u,\"frozen_frames\":%u,"
      "\"retained_backgrounds\":%u,\"replacements\":%u,\"atomic_rejections\":%u,"
      "\"failure_prefixes\":%u,\"geometry_hash\":\"%016llx\",\"face_hash\":\"%016llx\","
      "\"state_hash\":\"%016llx\",\"pcm_hash\":\"%016llx\"}\n",
      limit, entries, frames, requests, voiced, fixed, automatic, controlled, frozen,
      retained, replaced, rejected, prefix_failures, (unsigned long long)geometry,
      (unsigned long long)face_hash, (unsigned long long)state_hash,
      (unsigned long long)pcm_hash) > 0);
  printf("PASS selected presentation scene: %u entries %u frames %u PCM frames; geometry=%016llx state=%016llx pcm=%016llx\n",
      entries, frames, voiced, (unsigned long long)geometry,
      (unsigned long long)state_hash, (unsigned long long)pcm_hash);
  rc = 0;
done:
  if (records && fclose(records)) rc = 1;
  if (report && fclose(report)) rc = 1;
  bk_ending_selected_assets_destroy(old);
  bk_ending_selected_assets_destroy(assets);
  bk_ending_audio_destroy(audio);
  bk_audio_destroy(mixer);
  bk_resources_destroy(store);
  free(state);
  return rc;
}
