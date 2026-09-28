/* Actual assets after the recovered scalar prefix. Does not stand in for
 * remaining stage loaders or a complete entry/game frame. */
#include "scene/ending_normal_assets.h"
#include "scene/ending_state.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#define CHECK(x)                                                               \
  do {                                                                         \
    if (!(x)) {                                                                \
      fprintf(stderr, "line%d: %s\n", __LINE__, e);                            \
      goto done;                                                               \
    }                                                                          \
  } while (0)
static const float identity[16] = {1, 0, 0, 0, 0, 1, 0, 0,
                                   0, 0, 1, 0, 0, 0, 0, 1};
static uint64_t hash(uint64_t h, const void *p, size_t n) {
  const uint8_t *b = p;
  while (n--)
    h = (h ^ *b++) * UINT64_C(1099511628211);
  return h;
}
typedef struct {
  BkEndingNormalAssets **owner;
  unsigned calls;
} Input;
static int warp(void *p, float x, float y, char e[256]) {
  Input *in = p;
  if (*in->owner || x != 320 || y != 240) {
    snprintf(e, 256, "prior resources still active or bad warp");
    return 0;
  }
  in->calls++;
  return 1;
}
int main(int argc, char **argv) {
  if (argc != 2)
    return 2;
  char e[256] = {0}, path[1024];
  int result = 1;
  BkResourceStore *store = NULL, *empty = NULL;
  BkEndingNormalAssets *owner = NULL;
  BkClipSet *clips = NULL;
  BkBlob raw = {0};
  BkEndingState state = {0};
  BkFadeSprite overlay = {1, 2, 3};
  BkMenuCamera camera = {0};
  BkEndingCameraPresets presets = {0};
  memcpy(camera.pose.world, identity, sizeof(camera.pose.world));
  memcpy(camera.matrix, identity, sizeof(camera.matrix));
  Input input = {&owner, 0};
  BkEndingStateOps ops = {&input, warp};
  uint64_t digest = UINT64_C(14695981039346656037);
  unsigned loads = 0, names = 0, failures = 0;
  store = bk_resources_create(e);
  empty = bk_resources_create(e);
  CHECK(store && empty);
  const char *packs[] = {"bk3_08", "bk3_04", "bk3_03", "fambom", "bk3_09",
                         "bk3_10", "bk3_11", "bk3_12", "bk3_13"};
  /* Native4cf318/4d00fa/4d1d22/4d2320/4d39e6 packaged branches. */
  const char *primary_packs[10] = {"bk3_08", "bk3_09", "bk3_10", "bk3_10",
                                   "bk3_10", "bk3_11", "bk3_12", "bk3_13",
                                   "bk3_13", "bk3_13"};
  for (unsigned i = 0; i < sizeof(packs) / sizeof(*packs); i++) {
    snprintf(path, sizeof(path), "%s/%s.pp", argv[1], packs[i]);
    CHECK(bk_resources_mount(store, packs[i], path, e));
  }
  for (unsigned repetition = 0; repetition < 3; repetition++)
    for (unsigned group = 0; group < 5; group++)
      for (unsigned variant = 0; variant < 2; variant++) {
        /* Retained fields survive actual create/destroy cycles. */
        state.frame.previous_clock = 100 + repetition;
        state.ui_controller.normal.cycles = 31 + group;
        state.auxiliary.base = 41;
        state.auxiliary.direction = 2;
        CHECK(bk_ending_state_begin(&state, &overlay, group, variant, .5f,
                                    (int32_t[2]){0}, &ops, e));
        CHECK(state.frame.previous_clock == 100 + repetition &&
              state.ui_controller.normal.cycles == 31 + (int)group &&
              state.auxiliary.base == 41 && state.auxiliary.direction == 2);
        if (!repetition && !variant)
          for (unsigned i = 0; i < 10; i++) {
            CHECK(state.model_paths[i][0] == '\\');
            CHECK(bk_resources_read(store, primary_packs[i],
                                    state.model_paths[i] + 1, &raw,
                                    e) == BK_RESOURCE_OK);
            clips = bk_clip_set_decode(raw.data, raw.size, e);
            bk_blob_free(&raw);
            CHECK(clips);
            digest = hash(digest, state.model_paths[i], 260);
            digest = hash(digest, bk_clip_model_name(clips),
                          strlen(bk_clip_model_name(clips)));
            bk_clip_set_destroy(clips);
            clips = NULL;
            names++;
          }
        uint32_t random = 123 + group + repetition,
                 clocks[] = {100, 110, 120, 130};
        BkMenuCamera saved = camera;
        BkEndingCameraPresets saved_presets = presets;
        uint32_t saved_random = random;
        owner = bk_ending_normal_assets_create(empty, state.frame.group,
                                               state.auxiliary.variant, clocks,
                                               &random, &camera, &presets, e);
        CHECK(!owner && random == saved_random &&
              !memcmp(&saved, &camera, sizeof(camera)) &&
              !memcmp(&saved_presets, &presets, sizeof(presets)));
        failures++;
        *e = 0;
        owner = bk_ending_normal_assets_create(store, state.frame.group,
                                               state.auxiliary.variant, clocks,
                                               &random, &camera, &presets, e);
        CHECK(owner);
        const BkEndingNormalConfig *config =
            bk_ending_normal_assets_config(owner);
        CHECK(!strcmp(config->primary, state.model_paths[0] + 1));
        CHECK(bk_ending_normal_assets_load_background(owner, store, e));
        digest = hash(digest, config, sizeof(*config));
        for (unsigned actor = 0; actor < 5; actor++) {
          BkActorPose *pose = bk_ending_normal_assets_pose(owner, actor);
          CHECK(pose);
          size_t count;
          const float *world = bk_actor_pose_world(pose, &count);
          digest = hash(digest, world, count * sizeof(float));
        }
        loads++;
        /* A premature second entry is rejected by the real enclosing owner
         * check. */
        BkEndingState saved_state = state;
        CHECK(!bk_ending_state_begin(&state, &overlay, group, 0, .5f,
                                     (int32_t[2]){0}, &ops, e));
        CHECK(!memcmp(&saved_state, &state, sizeof(state)));
        failures++;
        *e = 0;
        bk_ending_normal_assets_destroy(owner);
        owner = NULL;
      }
  CHECK(input.calls == 30 && names == 50 && loads == 30);
  printf("ending state assets PASS loads=%u paths=%u rejections=%u "
         "hash=%016" PRIx64 "\n",
         loads, names, failures, digest);
  result = 0;
done:
  bk_blob_free(&raw);
  bk_clip_set_destroy(clips);
  bk_ending_normal_assets_destroy(owner);
  bk_resources_destroy(empty);
  bk_resources_destroy(store);
  return result;
}
