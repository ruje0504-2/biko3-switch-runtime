/* Separate object origins plus camera rotation/translation and a3-second
 * decaying stop. Same strict front/back relation throughout. --legacy is an
 * expected-failure comparison with the old composed-MVP depth conversion. */
#include "core/matrix.h"
#include "render/renderer.h"
#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#define W 64
#define H 48
#define CHECK(x)                                                               \
  do {                                                                         \
    if (!(x)) {                                                                \
      fprintf(stderr, "depth transform line%d: %s (%s)\n", __LINE__, #x, e);   \
      goto done;                                                               \
    }                                                                          \
  } while (0)
int main(int argc, char **argv) {
  if (argc > 2 || (argc == 2 && strcmp(argv[1], "--legacy")))
    return 2;
  int legacy = argc == 2;
  char e[256] = {0};
  int rc = 1;
  BkRenderer *r = bk_renderer_create(W, H, stderr, e);
  BkTexture *texture = NULL;
  BkLightSet *lights = NULL;
  BkGpuMesh *meshes[2] = {0};
  uint8_t white[] = {255, 255, 255, 255}, pixels[W * H * 4];
  const uint16_t indices[] = {0, 1, 2, 0, 2, 3};
  const int xy[4][2] = {{-1, -1}, {1, -1}, {1, 1}, {-1, 1}};
  const float distances[] = {60, 100, 200, 1000};
  const float origin[2][3] = {{320, 30, 100}, {-77, 0, -290}};
  const float gaps[] = {.002f, .005f, .01f, .03f};
  BkDrawState draw = {BK_BLEND_OPAQUE, 1, BK_CULL_NONE};
  unsigned frames = 0, wrong = 0, changes = 0, brightness_errors = 0;
  unsigned phase_wrong[3] = {0}, phase_changes[3] = {0};
  CHECK(r);
  texture = bk_texture_create(r, &(BkImage){1, 1, white}, e);
  lights = bk_light_set_create(r, &(BkLighting){0}, e);
  CHECK(texture && lights);
  BkDepthTransform untouched = {{1, 2, 3, 4}}, saved = untouched;
  float invalid[16];
  memcpy(invalid, bk_identity, 64);
  invalid[7] = NAN;
  CHECK(!bk_renderer_depth_transform(NULL, bk_identity, bk_identity, e));
  CHECK(!bk_renderer_depth_transform(&untouched, NULL, bk_identity, e));
  CHECK(!bk_renderer_depth_transform(&untouched, bk_identity, NULL, e));
  CHECK(!bk_renderer_depth_transform(&untouched, invalid, bk_identity, e));
  CHECK(!bk_renderer_depth_transform(&untouched, bk_identity, invalid, e));
  CHECK(!memcmp(&untouched, &saved, sizeof(saved)));
  e[0] = 0;
  {
    BkLighting l = {.ambient = {.2f, .2f, .2f}};
    CHECK(bk_light_set_update(r, lights, &l, e));
    const int expected_green = 51;
    unsigned mode_wrong = 0, mode_changes = 0;
    for (unsigned c = 0; c < 4; ++c) {
      for (unsigned k = 0; k < 2; ++k) {
        float z = distances[c] + (k ? gaps[c] : 0), extent = distances[c];
        BkLitVertex v[4] = {0};
        for (unsigned i = 0; i < 4; ++i) {
          v[i].base = (BkVertex){xy[i][0] * extent + 333 - origin[k][0],
                                 xy[i][1] * extent + 25 - origin[k][1],
                                 z - origin[k][2],
                                 0,
                                 0,
                                 k == 1,
                                 k == 0,
                                 0,
                                 1};
          v[i].normal[2] = -1;
          v[i].ambient[k ? 0 : 1] = 1;
        }
        meshes[k] = bk_lit_mesh_create(r, v, 4, indices, 6, e);
        CHECK(meshes[k]);
      }
      for (unsigned order = 0; order < 2; ++order) {
        unsigned previous = 0;
        for (unsigned f = 0; f < 360; ++f) {
          float camera[16], view[16], projection[16], vp[16], held[16];
          memcpy(camera, bk_identity, sizeof(camera));
          double motion =
              f < 120   ? sin(f * .13) * 2
              : f < 300 ? sin(119 * .13) * 2 * exp(-(double)(f - 119) * .035)
                        : 0;
          camera[12] = 333 + (float)motion;
          camera[13] = 25 + (float)(motion * .3);
          camera[14] = (float)(motion * .5);
          CHECK(
              bk_camera_aim(camera, camera, (float[]){333, 25, distances[c]}));
          CHECK(bk_camera_view(view, camera) &&
                bk_camera_projection(projection,
                                     &(BkCameraLens){1, .75f, .5f, 126384}));
          BkDepthTransform depth;
          CHECK(bk_renderer_depth_transform(&depth, view, projection, e));
          bk_matrix_multiply(vp, view, projection);
          memcpy(held, vp, sizeof(held));
          CHECK(bk_light_set_view(r, lights, camera + 12, e) &&
                bk_renderer_begin(r, e));
          for (unsigned k = 0; k < 2; ++k) {
            unsigned index = k ^ order;
            float world[16], mvp[16];
            memcpy(world, bk_identity, 64);
            memcpy(world + 12, origin[index], 12);
            bk_matrix_multiply(mvp, world, vp);
            CHECK(legacy
                      ? bk_renderer_draw_lit_mesh(r, texture, meshes[index],
                                                  lights, mvp, world, draw, e)
                      : bk_renderer_draw_lit_mesh_projected(
                            r, texture, meshes[index], lights, mvp, world,
                            &depth, draw, e));
          }
          CHECK(!memcmp(vp, held, sizeof(held)));
          CHECK(bk_renderer_end(r, e) &&
                bk_renderer_readback(r, pixels, sizeof(pixels), e));
          unsigned red = 0;
          for (unsigned y = 12; y < 36; ++y)
            for (unsigned x = 16; x < 48; ++x) {
              const uint8_t *p = pixels + (y * W + x) * 4;
              red += p[0] > 2;
              brightness_errors +=
                  p[0] <= 2 && abs((int)p[1] - expected_green) > 2;
              CHECK(p[3] == 255 && p[2] == 0 && (p[0] > 20 || p[1] > 20));
            }
          unsigned phase = f < 120 ? 0 : f < 300 ? 1 : 2;
          phase_wrong[phase] += red;
          phase_changes[phase] += f && red != previous;
          mode_wrong += red;
          if (f && red != previous)
            mode_changes++;
          previous = red;
          frames++;
        }
      }
      for (unsigned k = 0; k < 2; ++k) {
        bk_mesh_destroy(r, meshes[k]);
        meshes[k] = NULL;
      }
    }
    printf("DEPTH_TRANSFORM frames2880 wrong_pixels%u coverage_changes%u\n",
           mode_wrong, mode_changes);
    wrong += mode_wrong;
    changes += mode_changes;
  }
  printf("%s depth transform frames%u wrong_pixels%u coverage_changes%u "
         "brightness_errors%u\n",
         wrong || brightness_errors ? "FAIL" : "PASS", frames, wrong, changes,
         brightness_errors);
  printf("PHASE wrong%u/%u/%u changes%u/%u/%u\n", phase_wrong[0],
         phase_wrong[1], phase_wrong[2], phase_changes[0], phase_changes[1],
         phase_changes[2]);
  CHECK(!wrong && !brightness_errors);
  rc = 0;
done:
  for (unsigned k = 0; k < 2; ++k)
    bk_mesh_destroy(r, meshes[k]);
  bk_light_set_destroy(r, lights);
  bk_texture_destroy(r, texture);
  bk_renderer_destroy(r);
  return rc;
}
