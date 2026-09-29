#include "scene/ending_presentation.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
/* Actual-resource fixture, not natural parent-controller progression. The
 * binary records retain the real consumed PCM window and envelope states
 * for independent4ad363/4ad5a4 execution, rather than recomputing its formula
 * here. No source assets are written. */
#define CHECK(x)                                                               \
  do {                                                                         \
    if (!(x)) {                                                                \
      fprintf(stderr, "line %d: %s\n", __LINE__, e);                           \
      goto done;                                                               \
    }                                                                          \
  } while (0)
typedef struct {
  uint64_t submitted, consumed, hash;
} Sink;
typedef struct {
  uint32_t now, calls;
  int failure;
} Clock;
static uint64_t hash(uint64_t h, const void *data, size_t size) {
  const uint8_t *p = data;
  for (size_t i = 0; i < size; ++i)
    h = (h ^ p[i]) * UINT64_C(1099511628211);
  return h;
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
static int clock_read(void *p, uint32_t *milliseconds, char e[256]) {
  Clock *c = p;
  if ((int)c->calls == c->failure) {
    snprintf(e, 256, "injected clock failure at %u", c->calls);
    return 0;
  }
  *milliseconds = c->now + c->calls++;
  return 1;
}
static uint32_t root(BkEndingNormalAssets *a, unsigned index) {
  const BkModel *m = bk_actor_pose_model(bk_ending_normal_assets_pose(a, index));
  for (uint32_t i = 0; i < m->frame_count; ++i)
    if (m->frames[i].parent_index == BK_MODEL_NONE)
      return bk_actor_forest_node(bk_ending_normal_assets_forest(a), index, i);
  return BK_FRAME_NONE;
}
static int write_record(FILE *file, const uint32_t values[14],
                        const uint8_t samples[440]) {
  uint8_t words[56];
  for (unsigned i = 0; i < 14; ++i)
    for (unsigned j = 0; j < 4; ++j)
      words[i * 4 + j] = (uint8_t)(values[i] >> (j * 8));
  return fwrite(words, sizeof(words), 1, file) == 1 &&
         fwrite(samples, 440, 1, file) == 1;
}
static uint32_t float_bits(float x) {
  uint32_t out;
  memcpy(&out, &x, sizeof(out));
  return out;
}
int main(int argc, char **argv) {
  if (argc != 3)
    return 2;
  char e[256] = {0}, path[1024];
  int rc = 1;
  FILE *records = fopen(argv[2], "wb");
  BkResourceStore *store = bk_resources_create(e);
  BkEndingNormalAssets *assets = NULL;
  BkEndingAudio *audio = NULL;
  BkAudio *mixer = NULL;
  BkMaterialPose *materials = NULL;
  uint64_t geometry = UINT64_C(14695981039346656037);
  uint64_t pcm_hash = geometry;
  unsigned frames = 0, rejections = 0, voice_frames = 0, manual_frames = 0;
  CHECK(records && store);
  CHECK(fwrite("BK3EP001", 8, 1, records) == 1);
  const char *packs[] = {"bk3_08", "bk3_04", "bk3_03", "fambom", "bk3_02", "bk3_06"};
  for (unsigned i = 0; i < sizeof(packs) / sizeof(*packs); ++i) {
    snprintf(path, sizeof(path), "%s/%s.pp", argv[1], packs[i]);
    CHECK(bk_resources_mount(store, packs[i], path, e));
  }
  for (unsigned group = 0; group < 5; ++group)
    for (unsigned variant = 0; variant < 2; ++variant) {
      uint32_t random = 101 + group * 2 + variant;
      uint32_t clocks[4] = {100, 100, 100, 100};
      BkMenuCamera camera = {0};
      for (unsigned i = 0; i < 4; ++i)
        camera.matrix[i * 5] = camera.pose.world[i * 5] = 1;
      BkEndingCameraPresets presets;
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
      CHECK(audio && bk_ending_audio_bind(audio, 0, "bk3_06", "PH11201.wav", e));
      BkEndingAudioCall play = {.operation = BK_ENDING_AUDIO_RESTART,
          .slot = 0, .flags = 1, .volume = -500};
      int started = 0;
      CHECK(bk_ending_audio_call(audio, group, variant, 0, &play, &started, e));
      CHECK(bk_audio_fill(mixer, e));
      BkEndingFrameState f = {.phase = 1, .state_721ee0 = 3, .group = (uint8_t)group};
      BkEndingAuxiliaryState aux = {.variant = (int32_t)variant};
      int32_t ready = 1, flip = 1, disabled[4] = {-8, -9, -10, -11};
      uint8_t toggles[8] = {0};
      uint32_t primary = root(assets, 0), secondary = root(assets, 1), background = root(assets, 4);
      CHECK(primary != BK_FRAME_NONE && secondary != BK_FRAME_NONE && background != BK_FRAME_NONE);
      BkBomAssets *bom = bk_ending_normal_assets_bom(assets);
      const BkBomAssetBinding *first = bk_bom_assets_binding(bom, 0);
      CHECK(first);
      BkActorForest *forest = bk_ending_normal_assets_forest(assets);
      uint32_t direct_reference = bk_actor_forest_node(forest, 0, first->parent);
      uint32_t direct_node = bk_actor_forest_node(forest, 0, first->reference);
      CHECK(direct_reference != BK_FRAME_NONE && direct_node != BK_FRAME_NONE);
      BkEndingPresentationBindings b = {&f, &aux, &ready, toggles, &primary,
          &background, &secondary, &direct_reference, &direct_node, &flip};
      BkEndingPresentationRetained retained = {0};
      Clock clock = {.now = 1000, .failure = -1};
      BkEndingPresentationScene scene = {assets, audio, materials, disabled,
          bk_bom_assets_count(bom), &retained, &random, &clock, clock_read};
      BkEndingFrameInput input = {0};
      BkClipState tracks[2], after;
      for (unsigned i = 0; i < 2; ++i)
        CHECK(bk_actor_pose_state(bk_ending_normal_assets_pose(assets, 2 + i), tracks + i));
      for (unsigned step = 0; step < 180; ++step) {
        clock.now = step < 90 ? 1000 + step * 17 : UINT32_MAX - 700 + (step - 90) * 17;
        clock.calls = 0;
        f.phase = step % 9 ? 1 : 2;
        f.state_721ee0 = step % 6 ? 3 : 4;
        const int32_t caches[] = {9, 10, 11, 12, 26, 27};
        f.camera_cached = caches[step % 6];
        ready = step % 13 ? 1 : 6;
        aux.expression_a = (int32_t)(step % 10);
        aux.expression_b = step % 30 < 15 ? 3 : 1;
        toggles[0] = step % 12 < 6 ? 0 : 2;
        toggles[3] = step % 10 < 5 ? 0 : 255;
        toggles[5] = step % 7 < 4 ? 0 : 1;
        flip = (int32_t)(step % 2);
        input.words[6] = (uint32_t)((int32_t)(step % 5) - 2);
        input.words[7] = (uint32_t)((int32_t)(step % 7) - 3);
        CHECK(bk_actor_forest_visibility(forest, secondary, step % 11 == 0, e));
        if (step % 60 == 20)
          CHECK(bk_audio_pause(mixer, 0, e));
        if (step % 60 == 25)
          CHECK(bk_audio_resume(mixer, 0, 1, e));
        sink.consumed += 480;
        if (sink.consumed > sink.submitted)
          sink.consumed = sink.submitted;
        CHECK(bk_audio_poll(mixer, e));
        BkAudioCursor cursor;
        int playing = 0;
        CHECK(bk_audio_playing(mixer, 0, &playing) && bk_audio_cursor(mixer, 0, &cursor));
        uint32_t bytes = cursor.pcm ? (uint32_t)(bk_pcm_frames(cursor.pcm) * bk_pcm_channels(cursor.pcm) * 2) : 4096;
        uint32_t offset = cursor.pcm ? (uint32_t)(cursor.source_frame * bk_pcm_channels(cursor.pcm) * 2) : 0;
        uint8_t window[440] = {0};
        int sample = cursor.pcm && cursor.buffered && bytes >= 443 && offset < bytes - 443;
        if (sample) {
          const int16_t *pcm = bk_pcm_samples(cursor.pcm) + offset / 2;
          for (unsigned i = 0; i < 220; ++i) {
            window[i * 2] = (uint8_t)pcm[i];
            window[i * 2 + 1] = (uint8_t)((uint16_t)pcm[i] >> 8);
          }
        }
        BkEndingVoiceEnvelope before = retained.voice;
        int changed = bk_ending_normal_assets_face_state(assets)->expression != aux.expression_b;
        CHECK(bk_ending_presentation_scene_step(&scene, &b, &input, 1.f / 60, e));
        CHECK(clock.calls == (unsigned)(changed ? 4 : 3));
        for (unsigned i = 0; i < 2; ++i) {
          CHECK(bk_actor_pose_state(bk_ending_normal_assets_pose(assets, 2 + i), &after));
          CHECK(!memcmp(tracks + i, &after, sizeof(after)));
        }
        uint32_t words[14] = {group, variant, step, (uint32_t)playing,
            (uint32_t)cursor.playing, (uint32_t)!sample, bytes, offset,
            float_bits(before.target), float_bits(before.smoothed),
            float_bits(retained.voice.target), float_bits(retained.voice.smoothed),
            random, clock.calls};
        CHECK(write_record(records, words, window));
        voice_frames += retained.voice.smoothed > 0;
        for (unsigned i = 0; i < 3; ++i)
          CHECK(isfinite(retained.manual[i].offset[0]) && isfinite(retained.manual[i].offset[1]));
        manual_frames += fabsf(retained.manual[0].offset[0]) + fabsf(retained.manual[1].offset[0]) + fabsf(retained.manual[2].offset[0]) > 0;
        for (unsigned i = 0; i < 5; ++i) {
          size_t count = 0;
          const float *world = bk_actor_pose_world(bk_ending_normal_assets_pose(assets, i), &count);
          CHECK(world);
          geometry = hash(geometry, world, count * sizeof(*world));
        }
        CHECK(bk_audio_fill(mixer, e));
        frames++;
      }
      /* Fail before any animation/face effect; verify the last valid clocks
       * and independent retained state are not consumed. */
      BkEndingPresentationRetained saved = retained;
      BkClipState actor_before;
      CHECK(bk_actor_pose_state(bk_ending_normal_assets_pose(assets, 0), &actor_before));
      clock.calls = 0;
      clock.failure = 0;
      CHECK(!bk_ending_presentation_scene_step(&scene, &b, &input, 1.f / 60, e));
      CHECK(!memcmp(&saved, &retained, sizeof(saved)));
      CHECK(bk_actor_pose_state(bk_ending_normal_assets_pose(assets, 0), &after));
      CHECK(!memcmp(&after, &actor_before, sizeof(after)));
      rejections++;
      pcm_hash = hash(pcm_hash, &sink.hash, sizeof(sink.hash));
      bk_ending_audio_destroy(audio); audio = NULL;
      bk_audio_destroy(mixer); mixer = NULL;
      bk_material_pose_destroy(materials); materials = NULL;
      bk_ending_normal_assets_destroy(assets); assets = NULL;
    }
  CHECK(frames == 1800 && voice_frames && manual_frames);
  printf("PASS ending presentation profiles=10 frames=%u voice=%u manual=%u rejected=%u geometry=%016llx pcm=%016llx\n",
      frames, voice_frames, manual_frames, rejections,
      (unsigned long long)geometry, (unsigned long long)pcm_hash);
  rc = 0;
done:
  if (records && fclose(records))
    rc = 1;
  bk_ending_audio_destroy(audio);
  bk_audio_destroy(mixer);
  bk_material_pose_destroy(materials);
  bk_ending_normal_assets_destroy(assets);
  bk_resources_destroy(store);
  return rc;
}
