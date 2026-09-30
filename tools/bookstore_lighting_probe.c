/* Real m00_11 textures/light layers, with only the sort camera rotating.
 * Freeze raster transforms after prepare to isolate order from coverage/depth.
 * Private access is confined to this diagnostic executable, never production.
 * --native preserves the original exchange sort as an expected-failure control.
 */
#include "scene/actor_render.c"
#include "scene/background_assets.h"
#include "scene/lighting_assets.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#define W 960
#define H 720
#define CHECK(x)                                                               \
  do {                                                                         \
    if (!(x)) {                                                                \
      fprintf(stderr, "bookstore line%d (%s): %s\n", __LINE__, #x, error);     \
      goto done;                                                               \
    }                                                                          \
  } while (0)
int main(int argc, char **argv) {
  if (argc < 2 || argc > 3 || (argc == 3 && strcmp(argv[2], "--native")))
    return 2;
  int native = argc == 3, rc = 1;
  char error[256] = {0}, path[2048];
  BkResourceStore *store = NULL;
  BkBackgroundAssets *assets = NULL;
  BkRenderer *renderer = NULL;
  BkActorRender *actor = NULL;
  BkActorRenderBatch *batch = NULL;
  BkSceneLighting *scene_lights = NULL;
  BkLightSet *lights = NULL;
  uint8_t *pixels = NULL, *previous = NULL;
  uint64_t changed[3] = {0};
  unsigned changed_frames[3] = {0}, maximum[3] = {0}, worst[3] = {0};
  CHECK(pixels = malloc(W * H * 4));
  CHECK(previous = malloc(W * H * 4));
  CHECK(store = bk_resources_create(error));
  snprintf(path, sizeof(path), "%s/bk3_03.pp", argv[1]);
  CHECK(bk_resources_mount(store, "bk3_03", path, error));
  CHECK(bk_resources_mount_directory(store, "collision", argv[1],
                                     BK_COLLISION_ATR_SIZE, error));
  CHECK(assets = bk_background_assets_create(store, 0, 1, 1, 0, error));
  bk_background_assets_publish(assets);
  const BkActorPose *pose = bk_background_assets_pose(assets, 0);
  const BkModel *model = bk_actor_pose_model(pose);
  /* Viewer, world and all material/light values stay fixed for each view;
   * even unused/authored specular materials cannot affect this isolation. */
  size_t floats;
  const float *world = bk_actor_pose_world(pose, &floats);
  CHECK(scene_lights = bk_scene_lighting_create(model, world, floats, error));
  BkLightingPassInput input = {.mode = 0, .shadow_mode = 1};
  BkLightingPass pass;
  CHECK(bk_scene_lighting_input(scene_lights, &input));
  CHECK(bk_lighting_pass(&input, &pass));
  for (unsigned i = 0; i < pass.count; ++i)
    if (pass.commands[i].kind == BK_PASS_AMBIENT ||
        pass.commands[i].kind == BK_PASS_LIGHT_ENABLE)
      CHECK(bk_scene_lighting_command(scene_lights, &pass.commands[i]));
  BkLighting held;
  CHECK(bk_scene_lighting_values(scene_lights, world, floats, &held, error));
  CHECK(renderer = bk_renderer_create(W, H, stderr, error));
  CHECK(lights = bk_light_set_create(renderer, &held, error));
  CHECK(actor = bk_actor_render_create(renderer, store, "bk3_03", model, NULL,
                                       error));
  if (!native)
    CHECK(bk_actor_render_stabilize_layers(actor));
  CHECK(batch = bk_actor_render_batch_create(renderer, 4096, error));
  const float views[][6] = {
      {270, 25, 135, 309, 15, 142}, {280, 25, 220, 320, 25, 185},
      {220, 25, 155, 253, 15, 183}, {250, 25, 160, 230, 20, 242},
      {280, 25, 150, 305, 20, 95},  {285, 25, 200, 350, 20, 240},
      {240, 25, 140, 207, 15, 184}, {285, 25, 300, 320, 20, 350},
      {280, 25, 400, 350, 20, 450}, {180, 25, 180, 160, 20, 220},
      {40, 25, 120, 30, 20, 170},   {250, 25, 125, 290, 10, 145}};
  unsigned frames = 0;
  for (unsigned v = 0; v < sizeof(views) / sizeof(*views); ++v) {
    float raster[16];
    BkDepthTransform depth;
    CHECK(bk_light_set_view(renderer, lights, views[v], error));
    for (unsigned f = 0; f < 210; ++f) {
      unsigned phase = f < 45 ? 0 : f < 180 ? 1 : 2;
      float motion = phase == 0 ? sinf(f * .13f) * 4.f
                     : phase == 1
                         ? sinf(44 * .13f) * 4.f * expf(-(float)(f - 44) / 30.f)
                         : 0;
      float camera[16], target[3], view[16], projection[16];
      memcpy(camera, bk_identity, sizeof(camera));
      memcpy(camera + 12, views[v], 12);
      memcpy(target, views[v] + 3, 12);
      target[0] += motion;
      CHECK(bk_camera_aim(camera, camera, target));
      CHECK(bk_camera_view(view, camera));
      CHECK(bk_camera_projection(projection,
                                 &(BkCameraLens){1, .75f, .5f, 126384}));
      BkLighting current;
      CHECK(bk_scene_lighting_values(scene_lights, world, floats, &current,
                                     error));
      CHECK(!memcmp(&held, &current, sizeof(held)));
      CHECK(bk_actor_render_prepare(actor, pose,
                                    bk_background_assets_materials(assets, 0),
                                    NULL, view, projection, error));
      if (!f) {
        memcpy(raster, actor->vp, sizeof(raster));
        depth = actor->depth;
      }
      memcpy(actor->vp, raster, sizeof(raster));
      actor->depth = depth;
      CHECK(bk_actor_render_batch_prepare(batch, &actor, 1, error));
      CHECK(bk_renderer_begin(renderer, error));
      CHECK(bk_actor_render_batch_draw(batch, lights, error));
      CHECK(bk_renderer_end(renderer, error));
      CHECK(bk_renderer_readback(renderer, pixels, W * H * 4, error));
      if (f) {
        unsigned count = 0;
        for (unsigned i = 0; i < W * H; ++i) {
          unsigned delta = 0;
          for (unsigned c = 0; c < 3; ++c) {
            unsigned d = abs((int)pixels[i * 4 + c] - previous[i * 4 + c]);
            if (d > delta)
              delta = d;
          }
          count += delta > 1;
          if (delta > maximum[phase])
            maximum[phase] = delta;
        }
        changed[phase] += count;
        changed_frames[phase] += count != 0;
        if (count > worst[phase])
          worst[phase] = count;
      }
      memcpy(previous, pixels, W * H * 4);
      ++frames;
    }
  }
  printf("%s bookstore %s: views12 frames%u lights%u; "
         "changed_pixels%llu/%llu/%llu changed_frames%u/%u/%u "
         "worst%u/%u/%u max_rgb%u/%u/%u (move/decay/hold)\n",
         changed[0] + changed[1] + changed[2] ? "FAIL" : "PASS",
         native ? "native" : "stable", frames, held.point_count,
         (unsigned long long)changed[0], (unsigned long long)changed[1],
         (unsigned long long)changed[2], changed_frames[0], changed_frames[1],
         changed_frames[2], worst[0], worst[1], worst[2], maximum[0],
         maximum[1], maximum[2]);
  CHECK(!changed[0] && !changed[1] && !changed[2]);
  rc = 0;
done:
  bk_actor_render_batch_destroy(batch);
  bk_actor_render_destroy(actor);
  bk_light_set_destroy(renderer, lights);
  bk_scene_lighting_destroy(scene_lights);
  bk_background_assets_destroy(assets);
  bk_renderer_destroy(renderer);
  bk_resources_destroy(store);
  free(previous);
  free(pixels);
  return rc;
}
