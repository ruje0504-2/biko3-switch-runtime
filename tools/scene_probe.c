/* Synthetic GPU geometry contract plus optional original scene lifetime test.
 */
#include "core/matrix.h"
#include "scene/scene.h"
#include <stdlib.h>
#include <string.h>
#define WIDTH 1280
#define HEIGHT 720
#define BYTES ((size_t)WIDTH * HEIGHT * 4)
static int capture(BkRenderer *r, BkScene *scene, uint8_t *pixels,
                   char error[256]) {
  BkSceneFrame frame = {0};
  return bk_renderer_begin(r, error) && bk_scene_draw(scene, &frame, error) &&
         bk_renderer_end(r, error) &&
         bk_renderer_readback(r, pixels, BYTES, error);
}
static int save(const char *prefix, const char *name, const uint8_t *data,
                char error[256]) {
  if (!prefix)
    return 1;
  char path[1024];
  if (snprintf(path, sizeof(path), "%s-%s.rgba", prefix, name) >=
      (int)sizeof(path))
    return 0;
  FILE *file = fopen(path, "wb");
  if (!file) {
    snprintf(error, 256, "capture output open failed");
    return 0;
  }
  int written = fwrite(data, 1, BYTES, file) == BYTES;
  int closed = fclose(file) == 0;
  if (!written || !closed)
    snprintf(error, 256, "capture output write failed");
  return written && closed;
}
static int geometry(BkRenderer *r, uint8_t *pixels, char error[256]) {
  uint8_t colors[] = {255, 0,   0, 255, 0, 255, 0,   255,
                      255, 255, 0, 255, 0, 0,   255, 255};
  BkImage im = {2, 2, colors};
  BkTexture *texture = bk_texture_create(r, &im, error);
  if (!texture)
    return 0;
  int ok = 0;
  for (unsigned test = 0; test < 6; test++) {
    float uv = test == 5 ? .75f : .25f;
    BkVertex v[] = {{-1, -1, 0, uv, uv, 1, 1, 1, 1},
                    {0, 1, 0, uv, uv, 1, 1, 1, 1},
                    {1, -1, 0, uv, uv, 1, 1, 1, 1}};
    uint16_t forward[] = {0, 1, 2}, reverse[] = {0, 2, 1};
    BkGpuMesh *mesh = bk_mesh_create(
        r, v, 3, test == 1 || test == 2 ? reverse : forward, 3, error);
    float world[16], projection[16], mvp[16];
    memcpy(world, bk_identity, sizeof(world));
    world[14] = test == 3 ? .1f : test == 4 ? 11 : 2;
    int projected = bk_matrix_projection(projection, 1.570796327f, 1, .2f, 10);
    bk_matrix_multiply(mvp, world, projection);
    int drawn =
        mesh && projected && bk_renderer_begin(r, error) &&
        bk_renderer_draw_mesh(
            r, texture, mesh, mvp,
            (BkDrawState){BK_BLEND_OPAQUE, 1,
                          test == 2 ? BK_CULL_NONE : BK_CULL_COUNTER_CLOCKWISE},
            error) &&
        bk_renderer_end(r, error) &&
        bk_renderer_readback(r, pixels, BYTES, error);
    bk_mesh_destroy(r, mesh);
    if (!drawn)
      goto done;
    const uint8_t *pixel = pixels + ((HEIGHT / 2) * WIDTH + WIDTH / 2) * 4;
    int visible = test == 0 || test == 2 || test == 5;
    uint8_t expected[] = {visible && test != 5 ? 255 : 0, 0,
                          test == 5 ? 255 : 0, 255};
    if (memcmp(pixel, expected, 4)) {
      snprintf(error, 256, "geometry case %u: RGBA %u,%u,%u,%u", test, pixel[0],
               pixel[1], pixel[2], pixel[3]);
      goto done;
    }
  }
  fprintf(stderr, "PASS: perspective, clockwise front/reversed back, cull "
                  "disable, near/far clip, UV corners\n");
  ok = 1;
done:
  bk_texture_destroy(r, texture);
  return ok;
}
static int originals(BkRenderer *r, const char *root, const char *prefix,
                     uint8_t *pixels, char error[256]) {
  BkResourceStore *store = bk_resources_create(error), *empty = NULL;
  BkScene *scene = NULL, *next = NULL;
  uint8_t *baseline = malloc(BYTES);
  int ok = 0;
  if (!store || !baseline)
    goto done;
  const char *packs[] = {"bk3_00", "bk3_03"};
  for (unsigned i = 0; i < 2; i++) {
    char path[1024];
    if (snprintf(path, sizeof(path), "%s/Data/%s.pp", root, packs[i]) >=
            (int)sizeof(path) ||
        !bk_resources_mount(store, packs[i], path, error))
      goto done;
  }
  BkSceneServices services = {store, r, stderr, NULL, NULL, NULL};
  scene = bk_scene_create(BK_SCENE_STATIC_WORLD, &services, error);
  if (!scene || !capture(r, scene, baseline, error) ||
      !save(prefix, "office", baseline, error))
    goto done;
  for (unsigned i = 0; i < 15; i++) {
    if (!capture(r, scene, pixels, error))
      goto done;
    if (memcmp(pixels, baseline, BYTES)) {
      snprintf(error, 256, "static frame changed");
      goto done;
    }
  }
  BkInput move = {.move_y = 1, .look_x = .3f, .held = BK_BUTTON_UP};
  for (unsigned i = 0; i < 60; i++)
    if (!bk_scene_step(scene, 1.0 / 60, &move, error))
      goto done;
  if (!capture(r, scene, pixels, error) ||
      !save(prefix, "moved", pixels, error))
    goto done;
  if (!memcmp(pixels, baseline, BYTES)) {
    snprintf(error, 256, "camera movement had no effect");
    goto done;
  }
  BkInput reset = {.pressed = BK_BUTTON_CONFIRM};
  if (!bk_scene_step(scene, 1.0 / 60, &reset, error) ||
      !capture(r, scene, pixels, error))
    goto done;
  if (memcmp(pixels, baseline, BYTES)) {
    snprintf(error, 256, "camera reset differs");
    goto done;
  }
  /* Same lifetime order as application: construct next before destroying old.
   */
  for (unsigned i = 0; i < 3; i++) {
    next = bk_scene_create(BK_SCENE_TITLE_PREVIEW, &services, error);
    if (!next)
      goto done;
    bk_scene_destroy(scene);
    scene = next;
    next = NULL;
    if (!capture(r, scene, pixels, error))
      goto done;
    if (i == 0 && !save(prefix, "title", pixels, error))
      goto done;
    next = bk_scene_create(BK_SCENE_STATIC_WORLD, &services, error);
    if (!next)
      goto done;
    bk_scene_destroy(scene);
    scene = next;
    next = NULL;
    if (!capture(r, scene, pixels, error))
      goto done;
    if (memcmp(pixels, baseline, BYTES)) {
      snprintf(error, 256, "scene reload differs");
      goto done;
    }
  }
  /* A missing camera pack must fail without damaging the live office. */
  next = bk_scene_create(BK_SCENE_CAMERA_TRACK, &services, error);
  if (next) {
    snprintf(error, 256, "missing camera pack accepted");
    goto done;
  }
  if (!capture(r, scene, pixels, error) || memcmp(pixels, baseline, BYTES))
    goto done;
  char camera_path[1024];
  if (snprintf(camera_path, sizeof(camera_path), "%s/Data/bk3_04.pp", root) >=
          (int)sizeof(camera_path) ||
      !bk_resources_mount(store, "bk3_04", camera_path, error))
    goto done;
  for (unsigned cycle = 0; cycle < 3; cycle++) {
    next = bk_scene_create(BK_SCENE_CAMERA_TRACK, &services, error);
    if (!next)
      goto done;
    bk_scene_destroy(scene);
    scene = next;
    next = NULL;
    if (!capture(r, scene, baseline, error))
      goto done;
    if (cycle == 0 && !save(prefix, "track-start", baseline, error))
      goto done;
    BkInput idle = {0};
    for (unsigned i = 0; i < 60; i++)
      if (!bk_scene_step(scene, 1.0 / 60, &idle, error))
        goto done;
    if (!capture(r, scene, pixels, error) || !memcmp(pixels, baseline, BYTES)) {
      snprintf(error, 256, "XAN camera track did not advance");
      goto done;
    }
    if (cycle == 0 && !save(prefix, "clip-60-steps", pixels, error))
      goto done;
    if (!bk_scene_step(scene, 1.0 / 60, &reset, error) ||
        !capture(r, scene, pixels, error) || memcmp(pixels, baseline, BYTES)) {
      snprintf(error, 256, "XAN camera reset mismatch");
      goto done;
    }
    for (unsigned i = 0; i < 800; i++)
      if (!bk_scene_step(scene, 1.0 / 60, &idle, error))
        goto done;
    if (!capture(r, scene, pixels, error) || !memcmp(pixels, baseline, BYTES)) {
      snprintf(error, 256, "XAN clip endpoint did not move");
      goto done;
    }
    memcpy(baseline, pixels, BYTES);
    if (cycle == 0 && !save(prefix, "clip-end", pixels, error))
      goto done;
    for (unsigned i = 0; i < 60; i++)
      if (!bk_scene_step(scene, 1.0 / 60, &idle, error))
        goto done;
    if (!capture(r, scene, pixels, error) || memcmp(pixels, baseline, BYTES)) {
      snprintf(error, 256, "once-only XAN clip did not hold its endpoint");
      goto done;
    }
    next = bk_scene_create(BK_SCENE_STATIC_WORLD, &services, error);
    if (!next)
      goto done;
    bk_scene_destroy(scene);
    scene = next;
    next = NULL;
    if (!capture(r, scene, baseline, error))
      goto done;
  }
  fprintf(stderr, "PASS: XAN camera 60-step movement, reset, "
                  "400-tick end and endpoint hold; 3 clip/office "
                  "cycles; missing camera pack failure\n");
  next = bk_scene_create(BK_SCENE_ACTOR_PREVIEW, &services, error);
  if (next) {
    snprintf(error, 256, "missing actor resources accepted");
    goto done;
  }
  if (!capture(r, scene, pixels, error) || memcmp(pixels, baseline, BYTES))
    goto done;
  char actor_path[1024], data_path[1024];
  if (snprintf(actor_path, sizeof(actor_path), "%s/Data/bk3_01.pp", root) >=
          (int)sizeof(actor_path) ||
      snprintf(data_path, sizeof(data_path), "%s/Data", root) >=
          (int)sizeof(data_path) ||
      !bk_resources_mount(store, "bk3_01", actor_path, error) ||
      !bk_resources_mount_directory(store, "routes", data_path, 20480, error) ||
      !bk_resources_mount_directory(store, "faces", data_path, 20480, error))
    goto done;
  for (unsigned cycle = 0; cycle < 3; cycle++) {
    next = bk_scene_create(BK_SCENE_ACTOR_PREVIEW, &services, error);
    if (!next)
      goto done;
    bk_scene_destroy(scene);
    scene = next;
    next = NULL;
    if (!capture(r, scene, baseline, error))
      goto done;
    if (!cycle && !save(prefix, "actor-start", baseline, error))
      goto done;
    BkInput idle = {0};
    for (unsigned i = 0; i < 120; i++) {
      if (!bk_scene_step(scene, 1.0 / 60, &idle, error))
        goto done;
      /* Submit between updates to exercise CPU skin and mapped GPU waits. */
      if (i % 10 == 0 && !capture(r, scene, pixels, error))
        goto done;
    }
    if (!capture(r, scene, pixels, error) || !memcmp(pixels, baseline, BYTES)) {
      snprintf(error, 256, "actor scene animation did not change image");
      goto done;
    }
    if (!cycle && !save(prefix, "actor-moving", pixels, error))
      goto done;
    memcpy(baseline, pixels, BYTES);
    if (!capture(r, scene, pixels, error) || memcmp(pixels, baseline, BYTES)) {
      snprintf(error, 256, "actor draw advanced simulation without step");
      goto done;
    }
    next = bk_scene_create(BK_SCENE_STATIC_WORLD, &services, error);
    if (!next)
      goto done;
    bk_scene_destroy(scene);
    scene = next;
    next = NULL;
    if (!capture(r, scene, baseline, error))
      goto done;
  }
  fprintf(stderr, "PASS: actor scene120-step animation, draw-only stability, "
                  "3 actor/office cycles, missing resources failure\n");
  empty = bk_resources_create(error);
  if (!empty)
    goto done;
  services.resources = empty;
  next = bk_scene_create(BK_SCENE_STATIC_WORLD, &services, error);
  if (next) {
    snprintf(error, 256, "missing scene data accepted");
    goto done;
  }
  if (!capture(r, scene, pixels, error) || memcmp(pixels, baseline, BYTES))
    goto done;
  fprintf(stderr,
          "PASS: original office 16 identical frames, camera movement/reset, 3 "
          "title/office cycles, missing-data failure\n");
  ok = 1;
done:
  bk_scene_destroy(next);
  bk_scene_destroy(scene);
  bk_resources_destroy(empty);
  bk_resources_destroy(store);
  free(baseline);
  return ok;
}
int main(int argc, char **argv) {
  if (argc > 3) {
    fprintf(stderr, "usage: scene-probe [game-root [capture-prefix]]\n");
    return 2;
  }
  char error[256] = {0};
  BkRenderer *r = bk_renderer_create(WIDTH, HEIGHT, stderr, error);
  uint8_t *pixels = malloc(BYTES);
  int ok = r && pixels && geometry(r, pixels, error) &&
           (argc == 1 ||
            originals(r, argv[1], argc == 3 ? argv[2] : NULL, pixels, error));
  free(pixels);
  bk_renderer_destroy(r);
  if (!ok)
    fprintf(stderr, "scene probe FAILED: %s\n", error);
  return ok ? 0 : 1;
}
