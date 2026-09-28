#include "game/ending_camera_config.h"
#include "scene/ending_camera_assets.h"
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
static const float I[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
int main(int argc, char **argv) {
  if (argc != 2)
    return 2;
  char e[256] = {0}, path[1024];
  int rc = 1;
  BkResourceStore *store = bk_resources_create(e);
  BkEndingCameraAssets *a = NULL;
  BkActorForest *forest = NULL;
  BkActorPose *target = NULL;
  BkClipSet *fixture_clips = NULL;
  unsigned frames = 0, held = 0, updated = 0;
  uint64_t hash = UINT64_C(14695981039346656037);
  CHECK(store);
  snprintf(path, sizeof(path), "%s/bk3_04.pp", argv[1]);
  CHECK(bk_resources_mount(store, "bk3_04", path, e));
  /* Explicit stationary target fixture, not a fabricated ending actor. */
  uint8_t xan[0x5190] = {0};
  strcpy((char *)xan, "target.x");
  strcpy((char *)xan + 256, "target.x");
  fixture_clips = bk_clip_set_decode(xan, sizeof(xan), e);
  CHECK(fixture_clips);
  BkModelFrame target_frame = {
      .id = 1, .parent_index = BK_MODEL_NONE, .mesh_index = BK_MODEL_NONE};
  memcpy(target_frame.local, I, sizeof(I));
  BkModel target_model = {.frames = &target_frame, .frame_count = 1};
  target = bk_actor_pose_create_loaded(&target_model, fixture_clips, 0,
                                       (float[3]){100, 18, 40}, 0, e);
  CHECK(target);
  for (unsigned g = 0; g < 5; ++g)
    for (unsigned v = 0; v < 10; ++v) {
      BkMenuCamera s = {.focus = {7, 8, 9}, .fov = .4f};
      memcpy(s.matrix, I, sizeof(I));
      memcpy(s.pose.world, I, sizeof(I));
      s.matrix[12] = 21;
      s.pose.world[12] = 9;
      a = bk_ending_camera_assets_create(store, g, v,
                                         27 + (int)g * 61 + (int)v * 9, e);
      CHECK(a);
      BkActorPose *poses[3] = {target, bk_ending_camera_assets_pose(a, 0),
                               bk_ending_camera_assets_pose(a, 1)};
      forest = bk_actor_forest_create(poses, 3, e);
      CHECK(forest);
      uint32_t t = bk_actor_forest_node(forest, 0, 0);
      CHECK(bk_actor_forest_attach(forest, 0, t, e));
      CHECK(bk_actor_forest_anchor(forest, 1, s.pose.world, 0, e));
      BkEndingCameraPresets presets, before_presets;
      memset(&presets, 0x55, sizeof(presets));
      before_presets = presets;
      BkMenuCamera saved = s;
      assert(!bk_ending_camera_assets_attach(a, forest, (uint32_t[2]){2, 1}, &s,
                                             &presets, e));
      assert(!memcmp(&s, &saved, sizeof(s)) &&
             !memcmp(&presets, &before_presets, sizeof(presets)));
      const uint32_t indices[2] = {1, 2};
      CHECK(
          bk_ending_camera_assets_attach(a, forest, indices, &s, &presets, e));
      assert(s.pose.world[12] == 9 && s.matrix[12] == 21 && s.focus[0] == 7 &&
             s.pose.position[1] == 20 && s.fov == 1);
      BkEndingCameraPresets expected;
      assert(bk_ending_camera_config(&expected, g, v) &&
             !memcmp(&expected, &presets, sizeof(presets)));
      saved = s;
      assert(
          !bk_ending_camera_assets_attach(a, forest, indices, &s, &presets, e));
      assert(!memcmp(&saved, &s, sizeof(s)));
      uint32_t root = bk_ending_camera_assets_root(a, 0);
      uint32_t node = bk_ending_camera_assets_node(a, 0);
      BkClipState secondary;
      assert(bk_actor_pose_state(poses[2], &secondary));
      BkEndingCameraTransitions clocks = {.zoom_progress = .75f, .zoom_fov = 1};
      BkEndingCameraPresetGate gate = {.state_ee0 = 4};
      for (unsigned step = 0; step < 180; ++step) {
        BkEndingCameraKind kind = (BkEndingCameraKind)(step % 4);
        BkActorVisibilityEdit edit = {root, step % 11 == 3};
        CHECK(bk_actor_pose_visibility(poses[1], &edit, 1, e));
        float cached[16];
        memcpy(cached, bk_actor_pose_frame(poses[1], node), 64);
        BkClipState before, after;
        assert(bk_actor_pose_state(poses[1], &before));
        float dt = step % 9 ? .016f : 0;
        if (step < 120) {
          CHECK(bk_ending_camera_assets_step(
              a, forest, indices, &s, kind, (float[3]){1, 2, 3},
              (float[2]){2, -1}, step % 3, t, dt, e));
        } else {
          int complete = -1;
          CHECK(bk_ending_camera_assets_preset(
              a, forest, indices, &s, &clocks, &presets,
              step % 2 ? BK_ENDING_PRESET_ZOOM : BK_ENDING_PRESET, step % 3,
              (float[3]){1, 2, 3}, &gate, 0x10, dt, &complete, e));
          assert(complete == 0 || complete == 1);
          BkMenuCamera previous = s;
          BkEndingCameraTransitions previous_clocks = clocks;
          complete = 7;
          assert(!bk_ending_camera_assets_preset(
              a, forest, indices, &s, &clocks, &presets, BK_ENDING_PRESET, 3,
              (float[3]){1, 2, 3}, &gate, 0x10, dt, &complete, e));
          assert(complete == 7 && !memcmp(&s, &previous, sizeof(s)) &&
                 !memcmp(&clocks, &previous_clocks, sizeof(clocks)));
        }
        assert(!memcmp(cached, bk_actor_pose_frame(poses[1], node), 64));
        assert(bk_actor_pose_state(poses[1], &after));
        if (step >= 120 || kind != BK_ENDING_CAMERA_AUTO || edit.hidden) {
          assert(!memcmp(&before, &after, sizeof(before)));
          ++held;
        } else if (memcmp(&before, &after, sizeof(before)))
          ++updated;
        assert(bk_actor_pose_state(poses[2], &after));
        assert(!memcmp(&secondary, &after, sizeof(after)));
        for (unsigned k = 0; k < 16; ++k) {
          float anchor = bk_actor_forest_world(forest, 1)[k];
          assert(anchor == s.pose.world[k]);
        }
        saved = s;
        assert(!bk_ending_camera_assets_step(a, forest, indices, &s,
                                             BK_ENDING_CAMERA_AUTO, NULL, NULL,
                                             0, BK_FRAME_NONE, dt, e));
        assert(!memcmp(&saved, &s, sizeof(s)));
        const BkFrameVisit *visits;
        uint32_t count;
        CHECK(bk_actor_forest_draw(forest, step % 7 == 3 ? 1 : 0, &visits,
                                   &count, e));
        const unsigned char *bytes = (const unsigned char *)&s;
        for (size_t j = 0; j < sizeof(s); ++j) {
          hash ^= bytes[j];
          hash *= UINT64_C(1099511628211);
        }
        ++frames;
      }
      bk_actor_forest_destroy(forest);
      forest = NULL;
      bk_ending_camera_assets_destroy(a);
      a = NULL;
    }
  assert(updated > 1000 && held > 4000);
  assert(!bk_ending_camera_assets_create(store, 5, 0, 0, e));
  assert(!bk_ending_camera_assets_create(store, 0, 10, 0, e));
  printf("PASS ending cameras: 50 profiles, %u frames, %u held, %u updated, "
         "FNV%016llx\n",
         frames, held, updated, (unsigned long long)hash);
  rc = 0;
done:
  if (rc)
    fprintf(stderr, "%s\n", e);
  bk_actor_forest_destroy(forest);
  bk_ending_camera_assets_destroy(a);
  bk_actor_pose_destroy(target);
  bk_clip_set_destroy(fixture_clips);
  bk_resources_destroy(store);
  return rc;
}
