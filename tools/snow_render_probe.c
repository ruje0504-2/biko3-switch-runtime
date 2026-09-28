/* Real yuki.xan under moving external cameras. Tests GPU resources, hierarchy
 * cancellation and animated visible output; ambient-white inspection light. */
#include "scene/actor_render.h"
#include "scene/background_assets.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#define W 320
#define H 240
int main(int argc, char **argv) {
  if (argc != 2 && argc != 3)
    return 2;
  char error[256] = {0}, path[1024];
  BkResourceStore *store = bk_resources_create(error);
  BkBackgroundAssets *assets = NULL;
  BkRenderer *renderer = NULL;
  BkActorRender *snow = NULL;
  BkLightSet *lights = NULL;
  uint8_t *pixels = malloc(W * H * 4), *baseline = malloc(W * H * 4),
          *previous = calloc(W * H, 4);
  unsigned frames = 0, max_changed = 0, animated = 0, instances = 0;
  uint64_t colored = 0;
  int rc = 1;
  if (!store || !pixels || !baseline || !previous)
    goto done;
  const char *packs[] = {"bk3_03", "bk3_20"};
  for (unsigned i = 0; i < 2; ++i) {
    snprintf(path, sizeof(path), "%s/%s.pp", argv[1], packs[i]);
    if (!bk_resources_mount(store, packs[i], path, error))
      goto done;
  }
  if (!bk_resources_mount_directory(store, "collision", argv[1],
                                    BK_COLLISION_ATR_SIZE, error))
    goto done;
  assets = bk_background_assets_create(store, 0, 0, 1, 1, error);
  renderer = bk_renderer_create(W, H, stderr, error);
  if (!assets || !renderer)
    goto done;
  const BkActorPose *pose = bk_background_assets_pose(assets, 2);
  assert(pose);
  snow = bk_actor_render_create(renderer, store, "bk3_20",
                                bk_actor_pose_model(pose), NULL, error);
  BkLighting lighting = {.ambient = {1, 1, 1}};
  lights = bk_light_set_create(renderer, &lighting, error);
  if (!snow || !lights)
    goto done;
  BkBackgroundState state = {.music_volume = -6000};
  uint32_t random = 1;
  float projection[16];
  if (!bk_camera_projection(projection, &(BkCameraLens){1, .75f, .5f, 126384}))
    goto done;
  for (unsigned frame = 0; frame < 24; ++frame) {
    BkBackgroundInput input = {.seconds = .25f, .weather_enabled = 1};
    BkBackgroundCommands commands;
    if (!bk_background_assets_step(assets, &state, &random, &input, &commands,
                                   NULL, NULL, error))
      goto done;
    for (unsigned variant = 0; variant < 3; ++variant) {
      float camera[16], view[16], target[3];
      memcpy(camera, bk_identity, 64);
      camera[12] = variant * 43.25f;
      camera[13] = 10 + variant * 9.5f;
      camera[14] = variant * -17.125f;
      double angle = variant * .63;
      target[0] = camera[12] + (float)sin(angle) * 100;
      target[1] = camera[13] + variant * 12;
      target[2] = camera[14] + (float)cos(angle) * 100;
      if (!bk_camera_aim(camera, camera, target) ||
          !bk_camera_view(view, camera) ||
          !bk_background_assets_publish_camera(assets, camera, error) ||
          !bk_light_set_view(renderer, lights, camera + 12, error) ||
          !bk_actor_render_prepare(snow, pose,
                                   bk_background_assets_materials(assets, 2),
                                   NULL, view, projection, error) ||
          !bk_renderer_begin(renderer, error) ||
          !bk_actor_render_draw(snow, lights, error) ||
          !bk_renderer_end(renderer, error) ||
          !bk_renderer_readback(renderer, pixels, W * H * 4, error))
        goto done;
      BkActorRenderStats stats;
      assert(bk_actor_render_stats(snow, &stats));
      instances += stats.submitted_instances;
      unsigned visible = 0, changed = 0;
      for (unsigned p = 0; p < W * H; ++p) {
        visible += pixels[p * 4] || pixels[p * 4 + 1] || pixels[p * 4 + 2];
        if (variant) {
          int d = 0;
          for (unsigned c = 0; c < 4; ++c)
            d |= abs((int)pixels[p * 4 + c] - baseline[p * 4 + c]) > 3;
          changed += d != 0;
        }
      }
      assert(visible > 50);
      colored += visible;
      /* Subpixel cancellation can move a triangle edge; require >99.9%
       * pixels within3/255, rather than demanding bitwise raster identity. */
      assert(changed <= W * H / 1000);
      if (changed > max_changed)
        max_changed = changed;
      if (!variant) {
        if (frame && memcmp(previous, pixels, W * H * 4))
          ++animated;
        memcpy(previous, pixels, W * H * 4);
        memcpy(baseline, pixels, W * H * 4);
        if (frame == 12 && argc == 3) {
          FILE *out = fopen(argv[2], "wb");
          if (!out)
            goto done;
          fprintf(out, "P6\n%d %d\n255\n", W, H);
          int ok = 1;
          for (unsigned p = 0; p < W * H; ++p)
            if (fwrite(pixels + p * 4, 1, 3, out) != 3) {
              ok = 0;
              break;
            }
          if (fclose(out) || !ok)
            goto done;
        }
      }
      ++frames;
    }
  }
  assert(animated > 12);
  printf(
      "PASS snow: %u GPU frames, %u mesh instances, %llu colored pixels, %u "
      "animated changes, maximum %u edge pixels differ under3 moving cameras\n",
      frames, instances, (unsigned long long)colored, animated, max_changed);
  rc = 0;
done:
  if (rc)
    fprintf(stderr, "FAIL snow frame%u: %s\n", frames, error);
  bk_actor_render_destroy(snow);
  bk_light_set_destroy(renderer, lights);
  bk_renderer_destroy(renderer);
  bk_background_assets_destroy(assets);
  bk_resources_destroy(store);
  free(previous);
  free(baseline);
  free(pixels);
  return rc;
}
