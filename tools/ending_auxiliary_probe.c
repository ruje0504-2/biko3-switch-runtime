#include "scene/ending_auxiliary.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define CHECK(x)                                                               \
  do {                                                                         \
    if (!(x))                                                                  \
      goto done;                                                               \
  } while (0)
typedef struct {
  uint64_t submitted, consumed, samples, hash;
} Sink;
static uint64_t digest(uint64_t h, const void *data, size_t n) {
  const unsigned char *p = data;
  for (size_t i = 0; i < n; i++)
    h = (h ^ p[i]) * UINT64_C(1099511628211);
  return h;
}
static int submit(void *p, const int16_t *pcm, size_t frames, char e[256]) {
  (void)e;
  Sink *s = p;
  s->hash = digest(s->hash, pcm, frames * 2 * sizeof(*pcm));
  s->submitted += frames;
  s->samples += frames * 2;
  return 1;
}
static int poll(void *p, uint64_t *out, char e[256]) {
  (void)e;
  *out = ((Sink *)p)->consumed;
  return 1;
}
static uint64_t matrices(BkActorPose *a, uint32_t count) {
  uint64_t h = UINT64_C(14695981039346656037);
  for (uint32_t i = 0; i < count; i++) {
    h = digest(h, bk_actor_pose_local(a, i), 64);
    h = digest(h, bk_actor_pose_frame(a, i), 64);
    h = digest(h, bk_actor_pose_parent_world(a, i), 64);
  }
  return h;
}
int main(int argc, char **argv) {
  if (argc != 2)
    return 2;
  char e[256] = {0}, path[1024], name[32] = {0};
  int rc = 1;
  BkResourceStore *store = bk_resources_create(e);
  BkBlob raw = {0};
  BkClipSet *clips = NULL;
  BkModel *model = NULL;
  BkActorPose *actor = NULL, *sibling = NULL;
  BkEyeAssets *eyes = NULL;
  BkAudio *mixer = NULL;
  BkEndingAudio *audio = NULL;
  Sink sink = {.hash = UINT64_C(14695981039346656037)};
  uint64_t hash = UINT64_C(14695981039346656037);
  unsigned frames = 0, accepted = 0, rejected = 0, textures = 0;
  CHECK(store);
  for (unsigned i = 0; i < 4; i++) {
    const char *pack =
        (const char *[]){"bk3_02", "bk3_06", "bk3_08", "fambom"}[i];
    snprintf(path, sizeof(path), "%s/%s.pp", argv[1], pack);
    CHECK(bk_resources_mount(store, pack, path, e));
  }
  BkAudioSink output = {&sink, 48000, 480, 1920, submit, poll};
  mixer = bk_audio_create(&output, e);
  CHECK(mixer);
  audio = bk_ending_audio_create(store, mixer, 50, e);
  CHECK(audio);
  for (unsigned group = 0; group < 5; group++) {
    snprintf(name, sizeof(name), "h%02u_00.fam", group + 1);
    CHECK(bk_resources_read(store, "fambom", name, &raw, e) == BK_RESOURCE_OK);
    BkFaceConfig config;
    CHECK(bk_face_config_decode(raw.data, raw.size, &config, e));
    bk_blob_free(&raw);
    CHECK(bk_resources_read(store, "bk3_08", config.actor_clip, &raw, e) ==
          BK_RESOURCE_OK);
    clips = bk_clip_set_decode(raw.data, raw.size, e);
    bk_blob_free(&raw);
    CHECK(clips);
    CHECK(bk_resources_read(store, "bk3_08", bk_clip_model_name(clips), &raw,
                            e) == BK_RESOURCE_OK);
    CHECK(bk_model_decode(raw.data, raw.size, &model, e) == BK_MODEL_OK);
    bk_blob_free(&raw);
    uint32_t root = BK_MODEL_NONE;
    for (uint32_t i = 0; i < model->frame_count; i++)
      if (model->frames[i].parent_index == BK_MODEL_NONE) {
        assert(root == BK_MODEL_NONE);
        root = i;
      }
    CHECK(root != BK_MODEL_NONE);
    actor = bk_actor_pose_create(model, clips, root, NULL, (float[3]){0}, 0, 4,
                                 0, e);
    CHECK(actor);
    sibling = bk_actor_pose_create(model, clips, root, NULL, (float[3]){0}, 0,
                                   4, 0, e);
    CHECK(sibling);
    eyes = bk_eye_assets_create(store, "bk3_08", model,
                                bk_clip_model_name(clips), &config, e);
    CHECK(eyes);
    BkClipState sibling_clock;
    assert(bk_actor_pose_state(sibling, &sibling_clock));
    uint64_t sibling_matrices = matrices(sibling, model->frame_count);
    BkClipTiming sibling_times[128];
    for (unsigned slot = 0; slot < 128; slot++)
      assert(bk_actor_pose_timing(sibling, slot, &sibling_times[slot]));
    BkEndingAuxiliaryServices services = {actor, eyes, audio};
    BkEndingAuxiliaryState state = {.gate = 1, .base = 10};
    BkEndingFrameState frame = {
        .group = group, .phase = 6, .auxiliary_mode = 77};
    /* Explicit sound fixture for channels initialized by the future loader;
     * this is not a claim about original flow16 entry resource identities. */
    for (unsigned slot = 2; slot < 6; slot++)
      CHECK(bk_ending_audio_bind(audio, slot, "bk3_02", "se004.wav", e));
    for (unsigned step = 0; step < 720; step++) {
      uint64_t advance = (unsigned[]){0, 240, 480, 960, 1920, 3000}[step % 6];
      uint64_t queued = sink.submitted - sink.consumed;
      sink.consumed += advance < queued ? advance : queued;
      CHECK(bk_audio_poll(mixer, e));
      /* Each profile starts from an explicit action4 fixture. The full
       * ending stage owner that exits native gates1..3 is still separate. */
      if (step % 120 == 0)
        CHECK(bk_actor_pose_request_mode(actor, 4, BK_CLIP_REQUEST_CONFIGURED,
                                         e));
      if (step % 120 < 5 || step % 15 == 0) {
        state.variant = step / 360;
        state.selection = (step / 120) % 3;
        state.progress = (float)(step % 11) / 10;
        if (step % 75 == 0)
          for (unsigned slot = 2; slot < 5; slot++) {
            BkEndingAudioCall c = {
                BK_ENDING_AUDIO_RESTART, slot, 0, 0, 1, -900};
            int playing;
            CHECK(bk_ending_audio_call(audio, group, state.variant,
                                       state.selection, &c, &playing, e));
          }
        int32_t result;
        uint64_t before = matrices(actor, model->frame_count);
        CHECK(bk_ending_auxiliary_apply(
            &services, &state, &frame,
            (int[]){0, 1, 3, 2,
                    3}[step % 120 < 5 ? step % 120 : (step / 15) % 5],
            -700, -900, &result, e));
        assert(before == matrices(actor, model->frame_count) &&
               frame.auxiliary_mode == 77);
        accepted += result == 1;
        rejected += result == 0;
        if (result) {
          if (bk_eye_assets_image(eyes, 1, NULL)) {
            assert(bk_eye_assets_selected(eyes) == 1);
            textures++;
          }
          assert(state.expression_a >= 1 && state.expression_b >= 3);
        }
      }
      CHECK(bk_actor_pose_advance(
          actor, -1,
          step % 120 < 5
              ? .016f
              : (float[]){.001f, .016f, .1f, .5f, 2, 1.f / 30}[step % 6],
          e));
      bk_actor_pose_publish(actor);
      BkClipState clock;
      assert(bk_actor_pose_state(sibling, &clock) &&
             !memcmp(&clock, &sibling_clock, sizeof(clock)));
      assert(matrices(sibling, model->frame_count) == sibling_matrices);
      for (unsigned slot = 0; slot < 128; slot++) {
        const BkClipDefinition *definition = bk_clip_definition(clips, slot);
        int32_t chain, next;
        BkClipTiming timing;
        assert(bk_actor_pose_clip_link(sibling, slot, &chain, &next));
        assert(bk_actor_pose_timing(sibling, slot, &timing));
        assert(chain == definition->chain && next == definition->next &&
               !memcmp(&timing, &sibling_times[slot], sizeof(timing)));
      }
      uint64_t mh = matrices(actor, model->frame_count);
      hash = digest(hash, &mh, sizeof(mh));
      assert(bk_actor_pose_state(actor, &clock));
      hash = digest(hash, &clock, sizeof(clock));
      CHECK(bk_audio_fill(mixer, e));
      frames++;
    }
    CHECK(bk_ending_audio_stop(audio, e));
    bk_eye_assets_destroy(eyes);
    eyes = NULL;
    bk_actor_pose_destroy(actor);
    actor = NULL;
    bk_actor_pose_destroy(sibling);
    sibling = NULL;
    bk_model_destroy(model);
    model = NULL;
    bk_clip_set_destroy(clips);
    clips = NULL;
  }
  assert(frames == 3600 && accepted > 100 && textures > 100);
  printf("PASS ending auxiliary assets: 5 FAM primary models, 30 audio "
         "profiles, %u frames, %u accepted/%u gated, %u eye selections, "
         "matrices FNV%016llx, %llu PCM samples FNV%016llx; sibling and "
         "unpublished caches retained\n",
         frames, accepted, rejected, textures, (unsigned long long)hash,
         (unsigned long long)sink.samples, (unsigned long long)sink.hash);
  rc = 0;
done:
  if (rc)
    fprintf(stderr, "%s step%u: %s\n", name, frames, e);
  if (audio) {
    char ignored[256];
    bk_ending_audio_stop(audio, ignored);
  }
  bk_ending_audio_destroy(audio);
  bk_audio_destroy(mixer);
  bk_eye_assets_destroy(eyes);
  bk_actor_pose_destroy(actor);
  bk_actor_pose_destroy(sibling);
  bk_model_destroy(model);
  bk_clip_set_destroy(clips);
  bk_blob_free(&raw);
  bk_resources_destroy(store);
  return rc;
}
