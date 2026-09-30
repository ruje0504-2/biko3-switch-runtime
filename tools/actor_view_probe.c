/* Synthetic colored quads only: independent view snapshots, retained shared
 * textures, original sort identities, and viewport/depth preservation. */
#include "scene/actor_render.h"
#include "world/actor_forest.h"
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#define W 32
#define H 24
#define CHECK(x)                                                               \
  do {                                                                         \
    if (!(x)) {                                                                \
      fprintf(stderr, "line%d: %s: %s\n", __LINE__, #x, e);                    \
      goto done;                                                               \
    }                                                                          \
  } while (0)
static unsigned channels, frames, max_error;
static int pixels(const uint8_t *p, float red, float green, int split) {
  for (unsigned y = 2; y < H - 2; y++)
    for (unsigned x = 2; x < W - 2; x++)
      for (unsigned c = 0; c < 3; c++) {
        float expected =
            split && x >= W / 2 ? (c == 1 ? green : 0) : (c == 0 ? red : 0);
        int d = abs((int)p[(y * W + x) * 4 + c] - (int)lroundf(expected * 255));
        if ((unsigned)d > max_error)
          max_error = d;
        if (d > 1)
          return 0;
        channels++;
      }
  return 1;
}
int main(int argc, char **argv) {
  if (argc != 2)
    return 2;
  char e[256] = {0}, path[1024];
  int rc = 1;
  BkRenderer *r = NULL;
  BkResourceStore *store = NULL;
  BkClipSet *clips = NULL;
  BkActorPose *pose = NULL;
  BkActorForest *forest = NULL;
  BkMaterialPose *materials = NULL;
  BkLightSet *light = NULL;
  BkActorRender *a[3] = {0};
  BkActorRenderBatch *batch[3] = {0};
  FILE *f = NULL;
  uint8_t rgba[W * H * 4];
  CHECK(!mkdir(argv[1], 0700) || errno == EEXIST);
  snprintf(path, sizeof(path), "%s/view.bmp", argv[1]);
  uint8_t bmp[58] = {'B', 'M', 58, 0, 0, 0, 0, 0, 0, 0, 54, 0, 0, 0,  40,
                     0,   0,   0,  1, 0, 0, 0, 1, 0, 0, 0,  1, 0, 24, 0};
  bmp[54] = bmp[55] = bmp[56] = 255;
  f = fopen(path, "wb");
  CHECK(f);
  CHECK(fwrite(bmp, 1, sizeof(bmp), f) == sizeof(bmp));
  CHECK(!fclose(f));
  f = NULL;
  store = bk_resources_create(e);
  CHECK(store &&
        bk_resources_mount_directory(store, "fixture", argv[1], 1024, e));
  uint8_t xan[0x5190] = {0}, header[332] = {0};
  strcpy((char *)xan, "fixture.x");
  strcpy((char *)xan + 256, "fixture.x");
  clips = bk_clip_set_decode(xan, sizeof(xan), e);
  CHECK(clips);
  uint16_t indices[] = {0, 1, 2, 0, 2, 3};
  BkModelVertex vertices[4] = {{.position = {-1, -1, .2f}, .normal = {0, 0, 1}},
                               {.position = {1, -1, .2f}, .normal = {0, 0, 1}},
                               {.position = {1, 1, .2f}, .normal = {0, 0, 1}},
                               {.position = {-1, 1, .2f}, .normal = {0, 0, 1}}};
  BkModelFrame frame = {
      .id = 1, .parent_index = BK_MODEL_NONE, .mesh_index = 0};
  memcpy(frame.local, bk_identity, 64);
  BkModelMesh mesh = {.id = 2, .submesh_count = 1};
  BkModelMaterial material = {
      .id = 4, .diffuse = {1, 1, 1, 1}, .emissive = {1, 0, 0, 0}};
  BkModelTexture texture = {.id = 5, .filename = "view.bmp"};
  BkModelSubmesh sub = {.id = 3,
                        .material_id = 4,
                        .material_index = 0,
                        .texture_count = 1,
                        .texture_ids = {5},
                        .texture_indices = {0},
                        .vertex_count = 4,
                        .index_count = 6,
                        .vertices = vertices,
                        .indices = indices};
  BkModel model = {.source = header,
                   .source_size = sizeof(header),
                   .frames = &frame,
                   .frame_count = 1,
                   .meshes = &mesh,
                   .mesh_count = 1,
                   .submeshes = &sub,
                   .submesh_count = 1,
                   .materials = &material,
                   .material_count = 1,
                   .textures = &texture,
                   .texture_count = 1,
                   .vertex_count = 4,
                   .triangle_count = 2};
  pose = bk_actor_pose_create_loaded(&model, clips, 0, (float[3]){0}, 0, e);
  materials = bk_material_pose_create(&model, e);
  CHECK(pose && materials);
  forest = bk_actor_forest_create(&pose, 1, e);
  CHECK(forest && bk_actor_forest_attach(forest, 0, 2, e));
  r = bk_renderer_create(W, H, stderr, e);
  CHECK(r);
  light = bk_light_set_create(r, &(BkLighting){0}, e);
  CHECK(light);
  BkRenderStats empty = bk_renderer_stats(r);
  for (unsigned iteration = 0; iteration < 4; iteration++) {
    a[0] = bk_actor_render_create(r, store, "fixture", &model, NULL, e);
    CHECK(a[0]);
    for (unsigned i = 1; i < 3; i++) {
      BkRenderStats before = bk_renderer_stats(r);
      a[i] = bk_actor_render_create_view(a[i - 1], e);
      CHECK(a[i]);
      /* Exactly one vertex and one index allocation; no new image/texture or
       * staging allocations. All three views retain the same texture set. */
      CHECK(bk_renderer_stats(r).live_allocations - before.live_allocations ==
            2);
    }
    for (unsigned i = 0; i < 3; i++) {
      batch[i] = bk_actor_render_batch_create(r, 1, e);
      CHECK(batch[i]);
    }
    BkRenderStats baseline = bk_renderer_stats(r);
    for (unsigned step = 0; step < 20; step++) {
      float level = .25f + step * .025f;
      for (unsigned i = 0; i < 3; i++) {
        float transform[16];
        memcpy(transform, bk_identity, 64);
        transform[14] = i * .3f;
        CHECK(bk_actor_pose_root_local(pose, transform, e));
        BkMaterialValuesEdit edit = {
            .index = 0, .id = 4, .values = {.diffuse = {1, 1, 1, 1}}};
        edit.values.emissive[i] = level;
        CHECK(bk_material_pose_values(materials, &edit, 1, e));
        CHECK(bk_actor_render_prepare(a[i], pose, materials, NULL, bk_identity,
                                      bk_identity, e));
        CHECK(bk_actor_render_batch_prepare(batch[i], a + i, 1, e));
      }
      CHECK(bk_renderer_begin(r, e));
      CHECK(bk_actor_render_batch_draw(batch[0], light, e));
      CHECK(bk_actor_render_batch_draw(batch[1], light, e));
      CHECK(bk_renderer_capture(r, rgba, sizeof(rgba), e));
      CHECK(pixels(rgba, level, level, 0));
      BkViewport right = {W / 2, 0, W / 2, H};
      CHECK(bk_renderer_viewport(r, &right, e));
      CHECK(bk_renderer_clear_depth(r, 1, e));
      CHECK(bk_actor_render_batch_draw(batch[1], light, e));
      CHECK(bk_renderer_viewport(r, NULL, e));
      CHECK(bk_actor_render_batch_draw(batch[2], light, e));
      CHECK(bk_renderer_end(r, e) &&
            bk_renderer_readback(r, rgba, sizeof(rgba), e));
      CHECK(pixels(rgba, level, level, 1));
      frames++;
      BkRenderStats now = bk_renderer_stats(r);
      CHECK(now.live_allocations == baseline.live_allocations &&
            now.live_bytes == baseline.live_bytes);
    }
    /*423b01 suppresses both queue paths, without changing a previously
     * captured visible view. Clearing CPU flags cannot revive an old hidden
     * GPU snapshot. Publication is covered independently by the native VM. */
    CHECK(bk_actor_forest_draw_disable(forest, 2, 255, e));
    uint32_t hidden = 99;
    CHECK(bk_actor_pose_hidden(pose, 0, &hidden) && hidden == 0);
    CHECK(bk_actor_render_prepare(a[2], pose, materials, NULL, bk_identity,
                                 bk_identity, e));
    CHECK(bk_actor_render_batch_prepare(batch[2], a + 2, 1, e) &&
          bk_actor_render_batch_count(batch[2]) == 0);
    BkActorRenderVisit visit = {a[2], 0};
    CHECK(bk_actor_render_batch_prepare_visits(batch[2], &visit, 1, e) &&
          bk_actor_render_batch_count(batch[2]) == 0);
    CHECK(bk_renderer_begin(r, e) &&
          bk_actor_render_batch_draw(batch[0], light, e) &&
          bk_actor_render_batch_draw(batch[2], light, e) &&
          bk_renderer_end(r, e) &&
          bk_renderer_readback(r, rgba, sizeof(rgba), e));
    CHECK(pixels(rgba, .725f, 0, 0));
    frames++;
    CHECK(bk_actor_forest_draw_disable(forest, 2, 0, e));
    CHECK(bk_renderer_begin(r, e) &&
          bk_actor_render_batch_draw(batch[2], light, e) &&
          bk_renderer_end(r, e) &&
          bk_renderer_readback(r, rgba, sizeof(rgba), e));
    CHECK(pixels(rgba, 0, 0, 0));
    frames++;
    CHECK(bk_actor_render_prepare(a[2], pose, materials, NULL, bk_identity,
                                 bk_identity, e));
    CHECK(bk_actor_render_batch_prepare_visits(batch[2], &visit, 1, e) &&
          bk_actor_render_batch_count(batch[2]) == 1);
    CHECK(bk_actor_render_batch_prepare(batch[2], a + 2, 1, e) &&
          bk_actor_render_batch_count(batch[2]) == 1);
    /* Both owner-first and clone-first retirement; last view still samples
     * the retained texture and retains its prepared color/matrix. */
    unsigned survivor = iteration % 2 ? 0 : 2;
    for (unsigned i = 0; i < 3; i++)
      if (i != survivor) {
        bk_actor_render_batch_destroy(batch[i]);
        batch[i] = NULL;
        bk_actor_render_destroy(a[i]);
        a[i] = NULL;
      }
    CHECK(bk_renderer_begin(r, e) &&
          bk_actor_render_batch_draw(batch[survivor], light, e) &&
          bk_renderer_end(r, e));
    CHECK(bk_renderer_readback(r, rgba, sizeof(rgba), e));
    for (unsigned c = 0; c < 3; c++)
      CHECK(abs((int)rgba[(H / 2 * W + W / 2) * 4 + c] -
                (c == survivor ? 185 : 0)) <= 1);
    for (unsigned i = 0; i < 3; i++) {
      bk_actor_render_batch_destroy(batch[i]);
      batch[i] = NULL;
      bk_actor_render_destroy(a[i]);
      a[i] = NULL;
    }
    CHECK(bk_renderer_stats(r).live_allocations == empty.live_allocations &&
          bk_renderer_stats(r).live_bytes == empty.live_bytes);
  }
  printf("PASS actor view GPU: %u frames %u channels max_error=%u; 8 "
         "texture-free clones, 4 retirement orders, 8 draw-disable snapshots; stable allocations\n",
         frames, channels, max_error);
  rc = 0;
done:
  if (f)
    fclose(f);
  for (unsigned i = 0; i < 3; i++) {
    bk_actor_render_batch_destroy(batch[i]);
    bk_actor_render_destroy(a[i]);
  }
  bk_light_set_destroy(r, light);
  bk_renderer_destroy(r);
  bk_actor_forest_destroy(forest);
  bk_actor_pose_destroy(pose);
  bk_material_pose_destroy(materials);
  bk_clip_set_destroy(clips);
  bk_resources_destroy(store);
  return rc;
}
