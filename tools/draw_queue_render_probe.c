/* Synthetic cross-model overlap through the production actor upload/batch
 * path. Pixel expectations are independent alpha compositions, not the
 * implementation's sorted indices. No game assets or Windows raster claims. */
#include "core/matrix.h"
#include "scene/actor_render.h"
#include "world/actor_forest.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#define SIZE 64
#define ACTORS 5
#define REQUIRE(x)                                                             \
  do {                                                                         \
    if (!(x)) {                                                                \
      fprintf(stderr, "FAIL line%d: %s\n", __LINE__, error);                   \
      goto done;                                                               \
    }                                                                          \
  } while (0)
typedef struct {
  BkModel model;
  BkModelFrame frame[2];
  BkModelMesh mesh;
  BkModelSubmesh sub;
  BkModelMaterial material;
  BkModelTexture texture;
  BkModelVertex vertices[4];
  uint16_t indices[12];
  uint8_t header[332];
} Fixture;
static void fixture(Fixture *f, unsigned color, unsigned priority) {
  memset(f, 0, sizeof(*f));
  f->model = (BkModel){.source = f->header,
                       .source_size = sizeof(f->header),
                       .frames = f->frame,
                       .frame_count = 1,
                       .meshes = &f->mesh,
                       .mesh_count = 1,
                       .submeshes = &f->sub,
                       .submesh_count = 1,
                       .materials = &f->material,
                       .material_count = 1,
                       .textures = &f->texture,
                       .texture_count = color != 0};
  f->frame[0].id = 1;
  f->frame[0].parent_index = BK_MODEL_NONE;
  f->frame[0].mesh_index = 0;
  memcpy(f->frame[0].local, bk_identity, 64);
  f->mesh.submesh_count = 1;
  f->sub = (BkModelSubmesh){.vertex_count = 4,
                            .index_count = 12,
                            .vertices = f->vertices,
                            .indices = f->indices,
                            .texture_count = color != 0};
  f->material.diffuse[3] = color ? .5f : 1;
  if (color)
    f->material.emissive[color == 1 ? 0 : 2] = 1;
  strcpy(f->texture.filename, "white.bmp");
  f->header[76] = (uint8_t)priority;
  float xy[4][2] = {{-.8f, -.8f}, {.8f, -.8f}, {.8f, .8f}, {-.8f, .8f}};
  for (unsigned i = 0; i < 4; ++i) {
    memcpy(f->vertices[i].position, xy[i], 8);
    f->vertices[i].normal[2] = -1;
  }
  uint16_t indices[] = {0, 1, 2, 0, 2, 3, 2, 1, 0, 3, 2, 0};
  memcpy(f->indices, indices, sizeof(indices));
}
static int pixel(const uint8_t *p, const int expected[3]) {
  for (unsigned y = 20; y < 44; ++y)
    for (unsigned x = 20; x < 44; ++x)
      for (unsigned c = 0; c < 3; ++c)
        if (abs(p[(y * SIZE + x) * 4 + c] - expected[c]) > 1)
          return 0;
  return 1;
}
int main(void) {
  char error[256] = {0}, dir[] = "/tmp/biko3-draw-queue-XXXXXX",
       path[256] = {0};
  BkResourceStore *store = NULL;
  BkRenderer *r = NULL;
  BkLightSet *lights = NULL;
  BkActorRender *actors[ACTORS] = {0};
  BkActorPose *poses[ACTORS] = {0};
  BkActorRenderBatch *batch = NULL, *small = NULL;
  BkActorForest *forest = NULL;
  BkMaterialPose *edited = NULL;
  BkClipSet *clips = NULL;
  Fixture f[ACTORS];
  uint8_t pixels[SIZE * SIZE * 4], xan[0x5190] = {0};
  int rc = 1;
  unsigned checks = 0;
  REQUIRE(mkdtemp(dir));
  snprintf(path, sizeof(path), "%s/white.bmp", dir);
  uint8_t bmp[58] = {0x42, 0x4d, 58, 0, 0, 0, 0, 0, 0, 0, 54, 0, 0, 0, 40,
                     0,    0,    0,  1, 0, 0, 0, 1, 0, 0, 0,  1, 0, 24};
  bmp[54] = bmp[55] = bmp[56] = 255;
  FILE *out = fopen(path, "wb");
  REQUIRE(out);
  size_t written = fwrite(bmp, 1, sizeof(bmp), out);
  int closed = fclose(out);
  REQUIRE(written == sizeof(bmp) && !closed);
  store = bk_resources_create(error);
  REQUIRE(store);
  REQUIRE(bk_resources_mount_directory(store, "fixture", dir, 128, error));
  memcpy(xan, "quad.x", 7);
  memcpy(xan + 256, "quad.x", 7);
  clips = bk_clip_set_decode(xan, sizeof(xan), error);
  REQUIRE(clips);
  r = bk_renderer_create(SIZE, SIZE, stderr, error);
  REQUIRE(r);
  lights = bk_light_set_create(r, &(BkLighting){0}, error);
  REQUIRE(lights);
  batch = bk_actor_render_batch_create(r, 8, error);
  small = bk_actor_render_batch_create(r, 2, error);
  REQUIRE(batch && small);
  REQUIRE(!bk_actor_render_batch_create(r, 0, error));
  for (unsigned i = 0; i < ACTORS; ++i) {
    fixture(&f[i], i == 0 ? 0 : i == 2 ? 2 : 1, i == 3);
    if (i == 4) {
      f[i].model.frame_count = 2;
      f[i].frame[1] = f[i].frame[0];
      f[i].frame[1].id = 2;
      f[i].frame[1].parent_index = 0;
      f[i].frame[1].local[14] = .1f;
    }
    float position[3] = {0, 0, i == 0 ? .95f : i == 2 ? .8f : .2f};
    poses[i] =
        bk_actor_pose_create_loaded(&f[i].model, clips, 0, position, 0, error);
    REQUIRE(poses[i]);
    bk_actor_pose_publish(poses[i]);
    actors[i] =
        bk_actor_render_create(r, store, "fixture", &f[i].model, NULL, error);
    REQUIRE(actors[i]);
    REQUIRE(bk_actor_render_prepare(actors[i], poses[i], NULL, NULL,
                                    bk_identity, bk_identity, error));
    BkActorRenderStats cached;
    REQUIRE(bk_actor_render_stats(actors[i], &cached));
    REQUIRE(cached.uploaded_vertices == 0);
  }
  /* Rigid buffers survive camera/root changes; a material edit and its
   * removal must each replace them exactly once, with independent pixels. */
  edited = bk_material_pose_create(&f[1].model, error);
  REQUIRE(edited);
  REQUIRE(bk_material_pose_alpha(
      edited, &(BkMaterialAlphaEdit){0, .25f, NULL, 0}, 1, error));
  for (unsigned stage = 0; stage < 4; ++stage) {
    REQUIRE(bk_actor_pose_place(
        poses[1], (float[3]){stage == 1 ? .05f : 0, 0, .2f}, 0, error));
    bk_actor_pose_publish(poses[1]);
    REQUIRE(bk_actor_render_prepare(actors[1], poses[1],
                                    stage < 2 ? edited : NULL, NULL,
                                    bk_identity, bk_identity, error));
    BkActorRenderStats cached;
    REQUIRE(bk_actor_render_stats(actors[1], &cached));
    REQUIRE(cached.uploaded_vertices == (stage % 2 ? 0 : 4));
    REQUIRE(bk_renderer_begin(r, error));
    REQUIRE(bk_actor_render_draw(actors[1], lights, error));
    REQUIRE(bk_renderer_end(r, error));
    REQUIRE(bk_renderer_readback(r, pixels, sizeof(pixels), error));
    REQUIRE(pixel(pixels, (int[]){stage < 2 ? 64 : 128, 0, 0}));
    checks += 24 * 24;
  }
  /* Cases: mixed queue, all-transparent bypass, priority overrides distance,
   * reverse model insertion, duplicate occurrence, hidden actor. */
  const unsigned sources[][4] = {{0, 1, 2}, {1, 2},       {0, 3, 2},
                                 {2, 0, 1}, {0, 1, 2, 1}, {0, 1, 2}};
  const unsigned counts[] = {3, 2, 3, 3, 4, 3};
  const int expected[][3] = {{128, 0, 64}, {64, 0, 128}, {64, 0, 128},
                             {128, 0, 64}, {192, 0, 32}, {128, 0, 0}};
  const unsigned ordered[][4] = {{0, 2, 1}, {0, 1},       {0, 1, 2},
                                 {1, 0, 2}, {0, 2, 1, 3}, {0, 1}};
  for (unsigned s = 0; s < 6; ++s) {
    if (s == 5) {
      BkActorVisibilityEdit hide = {0, 1};
      REQUIRE(bk_actor_pose_visibility(poses[2], &hide, 1, error));
      REQUIRE(bk_actor_render_prepare(actors[2], poses[2], NULL, NULL,
                                      bk_identity, bk_identity, error));
    }
    BkActorRender *list[4];
    for (unsigned i = 0; i < counts[s]; ++i)
      list[i] = actors[sources[s][i]];
    REQUIRE(bk_actor_render_batch_prepare(batch, list, counts[s], error));
    unsigned total = counts[s] - (s == 5);
    REQUIRE(bk_actor_render_batch_count(batch) == total);
    for (unsigned i = 0; i < total; ++i) {
      uint32_t source, frame, sub;
      REQUIRE(bk_actor_render_batch_item(batch, i, &source, &frame, &sub));
      REQUIRE(source == ordered[s][i] && frame == 0 && sub == 0);
    }
    REQUIRE(bk_renderer_begin(r, error));
    REQUIRE(bk_actor_render_batch_draw(batch, lights, error));
    REQUIRE(bk_renderer_end(r, error));
    REQUIRE(bk_renderer_readback(r, pixels, sizeof(pixels), error));
    if (!pixel(pixels, expected[s])) {
      snprintf(error, 256, "case%u center %u,%u,%u wanted %d,%d,%d", s,
               pixels[(32 * SIZE + 32) * 4], pixels[(32 * SIZE + 32) * 4 + 1],
               pixels[(32 * SIZE + 32) * 4 + 2], expected[s][0], expected[s][1],
               expected[s][2]);
      goto done;
    }
    checks += 24 * 24;
    BkActorRenderVisit visits[4];
    for (unsigned i = 0; i < counts[s]; ++i)
      visits[i] = (BkActorRenderVisit){list[i], 0};
    REQUIRE(
        bk_actor_render_batch_prepare_visits(batch, visits, counts[s], error));
    REQUIRE(bk_actor_render_batch_count(batch) == total);
    REQUIRE(bk_renderer_begin(r, error));
    REQUIRE(bk_actor_render_batch_draw(batch, lights, error));
    REQUIRE(bk_renderer_end(r, error));
    REQUIRE(bk_renderer_readback(r, pixels, sizeof(pixels), error));
    REQUIRE(pixel(pixels, expected[s]));
    checks += 24 * 24;
  }
  for (unsigned mode = 0; mode < 4; ++mode) {
    BkActorRenderVisit visits[2] = {{actors[4], mode == 0 ? 0 : 1},
                                    {actors[4], 1}};
    unsigned total = mode < 2 ? 1 : 2;
    if (mode == 3)
      visits[0].frame = 0;
    REQUIRE(bk_actor_render_batch_prepare_visits(batch, visits, total, error));
    REQUIRE(bk_actor_render_batch_count(batch) == total);
    for (unsigned i = 0; i < total; ++i) {
      uint32_t source, frame, sub;
      REQUIRE(bk_actor_render_batch_item(batch, i, &source, &frame, &sub));
      REQUIRE(source == i && frame == visits[i].frame && sub == 0);
    }
    REQUIRE(bk_renderer_begin(r, error));
    REQUIRE(bk_actor_render_batch_draw(batch, lights, error));
    REQUIRE(bk_renderer_end(r, error));
    REQUIRE(bk_renderer_readback(r, pixels, sizeof(pixels), error));
    REQUIRE(pixel(pixels, (int[]){total == 1 ? 128 : 192, 0, 0}));
    checks += 24 * 24;
  }
  REQUIRE(!bk_actor_render_batch_prepare_visits(
      batch, &(BkActorRenderVisit){actors[4], 2}, 1, error));
  REQUIRE(bk_actor_render_batch_count(batch) == 0);
  BkActorRender *list[] = {actors[0], actors[1], actors[3]};
  REQUIRE(!bk_actor_render_batch_prepare(small, list, 3, error));
  REQUIRE(bk_actor_render_batch_count(small) == 0);
  REQUIRE(bk_actor_render_batch_prepare(batch, list, 3, error));
  REQUIRE(bk_actor_render_prepare(actors[3], poses[3], NULL, NULL, bk_identity,
                                  bk_identity, error));
  REQUIRE(bk_actor_render_batch_count(batch) == 0);
  REQUIRE(bk_renderer_begin(r, error));
  REQUIRE(!bk_actor_render_batch_draw(batch, lights, error));
  REQUIRE(!bk_actor_render_batch_draw(small, lights, error));
  REQUIRE(bk_renderer_end(r, error));
  REQUIRE(bk_renderer_readback(r, pixels, sizeof(pixels), error));
  REQUIRE(pixel(pixels, (int[]){0, 0, 0}));
  REQUIRE(bk_actor_render_batch_prepare(batch, NULL, 0, error));
  REQUIRE(bk_actor_render_batch_count(batch) == 0);
  REQUIRE(bk_renderer_begin(r, error));
  REQUIRE(bk_actor_render_batch_draw(batch, lights, error));
  REQUIRE(bk_renderer_end(r, error));
  forest = bk_actor_forest_create(poses, ACTORS, error);
  REQUIRE(forest);
  for (unsigned i = 0; i < ACTORS; ++i)
    REQUIRE(bk_actor_forest_attach(forest, 0,
                                   bk_actor_forest_node(forest, i, 0), error));
  BkActorVisibilityEdit hidden_parent[] = {{0, 1}, {1, 0}};
  REQUIRE(bk_actor_pose_visibility(poses[4], hidden_parent, 2, error));
  uint32_t moved_child = bk_actor_forest_node(forest, 4, 1);
  REQUIRE(bk_actor_forest_attach(forest, 0, moved_child, error));
  const BkFrameVisit *walk;
  uint32_t walked;
  REQUIRE(bk_actor_forest_draw(forest, moved_child, &walk, &walked, error));
  REQUIRE(bk_actor_render_prepare(actors[4], poses[4], NULL, NULL,
                                  bk_actor_forest_view(forest), bk_identity,
                                  error));
  BkActorRenderStats legacy;
  REQUIRE(bk_actor_render_stats(actors[4], &legacy));
  REQUIRE(legacy.submitted_instances == 0);
  BkActorRenderVisit submits[8];
  unsigned submitted = 0;
  for (uint32_t i = 0; i < walked; ++i)
    if (walk[i].submit) {
      uint32_t actor, frame;
      if (bk_actor_forest_binding(forest, walk[i].node, &actor, &frame))
        submits[submitted++] = (BkActorRenderVisit){actors[actor], frame};
    }
  REQUIRE(submitted == 1);
  REQUIRE(
      bk_actor_render_batch_prepare_visits(batch, submits, submitted, error));
  REQUIRE(bk_actor_render_batch_count(batch) == 1);
  REQUIRE(bk_renderer_begin(r, error));
  REQUIRE(bk_actor_render_batch_draw(batch, lights, error));
  REQUIRE(bk_renderer_end(r, error));
  REQUIRE(bk_renderer_readback(r, pixels, sizeof(pixels), error));
  REQUIRE(pixel(pixels, (int[]){128, 0, 0}));
  checks += 24 * 24;
  printf("PASS global actor queues: 17 model/frame/forest overlap cases, %u "
         "independently "
         "predicted pixels, native ordering, hidden/duplicate actors, "
         "capacity/epoch preflight, empty batch\n",
         checks);
  rc = 0;
done:
  bk_material_pose_destroy(edited);
  if (rc)
    fprintf(stderr, "FAIL global actor queues: %s\n", error);
  bk_actor_forest_destroy(forest);
  bk_actor_render_batch_destroy(batch);
  bk_actor_render_batch_destroy(small);
  for (unsigned i = 0; i < ACTORS; ++i) {
    bk_actor_render_destroy(actors[i]);
    bk_actor_pose_destroy(poses[i]);
  }
  bk_light_set_destroy(r, lights);
  bk_renderer_destroy(r);
  bk_clip_set_destroy(clips);
  bk_resources_destroy(store);
  if (path[0])
    unlink(path);
  rmdir(dir);
  return rc;
}
