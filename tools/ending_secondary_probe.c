#include "scene/ending_secondary_assets.h"
#include <stdio.h>
#include <string.h>

#define CHECK(expr) do { if (!(expr)) { \
  fprintf(stderr, "secondary probe line %d: %s: %s\n", __LINE__, #expr, error); \
  goto done; } } while (0)

static const float identity[16] = {
    1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
static BkMenuCamera initial_camera(void) {
  BkMenuCamera camera = {0};
  memcpy(camera.pose.world, identity, sizeof(identity));
  memcpy(camera.matrix, identity, sizeof(identity));
  camera.pose.world[12] = 21;
  camera.matrix[12] = -17;
  camera.focus[0] = 7;
  camera.focus[1] = 8;
  camera.focus[2] = 9;
  camera.fov = .4f;
  return camera;
}
int main(int argc, char **argv) {
  if (argc != 2)
    return 2;
  char error[256] = {0}, path[2048];
  int ok = 0;
  unsigned failures = 0, entries = 0, retries = 0;
  const char *packs[] = {"bk3_09", "fambom", "bk3_04", "bk3_03"};
  BkResourceStore *store = bk_resources_create(error);
  BkResourceStore *empty = bk_resources_create(error);
  BkEndingSecondaryAssets *owner = NULL;
  CHECK(store && empty);
  const uint32_t clocks[4] = {100, 110, 120, 130};
  /* Each prefix really loads progressively more resources. The group1
   * background fails last, after the primary/face/eyes/tracks were owned.
   * Caller-owned camera, presets and RNG must remain untouched on failure. */
  for (unsigned prefix = 0; prefix < 4; ++prefix) {
    BkMenuCamera camera = initial_camera(), held_camera = camera;
    BkEndingCameraPresets presets, held_presets;
    memset(&presets, 0x35, sizeof(presets));
    held_presets = presets;
    uint32_t random = 123;
    owner = bk_ending_secondary_assets_create(store, 1, 0, clocks, &random,
                                               &camera, &presets, error);
    CHECK(!owner && random == 123);
    CHECK(!memcmp(&camera, &held_camera, sizeof(camera)));
    CHECK(!memcmp(&presets, &held_presets, sizeof(presets)));
    ++failures;
    int n = snprintf(path, sizeof(path), "%s/%s.pp", argv[1], packs[prefix]);
    CHECK(n > 0 && (size_t)n < sizeof(path));
    CHECK(bk_resources_mount(store, packs[prefix], path, error));
  }
  for (unsigned group = 0; group < 5; ++group)
    for (unsigned variant = 0; variant < 2; ++variant) {
      BkMenuCamera camera = initial_camera();
      BkEndingCameraPresets presets;
      uint32_t random = 123 + entries;
      owner = bk_ending_secondary_assets_create(store, group, variant, clocks,
                    &random, &camera, &presets, error);
      CHECK(owner);
      BkActorPose *poses[3];
      for (unsigned i = 0; i < 3; ++i) {
        poses[i] = bk_ending_secondary_assets_pose(owner, i);
        CHECK(poses[i]);
        for (unsigned j = 0; j < i; ++j)
          CHECK(poses[i] != poses[j]);
      }
      CHECK(!bk_ending_secondary_assets_pose(owner, 4));
      CHECK(bk_ending_secondary_assets_root(owner, 0) != BK_FRAME_NONE);
      CHECK(bk_ending_secondary_assets_root(owner, 1) == BK_FRAME_NONE);
      float held_targets[3][3];
      for (unsigned i = 0; i < 3; ++i)
        memcpy(held_targets[i], bk_ending_secondary_assets_target(owner, i),
                 sizeof(held_targets[i]));
      if (group != 1) {
        CHECK(!bk_ending_secondary_assets_pose(owner, 3));
        CHECK(!bk_ending_secondary_assets_load_background(owner, empty, error));
        CHECK(!bk_ending_secondary_assets_pose(owner, 3));
        ++retries;
      }
      CHECK(bk_ending_secondary_assets_load_background(owner, store, error));
      BkActorPose *background = bk_ending_secondary_assets_pose(owner, 3);
      CHECK(background && bk_ending_secondary_assets_root(owner, 3) != BK_FRAME_NONE);
      CHECK(bk_ending_secondary_assets_load_background(owner, store, error));
      CHECK(background == bk_ending_secondary_assets_pose(owner, 3));
      for (unsigned i = 0; i < 3; ++i)
        CHECK(!memcmp(held_targets[i], bk_ending_secondary_assets_target(owner, i),
                         sizeof(held_targets[i])));
      bk_ending_secondary_assets_destroy(owner);
      owner = NULL;
      ++entries;
    }
  printf("PASS secondary resource lifecycle: %u entries, %u partial-load "
         "atomic failures, %u background failure/retries; independent "
         "primary/tracks/background owners; not playable phase2\n",
         entries, failures, retries);
  ok = 1;
done:
  bk_ending_secondary_assets_destroy(owner);
  bk_resources_destroy(empty);
  bk_resources_destroy(store);
  return !ok;
}
