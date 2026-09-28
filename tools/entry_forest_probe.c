/* Real scene-owner binding. Explicit clocks/controller inputs, no mission,
 * pickup interaction or sound output; does not claim a playable level. */
#include "scene/entry_forest.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x)                                                               \
  do {                                                                         \
    if (!(x))                                                                  \
      goto done;                                                               \
  } while (0)
int main(int argc, char **argv) {
  if (argc != 2)
    return 2;
  char error[256] = {0}, path[1024];
  BkResourceStore *store = bk_resources_create(error);
  BkEntryAssets *entry = NULL;
  BkBackgroundAssets *background = NULL;
  BkPropAssets *props = NULL;
  BkItemAssets *items = NULL;
  BkEntryForest *assembly = NULL;
  float *held = NULL;
  uint8_t *visited = NULL;
  uint32_t profiles = 0, frames = 0, objects = 0, nodes = 0;
  uint64_t held_checks = 0, published = 0;
  int rc = 1;
  CHECK(store);
  const char *packs[] = {"bk3_01", "bk3_04", "bk3_03",
                         "bk3_07", "bk3_20", "bk3_16"};
  for (unsigned i = 0; i < sizeof(packs) / sizeof(*packs); ++i) {
    snprintf(path, sizeof(path), "%s/%s.pp", argv[1], packs[i]);
    CHECK(bk_resources_mount(store, packs[i], path, error));
  }
  CHECK(bk_resources_mount_directory(store, "routes", argv[1], 20480, error));
  CHECK(bk_resources_mount_directory(store, "faces", argv[1], 20480, error));
  CHECK(bk_resources_mount_directory(store, "collision", argv[1],
                                     BK_COLLISION_ATR_SIZE, error));
  for (unsigned group = 0; group < 5; ++group)
    for (unsigned area = 0; area < 9; ++area) {
      entry = bk_entry_assets_create(
          store, &(BkEntryRequest){group, area, 0, 8}, error);
      CHECK(entry);
      if (profiles % 2 == 0) {
        CHECK(bk_entry_assets_load_player_shadow(entry, store, error));
        CHECK(bk_entry_assets_load_mesh_shadow(entry, store, error));
      }
      background = bk_background_assets_create(store, group, area, 1, 1, error);
      CHECK(background);
      const BkActorPose *primary = bk_background_assets_pose(background, 0);
      const BkModel *model = bk_actor_pose_model(primary);
      props = bk_prop_assets_create(store, group, area, model,
                                    bk_actor_pose_frame(primary, 0),
                                    (size_t)model->frame_count * 16, error);
      CHECK(props);
      BkItemState retained_items[BK_ITEM_LIMIT] = {0};
      items = bk_item_assets_create(store, group, area, (uint8_t[5]){0},
                                    retained_items, error);
      CHECK(items);
      assembly = bk_entry_forest_create(entry, background, props, items, error);
      CHECK(assembly);
      BkActorForest *forest = bk_entry_forest_frames(assembly);
      const BkFrameTree *tree = bk_actor_forest_tree(forest);
      uint32_t count = bk_frame_tree_count(tree),
               object_count = bk_entry_forest_count(assembly);
      objects += object_count;
      nodes += count;
      assert(object_count ==
             4 + (profiles % 2 == 0 ? 2 : 0) + (group == 0 && area <= 6) +
                 (group == 2 && area == 8) + bk_prop_assets_count(props) +
                 bk_item_assets_count(items));
      held = malloc((size_t)count * 16 * sizeof(float));
      visited = malloc(count);
      CHECK(held && visited);
      uint32_t last_global = 1;
      for (uint32_t i = 0; i < object_count; ++i) {
        const BkEntryTreeObject *o = bk_entry_forest_object(assembly, i);
        assert(o &&
               bk_entry_forest_root(assembly, o->kind, o->index) == o->root);
        if (o->kind == BK_ENTRY_TREE_SNOW) {
          assert(bk_frame_tree_parent(tree, o->root) == 1);
        } else {
          assert(bk_frame_tree_parent(tree, o->root) == 0);
          assert(bk_frame_tree_next(tree, last_global) == o->root);
          last_global = o->root;
        }
      }
      assert(bk_frame_tree_next(tree, last_global) == BK_FRAME_NONE);
      BkActorPose *player = bk_entry_assets_player(entry),
                  *npc = bk_entry_assets_actor(entry);
      BkFollowCamera *camera = bk_entry_assets_camera(entry);
      BkBackgroundState background_state = {.music_volume = -6000};
      uint32_t random = 1;
      for (unsigned frame = 0; frame < 16; ++frame) {
        CHECK(bk_prop_assets_collision(
            props, bk_background_assets_collision(background), error));
        CHECK(bk_actor_pose_advance(player, -1, .016f, error));
        CHECK(bk_actor_pose_advance(npc, -1, .008f, error));
        CHECK(bk_follow_camera_step(
            camera, bk_actor_pose_placement(npc)->position,
            bk_actor_pose_head(npc), NULL, .016f, error));
        CHECK(bk_prop_assets_step_presentation(props, .016f, error));
        BkBackgroundInput in = {
            .seconds = .016f, .now = frame * 16, .weather_enabled = 1};
        memcpy(in.player, bk_actor_pose_placement(player)->position, 12);
        memcpy(in.npc, bk_actor_pose_placement(npc)->position, 12);
        BkBackgroundCommands commands;
        CHECK(bk_background_assets_step(background, &background_state, &random,
                                        &in, &commands, NULL, NULL, error));
        for (uint32_t n = 2; n < count; ++n)
          memcpy(held + n * 16, bk_actor_forest_world(forest, n), 64);
        for (uint32_t i = 0; i < bk_item_assets_count(items); ++i)
          CHECK(bk_item_assets_hidden(items, i, frame % 5 == 3));
        CHECK(bk_item_assets_step(items, .016f, error));
        float old_track[16];
        memcpy(old_track, bk_follow_camera_track(camera), 64);
        BkClipState before, after;
        CHECK(bk_follow_camera_clip_state(camera, &before));
        uint32_t target =
            frame % 2 ? bk_entry_forest_root(assembly, BK_ENTRY_TREE_PLAYER, 0)
                      : 0;
        const BkFrameVisit *walk;
        uint32_t walked;
        CHECK(bk_entry_forest_draw(assembly, target, &walk, &walked, error));
        CHECK(bk_follow_camera_clip_state(camera, &after));
        assert(memcmp(&before, &after, sizeof(before)) == 0);
        if (target)
          assert(memcmp(old_track, bk_follow_camera_track(camera), 64) == 0);
        memset(visited, 0, count);
        for (uint32_t v = 0; v < walked; ++v) {
          assert(walk[v].node < count && !visited[walk[v].node]);
          visited[walk[v].node] = 1;
          published++;
        }
        for (uint32_t n = 2; n < count; ++n)
          if (!visited[n]) {
            assert(memcmp(held + n * 16, bk_actor_forest_world(forest, n),
                          64) == 0);
            held_checks++;
          }
        bk_collision_end_props(bk_background_assets_collision(background));
        frames++;
      }
      free(held);
      held = NULL;
      free(visited);
      visited = NULL;
      bk_entry_forest_destroy(assembly);
      assembly = NULL;
      bk_item_assets_destroy(items);
      items = NULL;
      bk_prop_assets_destroy(props);
      props = NULL;
      bk_background_assets_destroy(background);
      background = NULL;
      bk_entry_assets_destroy(entry);
      entry = NULL;
      profiles++;
      printf("PASS bound entry g%u/a%u: %u objects, %u nodes\n", group, area,
             object_count, count);
      fflush(stdout);
    }
  printf("PASS %u bound entries, %u frames, %u objects, %u nodes; %llu visited "
         "caches, %llu held caches; real collision owners, optional "
         "shadows/door/snow, shared track; explicit controller inputs, no "
         "pickup/game flow; item models bound\n",
         profiles, frames, objects, nodes, (unsigned long long)published,
         (unsigned long long)held_checks);
  rc = 0;
done:
  if (rc)
    fprintf(stderr, "FAIL entry forest profile%u frame%u: %s\n", profiles,
            frames, error);
  free(held);
  free(visited);
  bk_entry_forest_destroy(assembly);
  bk_prop_assets_destroy(props);
  bk_item_assets_destroy(items);
  bk_background_assets_destroy(background);
  bk_entry_assets_destroy(entry);
  bk_resources_destroy(store);
  return rc;
}
