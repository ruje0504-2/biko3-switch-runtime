/* Explicit orthographic/gray-ground fixture for the original mesh shadow. */
#include "scene/actor_render.h"
#include "scene/npc_shadow.h"
#include <stdlib.h>
#include <string.h>
#define W 96
int main(int argc, char **argv) {
  if (argc < 2 || argc > 3) {
    fprintf(stderr, "usage: shadow-render-probe DATA [RGBA]\n");
    return 2;
  }
  char error[256] = {0}, path[1024];
  int ok = 0;
  BkResourceStore *store = bk_resources_create(error);
  BkNpcShadow *shadow = NULL;
  BkActorRender *actor = NULL;
  BkRenderer *r = NULL;
  BkLightSet *lights = NULL;
  BkTexture *white = NULL;
  BkGpuMesh *ground = NULL;
  uint8_t pixels[W * W * 4], baseline[W * W * 4], first[W * W * 4];
  if (!store ||
      snprintf(path, sizeof(path), "%s/bk3_01.pp", argv[1]) >=
          (int)sizeof(path) ||
      !bk_resources_mount(store, "bk3_01", path, error))
    goto done;
  shadow = bk_npc_shadow_create(store, error);
  if (!shadow)
    goto done;
  const BkActorPose *pose = bk_npc_shadow_pose(shadow);
  r = bk_renderer_create(W, W, stderr, error);
  if (!r)
    goto done;
  actor = bk_actor_render_create(r, store, "bk3_01", bk_actor_pose_model(pose),
                                 NULL, error);
  BkLighting ambient = {.ambient = {1, 1, 1}};
  lights = bk_light_set_create(r, &ambient, error);
  uint8_t rgba[4] = {255, 255, 255, 255};
  BkImage im = {1, 1, rgba};
  white = bk_texture_create(r, &im, error);
  BkVertex v[4] = {{-1, -1, .99f, 0, 0, .8f, .8f, .8f, 1},
                   {1, -1, .99f, 0, 0, .8f, .8f, .8f, 1},
                   {1, 1, .99f, 0, 0, .8f, .8f, .8f, 1},
                   {-1, 1, .99f, 0, 0, .8f, .8f, .8f, 1}};
  const uint16_t indices[6] = {0, 1, 2, 0, 2, 3};
  ground = bk_mesh_create(r, v, 4, indices, 6, error);
  if (!actor || !lights || !white || !ground)
    goto done;
  bk_resources_destroy(store);
  store = NULL;
  const float view[16] = {1, 0, 0, 0, 0, 0, -1, 0, 0, 1, 0, 0, 0, 0, 40, 1};
  const float projection[16] = {1.f / 30, 0, 0,    0, 0, -1.f / 30, 0, 0,
                                0,        0, .01f, 0, 0, 0,         0, 1};
  double previous = -1;
  unsigned readbacks = 0;
  for (unsigned frame = 0; frame < 9; frame++) {
    uint8_t hidden = frame == 0 || frame == 4 || frame == 8;
    float position[3] = {frame < 4 ? ((int)frame - 2) * 10.f : 0, 0, 0};
    BkActorPlacement body;
    if (!bk_actor_placement(&body, position, 0) ||
        !bk_npc_shadow_place(shadow, &body, error) ||
        !bk_npc_shadow_step(shadow, hidden, frame * .2f, error))
      goto done;
    bk_npc_shadow_publish(shadow);
    if (!bk_actor_render_prepare(actor, pose, NULL, NULL, view, projection,
                                 error) ||
        !bk_renderer_begin(r, error) ||
        !bk_renderer_draw_mesh(r, white, ground, bk_identity,
                               (BkDrawState){BK_BLEND_OPAQUE, 0, BK_CULL_NONE},
                               error) ||
        !bk_actor_render_draw(actor, lights, error) ||
        !bk_renderer_end(r, error) ||
        !bk_renderer_readback(r, pixels, sizeof(pixels), error))
      goto done;
    readbacks++;
    if (!frame) {
      memcpy(baseline, pixels, sizeof(pixels));
      continue;
    }
    unsigned changed = 0;
    double centroid = 0;
    for (unsigned i = 0; i < W * W; i++) {
      if (pixels[i * 4] > baseline[i * 4] + 1 ||
          pixels[i * 4 + 1] != pixels[i * 4] ||
          pixels[i * 4 + 2] != pixels[i * 4]) {
        snprintf(error, 256, "shadow brightened or tinted ground");
        goto done;
      }
      if (pixels[i * 4] < baseline[i * 4] - 2) {
        changed++;
        centroid += i % W;
      }
    }
    if (hidden) {
      if (memcmp(baseline, pixels, sizeof(pixels))) {
        snprintf(error, 256, "hidden shadow changed ground");
        goto done;
      }
    } else {
      if (changed < 50) {
        snprintf(error, 256, "shadow had only %u changed pixels", changed);
        goto done;
      }
      centroid /= changed;
      if (frame < 4) {
        if (centroid <= previous) {
          snprintf(error, 256, "shadow did not follow translated body");
          goto done;
        }
        previous = centroid;
      }
      if (frame == 2)
        memcpy(first, pixels, sizeof(pixels));
      if (frame >= 5 && memcmp(first, pixels, sizeof(pixels))) {
        snprintf(error, 256,
                 "static shadow geometry drifted with its XAN clock");
        goto done;
      }
    }
  }
  if (argc == 3) {
    FILE *f = fopen(argv[2], "wb");
    if (!f)
      goto done;
    size_t written = fwrite(first, 1, sizeof(first), f);
    int closed = fclose(f);
    if (written != sizeof(first) || closed)
      goto done;
  }
  printf("PASS actual kage_01: %u Vulkan readbacks, inverse-color darkening, "
         "root following, hidden restore and no-SRT clock stability\n",
         readbacks);
  ok = 1;
done:
  if (!ok)
    fprintf(stderr, "shadow probe FAILED: %s\n", error);
  bk_actor_render_destroy(actor);
  bk_mesh_destroy(r, ground);
  bk_texture_destroy(r, white);
  bk_light_set_destroy(r, lights);
  bk_renderer_destroy(r);
  bk_npc_shadow_destroy(shadow);
  bk_resources_destroy(store);
  return ok ? 0 : 1;
}
