#include "game/ending_normal.h"
#include "scene/ending_auxiliary_assets.h"

#include <stdio.h>
#include <string.h>

#define CHECK(expr) do {                                                        \
  if (!(expr)) {                                                               \
    fprintf(stderr, "4D39E6 probe line %d: %s: %s\n", __LINE__, #expr, error); \
    goto done;                                                                 \
  }                                                                            \
} while (0)

static const float identity[16] = {
    1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};

static BkMenuCamera initial_camera(void) {
  BkMenuCamera camera = {0};
  memcpy(camera.pose.world, identity, sizeof(identity));
  memcpy(camera.matrix, identity, sizeof(identity));
  camera.fov = .4f;
  return camera;
}

int main(int argc, char **argv) {
  if (argc != 2)
    return 2;
  char error[256] = {0}, path[2048];
  int ok = 0;
  unsigned entries = 0, advances = 0;
  BkResourceStore *store = NULL;
  BkEndingBackgroundAssets *background = NULL;
  BkEndingAuxiliaryAssets *owner = NULL;
  const char *packs[] = {"bk3_03", "bk3_04", "bk3_12", "fambom"};

  store = bk_resources_create(error);
  CHECK(store);
  for (unsigned i = 0; i < sizeof(packs) / sizeof(*packs); ++i) {
    int n = snprintf(path, sizeof(path), "%s/%s.pp", argv[1], packs[i]);
    CHECK(n > 0 && (size_t)n < sizeof(path));
    CHECK(bk_resources_mount(store, packs[i], path, error));
  }
  const uint32_t clocks[4] = {100, 110, 120, 130};
  for (unsigned group = 0; group < 5; ++group)
    for (unsigned variant = 0; variant < 2; ++variant) {
      BkMenuCamera camera = initial_camera();
      BkEndingCameraPresets presets = {0};
      uint32_t random = 123 + group * 11 + variant;
      background = bk_ending_background_assets_create(
          store, bk_ending_normal_background(group, variant), error);
      CHECK(background);
      owner = bk_ending_auxiliary_assets_create_reloaded(
          store, group, variant, background, clocks, &random, &camera,
          &presets, error);
      CHECK(owner);
      CHECK(bk_ending_auxiliary_assets_background(owner));
      CHECK(bk_ending_auxiliary_assets_pose(owner, 0));
      CHECK(bk_ending_auxiliary_assets_pose(owner, 1));
      CHECK(bk_ending_auxiliary_assets_pose(owner, 2));
      CHECK(bk_ending_auxiliary_assets_pose(owner, 3));
      CHECK(bk_ending_auxiliary_assets_root(owner, 0) != BK_FRAME_NONE);
      CHECK(bk_ending_auxiliary_assets_root(owner, 3) != BK_FRAME_NONE);
      CHECK(bk_ending_auxiliary_assets_cameras(owner));
      CHECK(bk_ending_auxiliary_assets_follow(owner) != BK_MODEL_NONE);
      CHECK(bk_ending_auxiliary_assets_target(owner, 0));
      CHECK(bk_ending_auxiliary_assets_target(owner, 1));
      CHECK(bk_ending_auxiliary_assets_target(owner, 2));
      /*These are held published caches, not current ANIM locals. Capture
       *the actual inputs and outputs for native4D4167..4D4203 replay.*/
      const unsigned target_ids[] = {5, 13, 0};
      printf("AUX_TARGET %u %u", group, variant);
      for (unsigned pass = 0; pass < 2; ++pass)
        for (unsigned i = 0; i < 3; ++i) {
          uint32_t frame = bk_ending_auxiliary_assets_node(owner, target_ids[i]);
          const float *world = bk_actor_pose_frame(
              bk_ending_auxiliary_assets_pose(owner, 0), frame);
          const float *target = bk_ending_auxiliary_assets_target(owner, i);
          CHECK(world && !memcmp(target, world + 12, 3 * sizeof(float)));
          const float *values = pass ? target : world + 12;
          for (unsigned j = 0; j < 3; ++j) {
            uint32_t bits; memcpy(&bits, values + j, sizeof(bits));
            printf(" %08x", bits);
          }
        }
      puts("");
      uint32_t anchor, actor_index, root_frame;
      BkActorPose *primary = bk_ending_auxiliary_assets_pose(owner, 0);
      const BkModel *model = bk_actor_pose_model(primary);
      CHECK(bk_actor_forest_binding(bk_ending_auxiliary_assets_forest(owner),
          bk_ending_auxiliary_assets_root(owner, 0), &actor_index, &root_frame));
      CHECK(actor_index == 0);
      CHECK(bk_model_find_frame_first(model, root_frame,
                                       "A_okosi", &anchor, error));
      CHECK(bk_ending_auxiliary_assets_anchor(owner) == anchor);
      for (unsigned i = 0; i < 4; ++i) {
        CHECK(bk_ending_auxiliary_assets_advance(owner, i == 3 ? 3 : 0,
                                                 1.0f / 60.0f, error));
        ++advances;
      }
      bk_ending_auxiliary_assets_destroy(owner);
      owner = NULL;
      bk_ending_background_assets_destroy(background);
      background = NULL;
      ++entries;
    }
  printf("PASS 4D39E6 resources: %u group/variant entries, %u primary/background advances; retained background and bk3_12/FAM/camera ownership\n", entries, advances);
  ok = 1;
done:
  bk_ending_auxiliary_assets_destroy(owner);
  bk_ending_background_assets_destroy(background);
  bk_resources_destroy(store);
  return !ok;
}
