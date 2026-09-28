#include "scene/selection_world.h"
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
/* Explicit service-order fixture, not a movie or audio implementation. */
typedef struct {
  BkSelectionWorld *world;
  BkClipState before[2];
  unsigned movies, voices, movie_expected, order;
} Services;
static int movie(void *p, char e[256]) {
  (void)e;
  Services *s = p;
  assert(s->movie_expected && !s->order);
  s->order = 1;
  ++s->movies;
  BkClipState state;
  for (unsigned i = 0; i < 2; ++i) {
    assert(
        bk_actor_pose_state(bk_selection_world_pose(s->world, 2 + i), &state));
    assert(!memcmp(&state, &s->before[i], sizeof(state)));
  }
  return 1;
}
static int voice(void *p, float seconds, float *out, char e[256]) {
  (void)seconds;
  (void)e;
  Services *s = p;
  assert(s->order == s->movie_expected);
  s->order = 2;
  *out = (float)(s->voices++ % 10);
  return 1;
}
int main(int argc, char **argv) {
  if (argc != 2)
    return 2;
  char error[256] = {0}, path[1024];
  BkResourceStore *store = bk_resources_create(error);
  BkSelectionWorld *world = NULL;
  BkSelectionActorAssets *retired = NULL;
  unsigned frames = 0, replacements = 0, snapshots = 0;
  uint64_t matrices = 0;
  int rc = 1;
  CHECK(store);
  const char *packs[] = {"bk3_01", "bk3_03", "bk3_04", "bk3_06"};
  for (unsigned i = 0; i < 4; ++i) {
    snprintf(path, sizeof(path), "%s/%s.pp", argv[1], packs[i]);
    CHECK(bk_resources_mount(store, packs[i], path, error));
  }
  CHECK(bk_resources_mount_directory(store, "faces", argv[1], 20480, error));
  for (unsigned retained = 0; retained < 5; ++retained) {
    BkMenuCamera camera = {0};
    for (unsigned i = 0; i < 16; ++i)
      camera.pose.world[i] = camera.matrix[i] = i % 5 == 0;
    camera.pose.world[12] = 9;
    uint32_t rng = 12345 + retained, clocks[4] = {1000, 1001, 1002, 1003};
    world = bk_selection_world_create(store, retained, 0, .016f, &camera,
                                      clocks, &rng, error);
    CHECK(world);
    assert(
        bk_actor_pose_model(bk_selection_world_pose(world, 3))->frame_count ==
        152); /* h01_60, even when retained selected group is not0. */
    Services service = {.world = world};
    BkSelectionWorldOps ops = {&service, movie, voice};
    for (unsigned body = 0; body < 10; ++body) {
      unsigned group = body / 2, alt = body % 2;
      BkClipState held[3];
      BkActorPose *poses[3];
      for (unsigned i = 0; i < 3; ++i) {
        poses[i] = bk_selection_world_pose(world, i);
        assert(bk_actor_pose_state(poses[i], &held[i]));
      }
      BkMenuCamera old_camera = camera;
      BkSelectionActorAssets *old_body = bk_selection_world_body(world);
      uint32_t old_rng = rng;
      assert(!bk_selection_world_replace(world, 5, 0, clocks, &rng, &retired,
                                         error));
      assert(rng == old_rng && !retired &&
             bk_selection_world_body(world) == old_body &&
             !memcmp(&old_camera, &camera, sizeof(camera)));
      CHECK(bk_selection_world_replace(world, group, (uint8_t)alt, clocks, &rng,
                                       &retired, error));
      assert(retired == old_body);
      for (unsigned i = 0; i < 3; ++i) {
        BkClipState after;
        assert(poses[i] == bk_selection_world_pose(world, i));
        assert(bk_actor_pose_state(poses[i], &after));
        assert(!memcmp(&held[i], &after, sizeof(after)));
      }
      assert(!memcmp(&old_camera, &camera, sizeof(camera)));
      bk_selection_actor_assets_destroy(retired);
      retired = NULL;
      ++replacements;
      BkActorForest *forest = bk_selection_world_forest(world);
      const BkFrameTree *tree = bk_actor_forest_tree(forest);
      uint32_t node = bk_frame_tree_first(tree, 0);
      assert(node == 1);
      for (unsigned i = 0; i < 4; ++i) {
        node = bk_frame_tree_next(tree, node);
        assert(node == bk_selection_world_root(world, i));
      }
      assert(bk_frame_tree_next(tree, node) == BK_FRAME_NONE);
      for (unsigned frame = 0; frame < 48; ++frame) {
        BkSelectionWorldInput in = {.selected = group,
                                    .camera_mode = (frame / 12) % 2,
                                    .buttons = frame % 4,
                                    .voice_active = (uint8_t)(frame % 3 != 0),
                                    .seconds = frame % 7 ? .033f : 0,
                                    .motion = {1.25f, -.75f},
                                    .timestamp = 1100 + frame * 137};
        for (unsigned j = 0; j < 3; ++j)
          in.face_clocks[j] = in.timestamp + j + 1;
        for (unsigned i = 0; i < 2; ++i)
          assert(bk_actor_pose_state(bk_selection_world_pose(world, 2 + i),
                                     &service.before[i]));
        service.movie_expected = alt;
        service.order = 0;
        old_camera = camera;
        if (alt || in.voice_active) {
          assert(!bk_selection_world_step(world, &in, NULL, &rng, error));
          assert(!memcmp(&old_camera, &camera, sizeof(camera)));
        }
        CHECK(bk_selection_world_step(world, &in, &ops, &rng, error));
        BkClipState after;
        assert(bk_actor_pose_state(poses[1], &after));
        assert(!memcmp(&held[1], &after, sizeof(after)));
        BkLightingPass pass;
        CHECK(bk_selection_world_pass(world, &pass, error));
        unsigned objects = 0;
        for (unsigned i = 0; i < pass.count; ++i) {
          BkLightingCommand *c = &pass.commands[i];
          BkSceneLighting *lights = bk_selection_world_lighting(world);
          if (c->kind == BK_PASS_AMBIENT || c->kind == BK_PASS_LIGHT_ENABLE)
            CHECK(bk_scene_lighting_command(lights, c));
          else if (c->kind == BK_PASS_OBJECT) {
            assert(c->target == bk_selection_world_root(world, 2 + objects));
            BkActorPose *body_pose = bk_selection_world_pose(world, 3);
            BkLighting snapshot;
            CHECK(bk_scene_lighting_values(
                lights, bk_actor_pose_frame(body_pose, 0),
                bk_actor_pose_model(body_pose)->frame_count * 16, &snapshot,
                error));
            assert(snapshot.spot_count == (objects && !alt ? 2u : 0u));
            const BkFrameVisit *visits;
            uint32_t count;
            CHECK(bk_actor_forest_draw(forest, c->target, &visits, &count,
                                       error));
            ++objects;
            ++snapshots;
          } else
            assert(c->kind == BK_PASS_FLUSH);
        }
        assert(objects == 2);
        for (unsigned i = 0; i < 4; ++i) {
          BkActorPose *pose = bk_selection_world_pose(world, i);
          const BkModel *m = bk_actor_pose_model(pose);
          for (unsigned f = 0; f < m->frame_count; ++f) {
            const float *a = bk_actor_pose_frame(pose, f),
                        *b = bk_actor_pose_parent_world(pose, f);
            for (unsigned j = 0; j < 16; ++j)
              assert(isfinite(a[j]) && isfinite(b[j]));
            matrices += 2;
          }
        }
        ++frames;
      }
    }
    assert(service.movies == 240 && service.voices == 320);
    bk_selection_world_destroy(world);
    world = NULL;
  }
  printf("selection world PASS: 5 retained cameras, %u replacements, "
         "%u CPU frames, %u draw snapshots, %llu finite matrices\n",
         replacements, frames, snapshots, (unsigned long long)matrices);
  rc = 0;
done:
  if (rc)
    fprintf(stderr, "%s\n", error);
  bk_selection_actor_assets_destroy(retired);
  bk_selection_world_destroy(world);
  bk_resources_destroy(store);
  return rc;
}
