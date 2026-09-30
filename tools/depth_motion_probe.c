/* Fixed lighting, camera motion, two adjacent opaque surfaces. Their strict
 * front/back order is known geometrically; no original GPU image is assumed.
 * Tests all public draw paths and camera-independent vertex illumination. */
#include "core/matrix.h"
#include "render/renderer.h"
#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#define W 64
#define H 48
#define CHECK(x)                                                               \
  do {                                                                        \
    if (!(x)) {                                                               \
      fprintf(stderr, "depth motion line%d: %s (%s)\n", __LINE__, #x, e);     \
      goto done;                                                              \
    }                                                                         \
  } while (0)
static BkLighting lighting(unsigned mode) {
  BkLighting l = {.ambient = {.2f, .2f, .2f}};
  if (mode == 1) l.point_count = 1;
  if (mode == 2) l.spot_count = 1;
  if (mode == 3) l.point_count = l.spot_count = 4;
  BkPointLight p = {.position = {0, 0, 0}, .range = 100000,
                    .diffuse = {.3f, .3f, .3f}, .attenuation0 = 1};
  if (mode == 3)
    p.diffuse[0] = p.diffuse[1] = p.diffuse[2] = .08f;
  for (unsigned i = 0; i < l.point_count; ++i) l.points[i] = p;
  for (unsigned i = 0; i < l.spot_count; ++i)
    l.spots[i] = (BkSpotLight){.point = p, .direction = {0, 0, 1},
                               .falloff = 1, .theta = .4f, .phi = 2.8f};
  return l;
}
int main(void) {
  char e[256] = {0};
  int rc = 1;
  BkRenderer *r = bk_renderer_create(W, H, stderr, e);
  BkTexture *texture = NULL;
  BkLightSet *lights = NULL;
  BkGpuMesh *meshes[2] = {0};
  uint8_t white[] = {255, 255, 255, 255}, pixels[W * H * 4];
  const uint16_t indices[] = {0, 1, 2, 0, 2, 3};
  const int xy[4][2] = {{-1, -1}, {1, -1}, {1, 1}, {-1, 1}};
  const float distances[] = {100, 500, 1000, 2000};
  const float gaps[] = {.0004f, .008f, .03f, .1f};
  BkDrawState draw = {BK_BLEND_OPAQUE, 1, BK_CULL_NONE};
  unsigned frames = 0, wrong = 0, changes = 0, brightness_errors = 0;
  CHECK(r);
  texture = bk_texture_create(r, &(BkImage){1, 1, white}, e);
  lights = bk_light_set_create(r, &(BkLighting){0}, e);
  CHECK(texture && lights);
  /* Modes0..3: ambient/point/spot/mixed8. Mode4: indexed unlit.
   * Mode5: transient UI/triangle path, using the same perspective transform. */
  for (unsigned mode = 0; mode < 6; ++mode) {
    BkLighting l = lighting(mode);
    CHECK(bk_light_set_update(r, lights, &l, e));
    double cosine = 1 / sqrt(3.0), value = mode < 4 ? .2 : 1;
    for (unsigned i = 0; i < l.point_count; ++i)
      value += l.points[i].diffuse[1] * cosine;
    for (unsigned i = 0; i < l.spot_count; ++i) {
      double outer = cos(l.spots[i].phi * .5), inner = cos(l.spots[i].theta * .5);
      value += l.spots[i].point.diffuse[1] * cosine * (cosine - outer) / (inner - outer);
    }
    int expected_green = (int)lround(255 * value);
    unsigned mode_wrong = 0, mode_changes = 0;
    for (unsigned c = 0; c < 4; ++c) {
      BkVertex transient[2][6];
      for (unsigned k = 0; k < 2; ++k) {
        float z = distances[c] + (k ? gaps[c] : 0), extent = distances[c];
        BkLitVertex v[4] = {0};
        BkVertex flat[4];
        for (unsigned i = 0; i < 4; ++i) {
          v[i].base = (BkVertex){xy[i][0] * extent, xy[i][1] * extent, z,
                                 0, 0, k == 1, k == 0, 0, 1};
          v[i].normal[2] = -1;
          v[i].ambient[k ? 0 : 1] = 1;
          flat[i] = v[i].base;
        }
        for (unsigned i = 0; i < 6; ++i) transient[k][i] = flat[indices[i]];
        if (mode < 4)
          meshes[k] = bk_lit_mesh_create(r, v, 4, indices, 6, e);
        else if (mode == 4)
          meshes[k] = bk_mesh_create(r, flat, 4, indices, 6, e);
        CHECK(mode == 5 || meshes[k]);
      }
      for (unsigned order = 0; order < 2; ++order) {
        unsigned previous = 0;
        for (unsigned f = 0; f < 96; ++f) {
          float camera[16], view[16], projection[16], vp[16], held[16];
          memcpy(camera, bk_identity, sizeof(camera));
          camera[12] = sinf(f * .13f) * 2;
          camera[14] = cosf(f * .17f) * 2;
          CHECK(bk_camera_view(view, camera) &&
                bk_camera_projection(projection, &(BkCameraLens){1, .75f, .5f, 126384}));
          bk_matrix_multiply(vp, view, projection);
          memcpy(held, vp, sizeof(held));
          CHECK(bk_light_set_view(r, lights, camera + 12, e) && bk_renderer_begin(r, e));
          for (unsigned k = 0; k < 2; ++k) {
            unsigned index = k ^ order;
            if (mode < 4)
              CHECK(bk_renderer_draw_lit_mesh(r, texture, meshes[index], lights,
                                              vp, bk_identity, draw, e));
            else if (mode == 4)
              CHECK(bk_renderer_draw_mesh(r, texture, meshes[index], vp, draw, e));
            else
              CHECK(bk_renderer_draw_vertices(r, texture, transient[index], 6, vp, draw, e));
          }
          CHECK(!memcmp(vp, held, sizeof(held)));
          CHECK(bk_renderer_end(r, e) && bk_renderer_readback(r, pixels, sizeof(pixels), e));
          unsigned red = 0;
          for (unsigned y = 12; y < 36; ++y)
            for (unsigned x = 16; x < 48; ++x) {
              const uint8_t *p = pixels + (y * W + x) * 4;
              red += p[0] > 2;
              brightness_errors += p[0] <= 2 && abs((int)p[1] - expected_green) > 2;
              CHECK(p[3] == 255 && p[2] == 0 && (p[0] > 20 || p[1] > 20));
            }
          mode_wrong += red;
          if (f && red != previous) mode_changes++;
          previous = red;
          frames++;
        }
      }
      for (unsigned k = 0; k < 2; ++k) {
        bk_mesh_destroy(r, meshes[k]);
        meshes[k] = NULL;
      }
    }
    printf("DEPTH_MOTION mode%u frames768 wrong_pixels%u coverage_changes%u\n",
           mode, mode_wrong, mode_changes);
    wrong += mode_wrong;
    changes += mode_changes;
  }
  printf("%s depth motion frames%u wrong_pixels%u coverage_changes%u brightness_errors%u\n",
         wrong || brightness_errors ? "FAIL" : "PASS", frames, wrong, changes, brightness_errors);
  CHECK(!wrong && !brightness_errors);
  rc = 0;
done:
  for (unsigned k = 0; k < 2; ++k) bk_mesh_destroy(r, meshes[k]);
  bk_light_set_destroy(r, lights);
  bk_texture_destroy(r, texture);
  bk_renderer_destroy(r);
  return rc;
}
