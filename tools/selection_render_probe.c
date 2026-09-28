#include "scene/selection_render.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define WIDTH 320
#define HEIGHT 240
#define BYTES (WIDTH * HEIGHT * 4)
#define CHECK(v)                                                               \
  do {                                                                         \
    if (!(v))                                                                  \
      goto done;                                                               \
  } while (0)
static uint64_t hash(uint64_t h, const uint8_t *p, size_t n) {
  while (n--)
    h = (h ^ *p++) * UINT64_C(1099511628211);
  return h;
}
typedef struct {
  BkSelectionRender *render;
  int32_t now;
} Movie;
static int movie_step(void *context, char error[256]) {
  Movie *m = context;
  return bk_selection_render_movie_step(m->render, m->now, m->now, error);
}
int main(int argc, char **argv) {
  if (argc != 2 && argc != 3)
    return 2;
  char error[256] = {0}, path[1024];
  BkResourceStore *store = bk_resources_create(error);
  BkRenderer *renderer = NULL;
  BkSelectionWorld *world = NULL;
  BkSelectionRender *current = NULL, *next = NULL;
  BkSelectionActorAssets *retired = NULL;
  uint8_t *pixels = malloc(BYTES), *previous = malloc(BYTES);
  unsigned frames = 0, replacements = 0;
  unsigned frozen_movie_frames = 0, movie_changed_pixels = 0,
           changed_groups = 0;
  uint64_t total = UINT64_C(1469598103934665603), instances = 0, colored = 0;
  int rc = 1;
  int movie = getenv("BK_SELECTION_MOVIE_TEST") != NULL;
  CHECK(store && pixels && previous);
  const char *packs[] = {"bk3_01", "bk3_03", "bk3_04", "bk3_06", "bk3_18"};
  for (unsigned i = 0; i < (movie ? 5u : 4u); ++i) {
    snprintf(path, sizeof(path), "%s/%s.pp", argv[1], packs[i]);
    CHECK(bk_resources_mount(store, packs[i], path, error));
  }
  CHECK(bk_resources_mount_directory(store, "faces", argv[1], 20480, error));
  renderer = bk_renderer_create(WIDTH, HEIGHT, stderr, error);
  CHECK(renderer);
  for (unsigned retained = 0; retained < 5; ++retained) {
    BkMenuCamera camera = {0};
    for (unsigned i = 0; i < 16; ++i)
      camera.pose.world[i] = camera.matrix[i] = i % 5 == 0;
    uint32_t rng = 12345, clocks[4] = {1000, 1001, 1002, 1003};
    BkFog fog = {0}; /* Explicit fixture: native entry fog reset is separate. */
    world = bk_selection_world_create(store, retained, (uint8_t)movie, .016f,
                                      &camera, clocks, &rng, error);
    CHECK(world);
    current =
        bk_selection_render_create_at(renderer, store, world, 1000, error);
    CHECK(current);
    for (unsigned group = 0; group < 5; ++group) {
      if (group) {
        CHECK(bk_selection_world_replace(world, group, (uint8_t)movie, clocks,
                                         &rng, &retired, error));
        next = bk_selection_render_create_at(
            renderer, store, world, (int32_t)(1100 + group * 24 * 50), error);
        CHECK(next);
        /* Last OLD GPU snapshot must survive CPU replacement. */
        CHECK(bk_renderer_begin(renderer, error));
        CHECK(bk_selection_render_draw(current, error));
        CHECK(bk_renderer_end(renderer, error));
        CHECK(bk_renderer_readback(renderer, pixels, BYTES, error));
        assert(!memcmp(pixels, previous, BYTES));
        assert(!bk_selection_render_prepare(current, &camera, &fog, error));
        bk_selection_render_destroy(current);
        current = next;
        next = NULL;
        bk_selection_actor_assets_destroy(retired);
        retired = NULL;
        ++replacements;
      }
      for (unsigned frame = 0; frame < 24; ++frame) {
        BkSelectionWorldInput in = {.selected = group,
                                    .camera_mode = (frame / 6) % 2,
                                    .seconds = .05f,
                                    .motion = {1.5f, -.25f},
                                    .buttons = frame % 4,
                                    .timestamp =
                                        1100 + (group * 24 + frame) * 50};
        for (unsigned i = 0; i < 3; ++i)
          in.face_clocks[i] = in.timestamp + i + 1;
        Movie context = {current, (int32_t)in.timestamp};
        BkSelectionWorldOps ops = {.context = &context,
                                   .movie_step = movie_step};
        CHECK(bk_selection_world_step(world, &in, movie ? &ops : NULL, &rng,
                                      error));
        if (movie && frame)
          assert(bk_selection_render_movie_frame(current) != UINT32_MAX);
        CHECK(bk_selection_render_prepare(current, &camera, &fog, error));
        for (unsigned i = 0; i < 2; ++i) {
          BkActorRenderStats stats;
          assert(bk_selection_render_stats(current, i, &stats));
          assert(stats.submitted_instances > 0);
          instances += stats.visible_instances;
        }
        CHECK(bk_renderer_begin(renderer, error));
        CHECK(bk_selection_render_draw(current, error));
        CHECK(bk_renderer_end(renderer, error));
        CHECK(bk_renderer_readback(renderer, pixels, BYTES, error));
        unsigned visible = 0;
        for (unsigned i = 0; i < WIDTH * HEIGHT; ++i)
          visible += pixels[i * 4] > 30 || pixels[i * 4 + 1] > 30 ||
                     pixels[i * 4 + 2] > 30;
        assert(visible > 200);
        colored += visible;
        total = hash(total, pixels, BYTES);
        memcpy(previous, pixels, BYTES);
        if (argc == 3 && retained == 0 && frame == 23) {
          snprintf(path, sizeof(path), "%s-%u.rgba", argv[2], group);
          FILE *file = fopen(path, "wb");
          assert(file && fwrite(pixels, 1, BYTES, file) == BYTES);
          assert(!fclose(file));
        }
        ++frames;
        if (movie && frame == 23) {
          /* Explicit diagnostic cameras: native face close-ups need not
           * include the movie material. Hold geometry/lights per pair. */
          for (unsigned side = 0; side < 4; side++) {
            const float positions[4][3] = {
                {0, 10, -12}, {12, 10, 0}, {0, 10, 12}, {-12, 10, 0}};
            BkMenuCamera inspection = camera;
            memcpy(inspection.pose.world, bk_identity, 64);
            memcpy(inspection.pose.world + 12, positions[side], 12);
            inspection.fov = 1.2f;
            CHECK(bk_camera_aim(inspection.pose.world, inspection.pose.world,
                                (float[3]){0, 10, 0}));
            CHECK(bk_actor_forest_anchor(bk_selection_world_forest(world), 1,
                                         inspection.pose.world, 0, error));
            CHECK(
                bk_selection_render_prepare(current, &inspection, &fog, error));
            CHECK(bk_renderer_begin(renderer, error));
            CHECK(bk_selection_render_draw(current, error));
            CHECK(bk_renderer_end(renderer, error));
            CHECK(bk_renderer_readback(renderer, previous, BYTES, error));
            CHECK(bk_selection_render_movie_step(
                current, (int32_t)in.timestamp + 137 + (int32_t)side * 137,
                (int32_t)in.timestamp + 137 + (int32_t)side * 137, error));
            CHECK(bk_renderer_begin(renderer, error));
            CHECK(bk_selection_render_draw(current, error));
            CHECK(bk_renderer_end(renderer, error));
            CHECK(bk_renderer_readback(renderer, pixels, BYTES, error));
            unsigned changed = 0;
            for (unsigned i = 0; i < WIDTH * HEIGHT; i++)
              changed += memcmp(pixels + 4 * i, previous + 4 * i, 4) != 0;
            movie_changed_pixels += changed;
            if (changed)
              changed_groups |= 1u << group;
            memcpy(previous, pixels, BYTES);
            frozen_movie_frames += 2;
          }
          CHECK(bk_actor_forest_anchor(bk_selection_world_forest(world), 1,
                                       camera.pose.world, 0, error));
          CHECK(bk_selection_render_prepare(current, &camera, &fog, error));
          CHECK(bk_renderer_begin(renderer, error));
          CHECK(bk_selection_render_draw(current, error));
          CHECK(bk_renderer_end(renderer, error));
          CHECK(bk_renderer_readback(renderer, previous, BYTES, error));
        }
      }
    }
    bk_selection_render_destroy(current);
    current = NULL;
    CHECK(
        bk_selection_world_replace(world, 4, 1, clocks, &rng, &retired, error));
    assert(!bk_selection_render_create(renderer, store, world, error));
    assert(strstr(error, "movie"));
    bk_selection_actor_assets_destroy(retired);
    retired = NULL;
    bk_selection_world_destroy(world);
    world = NULL;
  }
  assert(!movie || movie_changed_pixels > 0);
  printf("selection render PASS: 25 profiles, %u GPU frames, %u retained "
         "replacement snapshots, %llu instances, %llu colored pixels, "
         "RGBA hash %016llx movie%d frozen%u changed%u groups%u\n",
         frames, replacements, (unsigned long long)instances,
         (unsigned long long)colored, (unsigned long long)total, movie,
         frozen_movie_frames, movie_changed_pixels, changed_groups);
  rc = 0;
done:
  if (rc)
    fprintf(stderr, "%s\n", error);
  bk_selection_render_destroy(next);
  bk_selection_render_destroy(current);
  bk_selection_actor_assets_destroy(retired);
  bk_selection_world_destroy(world);
  bk_renderer_destroy(renderer);
  bk_resources_destroy(store);
  free(pixels);
  free(previous);
  return rc;
}
