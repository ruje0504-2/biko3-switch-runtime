#include "scene/ending_normal_assets.h"
#include "scene/lighting_registry.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x)                                                               \
  do {                                                                         \
    if (!(x)) {                                                                \
      fprintf(stderr, "line %d: %s\n", __LINE__, e);                           \
      goto done;                                                               \
    }                                                                          \
  } while (0)
static const float I[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
static uint64_t hash(uint64_t h, const void *data, size_t n) {
  const unsigned char *b = data;
  while (n--)
    h = (h ^ *b++) * UINT64_C(1099511628211);
  return h;
}
int main(int argc, char **argv) {
  if (argc < 2 || argc > 3 || (argc == 3 && strcmp(argv[2], "--background")))
    return 2;
  int background = argc == 3;
  char e[256] = {0}, path[1024];
  int rc = 1;
  unsigned frames = 0, loads = 0, light_snapshots = 0;
  BkResourceStore *s = bk_resources_create(e), *empty = NULL;
  BkEndingNormalAssets *a = NULL;
  BkSceneLightRegistry *lights = NULL, *stress = NULL;
  uint64_t h = UINT64_C(14695981039346656037);
  CHECK(s);
  if (background) {
    empty = bk_resources_create(e);
    CHECK(empty);
  }
  const char *packs[] = {"bk3_08", "bk3_04", "bk3_03", "fambom"};
  for (unsigned i = 0; i < 4; i++) {
    snprintf(path, sizeof(path), "%s/%s.pp", argv[1], packs[i]);
    CHECK(bk_resources_mount(s, packs[i], path, e));
  }
  for (unsigned repetition = 0; repetition < 3; repetition++)
    for (unsigned group = 0; group < 5; group++)
      for (unsigned variant = 0; variant < 2; variant++) {
        uint32_t clocks[] = {100, 110, 120, 130}, rng = 123 + repetition;
        BkMenuCamera camera = {.focus = {7, 8, 9}, .fov = .4f};
        memcpy(camera.pose.world, I, 64);
        memcpy(camera.matrix, I, 64);
        camera.pose.world[12] = 21;
        BkEndingCameraPresets presets;
        a = bk_ending_normal_assets_create(s, group, variant, clocks, &rng,
                                           &camera, &presets, e);
        CHECK(a);
        loads++;
        BkActorPose *primary = bk_ending_normal_assets_pose(a, 0),
                    *aux = bk_ending_normal_assets_pose(a, 1);
        CHECK(bk_ending_normal_assets_config(a)->expression_a == 9 &&
              camera.fov == 1);
        CHECK((bk_ending_normal_assets_pose(a, 4) != NULL) == (group == 1));
        const BkModel *m = bk_actor_pose_model(primary);
        uint32_t hidden;
        CHECK(bk_actor_pose_hidden(aux, 0, &hidden) && hidden);
        CHECK(bk_ending_normal_assets_node(a, 0) != BK_MODEL_NONE);
        h = hash(h, bk_ending_normal_assets_config(a),
                 sizeof(BkEndingNormalConfig));
        h = hash(h, &camera, sizeof(camera));
        BkActorForest *forest = bk_ending_normal_assets_forest(a);
        if (background) {
          uint32_t count = bk_frame_tree_count(bk_actor_forest_tree(forest));
          float targets[3][3];
          for (unsigned i = 0; i < 3; i++)
            memcpy(targets[i], bk_ending_normal_assets_target(a, i), 12);
          if (group != 1) {
            CHECK(!bk_ending_normal_assets_load_background(a, empty, e));
            CHECK(!bk_ending_normal_assets_pose(a, 4) &&
                  count == bk_frame_tree_count(bk_actor_forest_tree(forest)));
          }
          CHECK(bk_ending_normal_assets_load_background(a, s, e));
          CHECK(bk_ending_normal_assets_pose(a, 4));
          CHECK(forest == bk_ending_normal_assets_forest(a));
          /* A repeated successful request does not read resources or refresh
           * cached camera targets. */
          CHECK(bk_ending_normal_assets_load_background(a, empty, e));
          for (unsigned i = 0; i < 3; i++)
            CHECK(
                !memcmp(targets[i], bk_ending_normal_assets_target(a, i), 12));
          BkLightSource sources[2];
          for (unsigned i = 0; i < 2; i++) {
            BkActorPose *pose = bk_ending_normal_assets_pose(a, i ? 4 : 0);
            sources[i].model = bk_actor_pose_model(pose);
            sources[i].world.matrices =
                bk_actor_pose_world(pose, &sources[i].world.floats);
          }
          lights = bk_scene_light_registry_create(sources, 2, e);
          CHECK(lights);
          if (!repetition && !group && !variant) {
            BkLightSource invalid[] = {sources[0], sources[1]};
            --invalid[1].world.floats;
            stress = bk_scene_light_registry_create(invalid, 2, e);
            CHECK(!stress);
            BkLightSource copies[] = {sources[0], sources[0], sources[0],
                                      sources[0]};
            /* Twenty authored records exceed the native16-slot registry;
             * fifteen fit the registry but twelve enabled point/spot lights
             * cannot fit one GPU snapshot. Both must fail without leakage. */
            stress = bk_scene_light_registry_create(copies, 4, e);
            CHECK(!stress);
            stress = bk_scene_light_registry_create(copies, 3, e);
            CHECK(stress);
            BkLightingPassInput input = {.mode = 0};
            BkLightingPass pass;
            CHECK(bk_scene_light_registry_input(stress, &input));
            CHECK(bk_lighting_pass(&input, &pass));
            for (unsigned i = 0; i < pass.count; i++)
              if (pass.commands[i].kind <= BK_PASS_LIGHT_ENABLE)
                CHECK(
                    bk_scene_light_registry_command(stress, &pass.commands[i]));
            BkLighting out = {0}, saved = out;
            BkLightWorld worlds[] = {sources[0].world, sources[0].world,
                                     sources[0].world};
            CHECK(!bk_scene_light_registry_values(stress, worlds, 3, &out, e));
            CHECK(!memcmp(&out, &saved, sizeof(out)));
            bk_scene_light_registry_destroy(stress);
            stress = NULL;
          }
        }
        /* Explicit component exercise, not the missing event/game controller.
         */
        for (unsigned step = 0; step < 60; step++) {
          float dt = step % 7 ? .016f : 0;
          CHECK(bk_actor_pose_advance(primary, -1, dt, e));
          if (background)
            CHECK(bk_actor_pose_advance(bk_ending_normal_assets_pose(a, 4), -1,
                                        dt, e));
          BkActorVisibilityEdit edit = {0, step % 4 == 0};
          CHECK(bk_actor_pose_visibility(aux, &edit, 1, e));
          CHECK(bk_bom_assets_advance(bk_ending_normal_assets_bom(a), dt, e));
          CHECK(bk_bom_assets_follow_references(bk_ending_normal_assets_bom(a),
                                                e));
          CHECK(bk_face_assets_step(
              bk_ending_normal_assets_face(a),
              bk_ending_normal_assets_face_state(a),
              bk_ending_normal_assets_config(a)->expression_a, step % 100,
              1000 + step * 17, 1001 + step * 17, 1002 + step * 17,
              1003 + step * 17, &rng, e));
          uint32_t target = bk_actor_forest_node(
              forest, 0, bk_ending_normal_assets_node(a, 1));
          CHECK(bk_ending_camera_assets_step(
              bk_ending_normal_assets_cameras(a), forest, (uint32_t[2]){2, 3},
              &camera,
              step % 2 ? BK_ENDING_CAMERA_AUTO : BK_ENDING_CAMERA_FIXED,
              bk_ending_normal_assets_target(a, 0), NULL, 0, target, dt, e));
          const BkFrameVisit *visits;
          uint32_t count;
          CHECK(bk_actor_forest_draw(forest, 0, &visits, &count, e));
          if (lights) {
            BkLightingPassInput input = {.mode = 1};
            input.objects[0] = 4;
            input.objects[4] = 1;
            input.objects[5] = 2;
            CHECK(bk_scene_light_registry_input(lights, &input));
            BkLightingPass pass;
            CHECK(bk_lighting_pass(&input, &pass));
            BkLightWorld worlds[2];
            for (unsigned i = 0; i < 2; i++)
              worlds[i].matrices = bk_actor_pose_world(
                  bk_ending_normal_assets_pose(a, i ? 4 : 0),
                  &worlds[i].floats);
            for (uint32_t i = 0; i < pass.count; i++) {
              BkLightingCommand *command = &pass.commands[i];
              if (command->kind <= BK_PASS_LIGHT_ENABLE)
                CHECK(bk_scene_light_registry_command(lights, command));
              else if (command->kind == BK_PASS_OBJECT) {
                BkLighting snapshot;
                CHECK(bk_scene_light_registry_values(lights, worlds, 2,
                                                     &snapshot, e));
                h = hash(h, &snapshot, sizeof(snapshot));
                light_snapshots++;
                BkLighting saved = snapshot;
                CHECK(!bk_scene_light_registry_values(lights, worlds, 1,
                                                      &snapshot, e));
                CHECK(!memcmp(&snapshot, &saved, sizeof(saved)));
              }
            }
          }
          for (uint32_t i = 0; i < m->frame_count; i++)
            h = hash(h, bk_actor_pose_frame(primary, i), 64);
          h = hash(h, &camera, sizeof(camera));
          if (background) {
            size_t floats;
            const float *world = bk_actor_pose_world(
                bk_ending_normal_assets_pose(a, 4), &floats);
            CHECK(world);
            h = hash(h, world, floats * sizeof(*world));
          }
          frames++;
        }
        bk_scene_light_registry_destroy(lights);
        lights = NULL;
        bk_ending_normal_assets_destroy(a);
        a = NULL;
      }
  bk_resources_destroy(empty);
  empty = bk_resources_create(e);
  CHECK(empty);
  BkMenuCamera camera = {0}, saved = camera;
  BkEndingCameraPresets p = {0}, saved_p = p;
  uint32_t rng = 17;
  CHECK(!bk_ending_normal_assets_create(empty, 0, 0, (uint32_t[4]){1, 2, 3, 4},
                                        &rng, &camera, &p, e));
  CHECK(rng == 17 && !memcmp(&camera, &saved, sizeof(camera)) &&
        !memcmp(&p, &saved_p, sizeof(p)));
  bk_resources_destroy(empty);
  empty = NULL;
  for (unsigned omitted = 0; omitted < 4; omitted++) {
    empty = bk_resources_create(e);
    CHECK(empty);
    for (unsigned i = 0; i < 4; i++) {
      if (i == omitted)
        continue;
      snprintf(path, sizeof(path), "%s/%s.pp", argv[1], packs[i]);
      CHECK(bk_resources_mount(empty, packs[i], path, e));
    }
    CHECK(!bk_ending_normal_assets_create(
        empty, 1, 0, (uint32_t[4]){1, 2, 3, 4}, &rng, &camera, &p, e));
    CHECK(rng == 17 && !memcmp(&camera, &saved, sizeof(camera)) &&
          !memcmp(&p, &saved_p, sizeof(p)));
    bk_resources_destroy(empty);
    empty = NULL;
  }
  printf("PASS normal ending assets background=%d loads=%u component_frames=%u "
         "light_snapshots=%u fnv=%016llx\n",
         background, loads, frames, light_snapshots, (unsigned long long)h);
  rc = 0;
done:
  bk_scene_light_registry_destroy(lights);
  bk_scene_light_registry_destroy(stress);
  bk_ending_normal_assets_destroy(a);
  bk_resources_destroy(empty);
  bk_resources_destroy(s);
  return rc;
}
