#include "scene/menu_camera_assets.h"
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
  BkMenuCameraAssets *assets = NULL;
  BkActorForest *forest = NULL;
  unsigned frames = 0, held = 0, fresh = 0;
  int rc = 1;
  CHECK(store);
  snprintf(path, sizeof(path), "%s/bk3_04.pp", argv[1]);
  CHECK(bk_resources_mount(store, "bk3_04", path, error));
  for (unsigned group = 0; group < 5; ++group) {
    BkMenuCamera state = {0};
    for (unsigned i = 0; i < 16; ++i)
      state.pose.world[i] = state.matrix[i] = i % 5 == 0;
    state.pose.world[12] = 9;
    state.matrix[12] = 21;
    state.focus[0] = 7;
    assets = bk_menu_camera_assets_create(store, group, .016f, &state, error);
    CHECK(assets);
    assert(state.pose.world[12] == 9 && state.matrix[12] == 21 &&
           state.focus[0] == 7);
    assert(state.radius == 22 && state.height == 18 &&
           state.pose.position[1] == 20 && state.fov == 1);
    BkActorPose *poses[2] = {bk_menu_camera_assets_pose(assets, 0),
                             bk_menu_camera_assets_pose(assets, 1)};
    forest = bk_actor_forest_create(poses, 2, error);
    CHECK(forest);
    for (unsigned i = 0; i < 2; ++i)
      CHECK(bk_actor_forest_attach(
          forest, 0,
          bk_actor_forest_node(forest, i,
                               bk_menu_camera_assets_root(assets, i)),
          error));
    uint32_t node = bk_menu_camera_assets_node(assets, 0);
    BkClipState secondary;
    assert(bk_actor_pose_state(poses[1], &secondary));
    for (unsigned step = 0; step < 360; ++step) {
      unsigned mode = (step / 30) % 2;
      float dt = step % 41 ? .016f : 0;
      BkActorVisibilityEdit edit = {bk_menu_camera_assets_root(assets, 0),
                                    step % 17 == 3};
      CHECK(bk_actor_pose_visibility(poses[0], &edit, 1, error));
      float old[16];
      memcpy(old, bk_actor_pose_frame(poses[0], node), 64);
      BkClipState before;
      assert(bk_actor_pose_state(poses[0], &before));
      CHECK(bk_menu_camera_assets_step(assets, forest, (uint32_t[2]){0, 1},
                                       &state, mode, (float[2]){1.25f, -.75f},
                                       step % 4, BK_FRAME_NONE, dt, error));
      BkClipState after;
      assert(bk_actor_pose_state(poses[0], &after));
      if (!mode) {
        assert(!memcmp(&before, &after, sizeof(before)));
        assert(!memcmp(old, bk_actor_pose_frame(poses[0], node), 64));
        ++held;
      } else if (memcmp(old, bk_actor_pose_frame(poses[0], node), 64))
        ++fresh;
      assert(bk_actor_pose_state(poses[1], &after));
      assert(!memcmp(&secondary, &after, sizeof(after)));
      BkMenuCamera saved = state;
      assert(!bk_menu_camera_assets_step(assets, forest, (uint32_t[2]){1, 0},
                                         &state, mode, (float[2]){0}, 0,
                                         BK_FRAME_NONE, dt, error));
      assert(!memcmp(&saved, &state, sizeof(state)));
      const BkFrameVisit *visits;
      uint32_t count;
      CHECK(bk_actor_forest_draw(forest, 0, &visits, &count, error));
      for (unsigned i = 0; i < 16; ++i)
        assert(isfinite(bk_actor_forest_view(forest)[i]));
      ++frames;
    }
    bk_actor_forest_destroy(forest);
    forest = NULL;
    bk_menu_camera_assets_destroy(assets);
    assets = NULL;
    BkMenuCamera saved = state;
    assert(!bk_menu_camera_assets_create(store, 5, .016f, &state, error));
    assert(!memcmp(&saved, &state, sizeof(state)));
  }
  assert(fresh > 700 && held == 900);
  printf("menu camera assets PASS: 5 profiles, %u frames, %u held, %u fresh\n",
         frames, held, fresh);
  rc = 0;
done:
  if (rc)
    fprintf(stderr, "%s\n", error);
  bk_actor_forest_destroy(forest);
  bk_menu_camera_assets_destroy(assets);
  bk_resources_destroy(store);
  return rc;
}
