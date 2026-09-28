/* Real background/door resources and original light-pass policy. Inspection
 * cameras and explicit object groups; native main-flow binding, snow/rain
 * and projected shadow remain separate dependencies. */
#include "game/entry.h"
#include "model/environment.h"
#include "scene/actor_render.h"
#include "scene/background_assets.h"
#include "scene/lighting_assets.h"
#include "world/actor_forest.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#define WIDTH 320
#define HEIGHT 240
int main(int argc, char **argv) {
  if (argc != 2 && argc != 3)
    return 2;
  char error[256] = {0}, path[1024];
  BkResourceStore *store = bk_resources_create(error);
  BkRenderer *renderer = NULL;
  BkBackgroundAssets *assets = NULL;
  BkActorRender *objects[2] = {0};
  BkActorRenderBatch *batches[2] = {0};
  BkActorForest *forest = NULL;
  BkActorRenderVisit *submits[2] = {0};
  BkSceneLighting *scene_lights = NULL;
  const BkModelEnvironment *environment = NULL;
  BkLightSet *lights[2] = {0};
  uint8_t *pixels = malloc(WIDTH * HEIGHT * 4);
  unsigned profiles = 0, frames = 0, instances = 0, fogged = 0;
  uint64_t colored_total = 0;
  int rc = 1;
  if (!store || !pixels)
    goto done;
  const char *packs[] = {"bk3_03", "bk3_17"};
  for (unsigned i = 0; i < 2; ++i) {
    snprintf(path, sizeof(path), "%s/%s.pp", argv[1], packs[i]);
    if (!bk_resources_mount(store, packs[i], path, error))
      goto done;
  }
  if (!bk_resources_mount_directory(store, "collision", argv[1],
                                    BK_COLLISION_ATR_SIZE, error))
    goto done;
  renderer = bk_renderer_create(WIDTH, HEIGHT, stderr, error);
  if (!renderer)
    goto done;
  for (unsigned i = 0; i < 2; ++i) {
    batches[i] = bk_actor_render_batch_create(renderer, 8192, error);
    if (!batches[i])
      goto done;
  }
  BkFogState retained = {.end = 1, .density = 1};
  for (unsigned quality = 0; quality < 2; ++quality)
    for (unsigned group = 0; group < 5; ++group)
      for (unsigned area = 0; area < 9; ++area) {
        assets =
            bk_background_assets_create(store, group, area, quality, 0, error);
        if (!assets)
          goto done;
        const char *pack = quality ? "bk3_03" : "bk3_17";
        for (unsigned i = 0; i < 2; ++i) {
          const BkActorPose *pose = bk_background_assets_pose(assets, i);
          if (!pose)
            continue;
          objects[i] = bk_actor_render_create(
              renderer, store, pack, bk_actor_pose_model(pose), NULL, error);
          if (!objects[i])
            goto done;
        }
        BkActorPose *bound[] = {bk_background_assets_bind_pose(assets, 0),
                               bk_background_assets_bind_pose(assets, 1)};
        uint32_t actor_count = bound[1] ? 2 : 1, roots[2] = {0};
        forest = bk_actor_forest_create(bound, actor_count, error);
        if (!forest)
          goto done;
        /*4f6bb0 loads/attaches primary before the optional door. This probe
         * disables weather and has no other scene roots. */
        for (unsigned a = 0; a < actor_count; ++a) {
          const BkModel *m = bk_actor_pose_model(bound[a]);
          unsigned root_count = 0;
          for (uint32_t f = 0; f < m->frame_count; ++f)
            if (m->frames[f].parent_index == BK_MODEL_NONE) {
              roots[a] = bk_actor_forest_node(forest, a, f);
              ++root_count;
              if (!bk_actor_forest_attach(forest, 0, roots[a], error))
                goto done;
            }
          assert(root_count == 1);
        }
        uint32_t registry_count =
            bk_frame_tree_count(bk_actor_forest_tree(forest));
        for (unsigned d = 0; d < 2; ++d) {
          submits[d] = calloc(registry_count, sizeof(*submits[d]));
          if (!submits[d])
            goto done;
        }
        const BkActorPose *primary = bk_background_assets_pose(assets, 0);
        size_t world_count =
            (size_t)bk_actor_pose_model(primary)->frame_count * 16;
        scene_lights = bk_scene_lighting_create(bk_actor_pose_model(primary),
                                                bk_actor_pose_frame(primary, 0),
                                                world_count, error);
        if (!scene_lights)
          goto done;
        environment = bk_scene_lighting_environment(scene_lights);
        for (unsigned i = 0; i < 2; ++i)
          lights[i] = bk_light_set_create(renderer, &(BkLighting){0}, error);
        BkFog fog;
        if (!lights[0] || !lights[1] || !bk_fog_enable(&retained, 0, 0, 1, 1) ||
            !bk_fog_load(&retained, &environment->fog, 1, 1, error) ||
            !bk_fog_resolve(&retained, &fog, error))
          goto done;
        fogged += fog.enabled;
        BkEntrySelection entry;
        assert(
            bk_game_entry_select(&entry, &(BkEntryRequest){group, area, 0, 8}));
        BkBackgroundState state = {.music_volume = -6000};
        uint32_t random = 1;
        unsigned profile_pixels = 0;
        for (unsigned frame = 0; frame < 8; ++frame) {
          float camera[16], view[16], projection[16], target[3];
          memcpy(camera, bk_identity, 64);
          memcpy(camera + 12, entry.player_position, 12);
          camera[13] += 20;
          double angle =
              (entry.player_yaw + frame * 45) * 3.141592653589793 / 180;
          target[0] = camera[12] + (float)sin(angle) * 100;
          target[1] = camera[13] - 5;
          target[2] = camera[14] + (float)cos(angle) * 100;
          BkCameraLens lens = {1, .75f, .5f, 126384};
          if (!bk_camera_aim(camera, camera, target) ||
              !bk_camera_projection(projection, &lens))
            goto done;
          BkBackgroundInput input = {
              .seconds = .25f, .player = {84, 0, 0}, .npc = {200, 0, 0}};
          input.player[0] += frame < 4 ? 0 : 80;
          BkBackgroundCommands commands;
          if (!bk_background_assets_step(assets, &state, &random, &input,
                                         &commands, NULL, NULL, error))
            goto done;
          if (!bk_actor_forest_anchor(forest, 1, camera, 0, error))
            goto done;
          BkLightingPassInput pass_input = {.scene_root = 99};
          const int modes[] = {0, 1, 2, 10};
          pass_input.mode = modes[frame % 4];
          pass_input.objects[0] =
              pass_input.mode == 10 ? (objects[1] ? 2 : 0) : 1;
          pass_input.objects[4] =
              pass_input.mode == 10 ? 1 : (objects[1] ? 2 : 0);
          pass_input.objects[20] = 1;
          pass_input.objects[21] = objects[1] ? 2 : 0;
          BkLightingPass pass;
          if (!bk_scene_lighting_input(scene_lights, &pass_input) ||
              !bk_lighting_pass(&pass_input, &pass))
            goto done;
          unsigned draws = 0;
          uint32_t submitted[2] = {0};
          for (unsigned c = 0; c < pass.count; ++c) {
            BkLightingCommand *command = &pass.commands[c];
            if (command->kind == BK_PASS_AMBIENT ||
                command->kind == BK_PASS_LIGHT_ENABLE) {
              assert(bk_scene_lighting_command(scene_lights, command));
            } else if (command->kind == BK_PASS_OBJECT) {
              assert(draws < 2);
              const BkFrameVisit *walk;
              uint32_t walked;
              assert(command->target == 99 || command->target == 1 ||
                     command->target == 2);
              uint32_t target_node =
                  command->target == 99 ? 0 : roots[command->target - 1];
              if (!bk_actor_forest_draw(forest, target_node, &walk, &walked,
                                        error))
                goto done;
              memcpy(view, bk_actor_forest_view(forest), sizeof(view));
              for (uint32_t v = 0; v < walked; ++v) {
                uint32_t actor, frame_index;
                if (walk[v].submit &&
                    bk_actor_forest_binding(forest, walk[v].node, &actor,
                                             &frame_index))
                  submits[draws][submitted[draws]++] =
                      (BkActorRenderVisit){objects[actor], frame_index};
              }
              BkLighting snapshot;
              if (!bk_scene_lighting_values(scene_lights,
                                            bk_actor_pose_frame(primary, 0),
                                            world_count, &snapshot, error) ||
                  !bk_light_set_update(renderer, lights[draws], &snapshot,
                                       error) ||
                  !bk_light_set_view(renderer, lights[draws], camera + 12,
                                     error) ||
                  !bk_light_set_fog(renderer, lights[draws], &fog, view, error))
                goto done;
              ++draws;
            } else
              assert(command->kind == BK_PASS_FLUSH);
          }
          /* These independent rigid roots have no local changes between
           * passes. Their shared post-walk snapshot is valid for both passes;
           * this does not cover cross-root ENVL or mid-pass animation. */
          assert(draws > 0);
          for (unsigned i = 0; i < 2; ++i)
            if (objects[i]) {
              if (!bk_actor_render_prepare(
                      objects[i], bk_background_assets_pose(assets, i),
                      bk_background_assets_materials(assets, i), NULL, view,
                      projection, error))
                goto done;
              BkActorRenderStats stats;
              assert(bk_actor_render_stats(objects[i], &stats));
              instances += stats.submitted_instances;
            }
          for (unsigned d = 0; d < draws; ++d) {
            if (!bk_actor_render_batch_prepare_visits(
                    batches[d], submits[d], submitted[d], error))
              goto done;
          }
          if (!bk_renderer_begin(renderer, error))
            goto done;
          for (unsigned d = 0; d < draws; ++d)
            if (!bk_actor_render_batch_draw(batches[d], lights[d], error))
              goto done;
          if (!bk_renderer_end(renderer, error) ||
              !bk_renderer_readback(renderer, pixels, WIDTH * HEIGHT * 4,
                                    error))
            goto done;
          for (unsigned i = 0; i < WIDTH * HEIGHT; ++i)
            profile_pixels +=
                pixels[i * 4] || pixels[i * 4 + 1] || pixels[i * 4 + 2];
          if (argc == 3 && quality == 1 && frame == 0 &&
              (area == 5 || area == 8)) {
            snprintf(path, sizeof(path), "%s-g%u-a%u.ppm", argv[2], group,
                     area);
            FILE *out = fopen(path, "wb");
            if (!out)
              goto done;
            fprintf(out, "P6\n%u %u\n255\n", WIDTH, HEIGHT);
            int ok = 1;
            for (unsigned i = 0; i < WIDTH * HEIGHT; ++i)
              if (fwrite(pixels + i * 4, 1, 3, out) != 3) {
                ok = 0;
                break;
              }
            if (fclose(out) != 0 || !ok)
              goto done;
          }
          ++frames;
        }
        if (profile_pixels < 100) {
          snprintf(error, 256, "empty profile g%u/a%u quality%u", group, area,
                   quality);
          goto done;
        }
        colored_total += profile_pixels;
        bk_actor_forest_destroy(forest);
        forest = NULL;
        for (unsigned d = 0; d < 2; ++d) {
          free(submits[d]);
          submits[d] = NULL;
        }
        for (unsigned i = 0; i < 2; ++i) {
          bk_actor_render_destroy(objects[i]);
          objects[i] = NULL;
        }
        for (unsigned i = 0; i < 2; ++i) {
          bk_light_set_destroy(renderer, lights[i]);
          lights[i] = NULL;
        }
        bk_scene_lighting_destroy(scene_lights);
        scene_lights = NULL;
        environment = NULL;
        bk_background_assets_destroy(assets);
        assets = NULL;
        ++profiles;
        printf("PASS background quality%u g%u a%u: %u colored pixels\n",
               quality, group, area, profile_pixels);
        fflush(stdout);
      }
  printf(
      "PASS %u background GPU profiles, %u frames, %u submitted instances, %u "
      "fog profiles, %llu colored pixels; inspection views/explicit object "
      "groups, native light passes and bound frame forest\n",
      profiles, frames, instances, fogged, (unsigned long long)colored_total);
  rc = 0;
done:
  if (rc)
    fprintf(stderr, "FAIL background GPU profile%u frame%u: %s\n", profiles,
            frames, error);
  bk_actor_forest_destroy(forest);
  for (unsigned d = 0; d < 2; ++d)
    free(submits[d]);
  for (unsigned i = 0; i < 2; ++i)
    bk_actor_render_destroy(objects[i]);
  for (unsigned i = 0; i < 2; ++i)
    bk_light_set_destroy(renderer, lights[i]);
  bk_scene_lighting_destroy(scene_lights);
  bk_background_assets_destroy(assets);
  for (unsigned i = 0; i < 2; ++i)
    bk_actor_render_batch_destroy(batches[i]);
  bk_renderer_destroy(renderer);
  bk_resources_destroy(store);
  free(pixels);
  return rc;
}
