/* Real item GPU lifecycle with explicit inspection cameras and a recording
 * pickup service fixture (no sound/notice UI output or playable game flow). */
#include "core/matrix.h"
#include "scene/actor_render.h"
#include "scene/item_assets.h"
#include "world/actor_forest.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#define SIZE 192
#define CHECK(x)                                                               \
  do {                                                                         \
    if (!(x))                                                                  \
      goto done;                                                               \
  } while (0)
static int sound(void *ctx, char error[256]) {
  (void)error;
  ++*(unsigned *)ctx;
  return 1;
}
static int notice(void *ctx, uint32_t id, char error[256]) {
  assert(id < 40005);
  return sound(ctx, error);
}
static int camera_for(const BkActorPose *pose, float camera[16],
                      float projection[16]) {
  const BkModel *m = bk_actor_pose_model(pose);
  float lo[3] = {FLT_MAX, FLT_MAX, FLT_MAX},
        hi[3] = {-FLT_MAX, -FLT_MAX, -FLT_MAX};
  unsigned points = 0;
  for (uint32_t f = 0; f < m->frame_count; ++f) {
    uint32_t mi = m->frames[f].mesh_index;
    if (mi == BK_MODEL_NONE)
      continue;
    const BkModelMesh *mesh = &m->meshes[mi];
    for (uint32_t s = 0; s < mesh->submesh_count; ++s) {
      const BkModelSubmesh *sub = &m->submeshes[mesh->first_submesh + s];
      for (uint32_t v = 0; v < sub->vertex_count; ++v) {
        float p[4];
        bk_matrix_point(p, sub->vertices[v].position,
                        bk_actor_pose_frame(pose, f));
        if (!isfinite(p[3]) || !p[3])
          return 0;
        for (unsigned axis = 0; axis < 3; ++axis) {
          float x = p[axis] / p[3];
          if (!isfinite(x))
            return 0;
          lo[axis] = fminf(lo[axis], x);
          hi[axis] = fmaxf(hi[axis], x);
        }
        points++;
      }
    }
  }
  if (!points)
    return 0;
  float target[3], radius = 0;
  for (unsigned i = 0; i < 3; ++i) {
    target[i] = (hi[i] + lo[i]) * .5f;
    radius = fmaxf(radius, (hi[i] - lo[i]) * .5f);
  }
  if (radius <= 0)
    return 0;
  memcpy(camera, bk_identity, 64);
  memcpy(camera + 12, target, 12);
  camera[12] += radius * .2f;
  camera[13] += radius * .4f;
  camera[14] -= radius * 3.5f;
  return bk_camera_aim(camera, camera, target) &&
         bk_camera_projection(
             projection, &(BkCameraLens){1, 1, radius * .01f, radius * 100});
}
int main(int argc, char **argv) {
  if (argc != 2)
    return 2;
  char error[256] = {0}, path[1024];
  BkResourceStore *store = bk_resources_create(error);
  BkRenderer *renderer = NULL;
  BkLightSet *light = NULL;
  BkItemAssets *items = NULL;
  BkActorRender *actor = NULL;
  BkActorRenderBatch *batch = NULL;
  BkActorForest *forest = NULL;
  BkActorRenderVisit *submits = NULL;
  uint8_t *pixels = malloc(SIZE * SIZE * 4);
  unsigned profiles = 0, frames = 0, callbacks = 0;
  uint64_t colored = 0;
  int rc = 1;
  CHECK(store && pixels);
  snprintf(path, sizeof(path), "%s/bk3_16.pp", argv[1]);
  CHECK(bk_resources_mount(store, "bk3_16", path, error));
  renderer = bk_renderer_create(SIZE, SIZE, stderr, error);
  CHECK(renderer);
  light =
      bk_light_set_create(renderer, &(BkLighting){.ambient = {1, 1, 1}}, error);
  CHECK(light);
  batch = bk_actor_render_batch_create(renderer, 512, error);
  CHECK(batch);
  for (unsigned g = 0; g < 5; ++g)
    for (unsigned area = 0; area < 9; ++area) {
      BkItemState retained[BK_ITEM_LIMIT] = {0};
      items = bk_item_assets_create(store, g, area, (uint8_t[5]){0}, retained,
                                    error);
      CHECK(items);
      for (uint32_t i = 0; i < bk_item_assets_count(items); ++i) {
        BkActorPose *pose = bk_item_assets_bind_pose(items, i);
        const BkModel *model = bk_actor_pose_model(pose);
        actor = bk_actor_render_create(renderer, store, "bk3_16", model, NULL,
                                       error);
        CHECK(actor);
        forest = bk_actor_forest_create(&pose, 1, error);
        CHECK(forest);
        uint32_t root = BK_MODEL_NONE;
        for (uint32_t f = 0; f < model->frame_count; ++f)
          if (model->frames[f].parent_index == BK_MODEL_NONE)
            root = f;
        CHECK(bk_actor_forest_attach(
            forest, 0, bk_actor_forest_node(forest, 0, root), error));
        submits = calloc(model->frame_count, sizeof(*submits));
        CHECK(submits);
        BkItemPickupState inventory = {0};
        BkItemPickupOps ops = {&callbacks, sound, notice};
        for (unsigned frame = 0; frame < 4; ++frame) {
          if (frame == 3)
            CHECK(bk_item_assets_hidden(items, i, 0));
          CHECK(bk_item_assets_step(items, .016f, error));
          if (frame == 1) {
            const BkItemState *s = bk_item_assets_state(items, i);
            float current[3], previous[3];
            memcpy(current, s->position, 12);
            memcpy(previous, s->position, 12);
            current[0] -= 1;
            previous[0] += 1;
            BkItemPickups picked;
            CHECK(bk_item_assets_pickup(items, &inventory, current, previous,
                                        &ops, &picked, error));
            assert(picked.count == 1 && picked.slots[0] == i &&
                   inventory.collected[s->id] == 1);
            uint32_t hidden;
            assert(bk_actor_pose_hidden(pose, root, &hidden) && !hidden);
          }
          const BkFrameVisit *walk;
          uint32_t walked;
          CHECK(bk_actor_forest_draw(forest, 0, &walk, &walked, error));
          float camera[16], projection[16];
          CHECK(camera_for(pose, camera, projection));
          CHECK(bk_actor_forest_anchor(forest, 1, camera, 0, error));
          CHECK(bk_actor_forest_draw(forest, 0, &walk, &walked, error));
          CHECK(bk_light_set_view(renderer, light, camera + 12, error));
          CHECK(bk_actor_render_prepare(actor, pose, NULL, NULL,
                                        bk_actor_forest_view(forest),
                                        projection, error));
          uint32_t n = 0;
          for (uint32_t v = 0; v < walked; ++v) {
            uint32_t a, f;
            if (walk[v].submit &&
                bk_actor_forest_binding(forest, walk[v].node, &a, &f))
              submits[n++] = (BkActorRenderVisit){actor, f};
          }
          CHECK(bk_actor_render_batch_prepare_visits(batch, submits, n, error));
          CHECK(bk_renderer_begin(renderer, error));
          CHECK(bk_actor_render_batch_draw(batch, light, error));
          CHECK(bk_renderer_end(renderer, error));
          CHECK(bk_renderer_readback(renderer, pixels, SIZE * SIZE * 4, error));
          unsigned visible = 0;
          for (unsigned p = 0; p < SIZE * SIZE; ++p)
            visible += pixels[p * 4] || pixels[p * 4 + 1] || pixels[p * 4 + 2];
          if (frame == 2 ? visible != 0 : visible < 16) {
            snprintf(error, 256,
                     "item g%u/a%u/i%u/frame%u has %u colored pixels", g, area,
                     i, frame, visible);
            goto done;
          }
          colored += visible;
          frames++;
        }
        free(submits);
        submits = NULL;
        bk_actor_forest_destroy(forest);
        forest = NULL;
        bk_actor_render_destroy(actor);
        actor = NULL;
        profiles++;
        printf("PASS item g%u/a%u/i%u pickup-to-hide GPU lifecycle\n", g, area,
               i);
        fflush(stdout);
      }
      bk_item_assets_destroy(items);
      items = NULL;
    }
  printf(
      "PASS %u item GPU instances, %u frames, %llu colored pixels, %u recorded "
      "pickup service calls; next-display hiding, inspection cameras\n",
      profiles, frames, (unsigned long long)colored, callbacks);
  rc = 0;
done:
  if (rc)
    fprintf(stderr, "FAIL item GPU: %s\n", error);
  free(submits);
  bk_actor_forest_destroy(forest);
  bk_actor_render_destroy(actor);
  bk_item_assets_destroy(items);
  bk_actor_render_batch_destroy(batch);
  bk_light_set_destroy(renderer, light);
  bk_renderer_destroy(renderer);
  bk_resources_destroy(store);
  free(pixels);
  return rc;
}
