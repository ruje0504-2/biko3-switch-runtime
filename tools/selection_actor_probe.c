#include "scene/lighting_assets.h"
#include "scene/selection_actor_assets.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#define CHECK(v)                                                               \
  do {                                                                         \
    if (!(v))                                                                  \
      goto done;                                                               \
  } while (0)
int main(int argc, char **argv) {
  if (argc != 2)
    return 2;
  char error[256] = {0}, path[1024];
  BkResourceStore *store = bk_resources_create(error);
  BkSelectionActorAssets *a = NULL;
  BkSceneLighting *lights = NULL;
  unsigned frames = 0, held = 0;
  uint64_t matrices = 0;
  int rc = 1;
  CHECK(store);
  const char *packs[] = {"bk3_01", "bk3_06"};
  for (unsigned i = 0; i < 2; ++i) {
    snprintf(path, sizeof(path), "%s/%s.pp", argv[1], packs[i]);
    CHECK(bk_resources_mount(store, packs[i], path, error));
  }
  CHECK(bk_resources_mount_directory(store, "faces", argv[1], 20480, error));
  for (unsigned group = 0; group < 5; ++group)
    for (unsigned alt = 0; alt < 2; ++alt) {
      uint32_t rng = 98765, clocks[4] = {1000, 1001, 1002, 1003};
      a = bk_selection_actor_assets_create(store, group, (uint8_t)alt, clocks,
                                           &rng, error);
      CHECK(a);
      BkActorPose *pose = bk_selection_actor_assets_pose(a);
      const BkModel *model = bk_actor_pose_model(pose);
      const float *world = bk_actor_pose_frame(pose, 0);
      lights = bk_scene_lighting_create(model, world, model->frame_count * 16,
                                        error);
      CHECK(lights);
      const BkModelEnvironment *environment =
          bk_scene_lighting_environment(lights);
      for (unsigned i = 0; i < environment->light_count; ++i)
        CHECK(bk_scene_lighting_command(
            lights, &(BkLightingCommand){BK_PASS_LIGHT_ENABLE, i, 1}));
      uint32_t focus = bk_selection_actor_assets_focus(a);
      assert(focus < model->frame_count);
      assert(bk_selection_actor_assets_needs_movie(a) == (int)alt);
      assert(bk_audio_clip_rate(bk_selection_actor_assets_voice(a)) > 0);
      for (unsigned frame = 0; frame < 180; ++frame) {
        BkClipState before, after;
        assert(bk_actor_pose_state(pose, &before));
        BkFaceState fs = *bk_selection_actor_assets_face_state(a);
        uint32_t saved_rng = rng;
        float camera[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 40, 35, 60, 1};
        uint32_t times[3] = {1100 + frame * 137, 1101 + frame * 137,
                             1102 + frame * 137};
        assert(!bk_selection_actor_assets_step(a, group, NAN, 0, camera,
                                               times[0], times, &rng, error));
        assert(bk_actor_pose_state(pose, &after));
        assert(
            !memcmp(&before, &after, sizeof(before)) && rng == saved_rng &&
            !memcmp(&fs, bk_selection_actor_assets_face_state(a), sizeof(fs)));
        float cached[16];
        memcpy(cached, bk_actor_pose_frame(pose, focus), 64);
        CHECK(bk_selection_actor_assets_step(a, group, frame % 7 ? .05f : 0,
                                             frame % 10, camera, times[0],
                                             times, &rng, error));
        assert(!memcmp(cached, bk_actor_pose_frame(pose, focus), 64));
        ++held;
        if (frame % 4 != 2)
          bk_actor_pose_publish(pose);
        for (uint32_t i = 0; i < model->frame_count; ++i) {
          const float *world = bk_actor_pose_frame(pose, i),
                      *local = bk_actor_pose_local(pose, i);
          for (unsigned j = 0; j < 16; ++j)
            assert(isfinite(world[j]) && isfinite(local[j]));
          matrices += 2;
        }
        BkLighting snapshot;
        CHECK(bk_scene_lighting_values(lights, bk_actor_pose_frame(pose, 0),
                                       model->frame_count * 16, &snapshot,
                                       error));
        assert(snapshot.spot_count == (alt ? 0u : 2u));
        ++frames;
      }
      bk_scene_lighting_destroy(lights);
      lights = NULL;
      bk_selection_actor_assets_destroy(a);
      a = NULL;
    }
  {
    uint32_t rng = 7;
    assert(!bk_selection_actor_assets_create(store, 5, 0, (uint32_t[4]){0},
                                             &rng, error));
    assert(rng == 7);
    assert(!bk_selection_actor_assets_create(store, 0, 2, (uint32_t[4]){0},
                                             &rng, error));
    assert(rng == 7);
  }
  printf("PASS selection actor:10 real bodies; %u frames, %u held world "
         "caches, %llu finite matrices; real point/spot lights; invalid input "
         "atomic\n",
         frames, held, (unsigned long long)matrices);
  rc = 0;
done:
  if (rc)
    fprintf(stderr, "%s\n", error);
  bk_scene_lighting_destroy(lights);
  bk_selection_actor_assets_destroy(a);
  bk_resources_destroy(store);
  return rc;
}
