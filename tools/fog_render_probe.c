/* Independent double reference for fog after texture/specular, before blend.
 * Perspective interpolation, all formulas, vertex/range vs pixel, alpha,
 * immutable in-flight descriptors, and rejection preserving old settings. */
#include "render/renderer.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#define SIZE 128
static double factor(const BkFog *fog, double distance) {
  if (!fog->enabled || !fog->mode)
    return 1;
  double f = fog->mode == 1   ? exp(-fog->density * distance)
             : fog->mode == 2 ? exp(-pow(fog->density * distance, 2))
                              : (fog->end - distance) / (fog->end - fog->start);
  return fmax(0, fmin(1, f));
}
int main(void) {
  char error[256] = {0};
  BkRenderer *r = bk_renderer_create(SIZE, SIZE, stderr, error);
  BkLightSet *lights = NULL;
  BkTexture *texture = NULL;
  BkGpuMesh *mesh = NULL;
  int rc = 1;
  if (!r)
    goto done;
  BkLighting illumination = {0};
  lights = bk_light_set_create(r, &illumination, error);
  uint8_t pixel[4] = {128, 192, 255, 128};
  BkImage image = {1, 1, pixel};
  texture = bk_texture_create(r, &image, error);
  if (!lights || !texture)
    goto done;
  float projection[16];
  BkCameraLens lens = {1, 1, .5f, 100};
  assert(bk_camera_projection(projection, &lens));
  uint16_t indices[] = {0, 1, 2};
  uint8_t pixels[SIZE * SIZE * 4];
  unsigned cases = 0;
  for (unsigned test = 0; test < 80; ++test) {
    BkFog fog = {.enabled = 1,
                 .mode = 1 + test % 3,
                 .table = (test / 3) % 2,
                 .range_based = (test / 6) % 2,
                 .color = 0x7f305070,
                 .start = 2,
                 .end = 12,
                 .density = .12f};
    if (test == 0)
      fog.enabled = 0;
    if (test == 1)
      fog.mode = 0;
    if (test == 2)
      fog.start = -12, fog.end = -2;
    if (test == 5)
      fog.start = 50, fog.end = 60;
    if (test == 7)
      fog.density = 0;
    if (test == 9)
      fog.density = 1;
    const double ndc[3][2] = {{-1, -1}, {3, -1}, {-1, 3}};
    double world[3][3], depth[3];
    float transform[16], view[16], camera[3] = {3, -2, 5};
    memcpy(transform, bk_identity, 64);
    memcpy(view, bk_identity, 64);
    for (unsigned j = 0; j < 3; ++j)
      transform[12 + j] = camera[j], view[12 + j] = -camera[j];
    BkLitVertex v[3] = {0};
    for (unsigned i = 0; i < 3; ++i) {
      depth[i] = 2 + i * 5;
      world[i][0] = ndc[i][0] * depth[i] / projection[0];
      world[i][1] = ndc[i][1] * depth[i] / projection[5];
      world[i][2] = depth[i];
      v[i].base = (BkVertex){world[i][0], world[i][1], world[i][2], .5f, .5f,
                             1,           1,           1,           .6f};
      v[i].normal[2] = 1;
      v[i].emissive[0] = .8f;
      v[i].emissive[1] = .6f;
      v[i].emissive[2] = .4f;
    }
    mesh = bk_lit_mesh_create(r, v, 3, indices, 3, error);
    if (!mesh || !bk_light_set_view(r, lights, camera, error) ||
        !bk_light_set_fog(r, lights, &fog, view, error) ||
        !bk_light_set_update(r, lights, &illumination, error))
      goto done;
    BkFog bad = fog;
    bad.enabled = 1;
    bad.mode = 3;
    bad.end = bad.start;
    assert(!bk_light_set_fog(r, lights, &bad, view, error));
    bad = fog;
    bad.density = NAN;
    assert(!bk_light_set_fog(r, lights, &bad, view, error));
    if (!bk_renderer_begin(r, error))
      goto done;
    assert(!bk_light_set_fog(r, lights, &fog, view, error));
    if (!bk_renderer_draw_lit_mesh(
            r, texture, mesh, lights, projection, transform,
            (BkDrawState){BK_BLEND_OPAQUE, 1, BK_CULL_NONE}, error) ||
        !bk_renderer_end(r, error) ||
        !bk_renderer_readback(r, pixels, sizeof(pixels), error))
      goto done;
    /* gl_FragCoord's pixel center gives screen barycentrics; reconstruct
     * perspective-correct eye depth or interpolated per-vertex fog. */
    unsigned positions[][2] = {{64, 64}, {19, 75}, {83, 23}, {108, 104}};
    for (unsigned p = 0; p < 4; ++p) {
      double b = (2 * (positions[p][0] + .5) / SIZE) / 4;
      double c = (2 * (positions[p][1] + .5) / SIZE) / 4;
      double weights[] = {1 - b - c, b, c}, sum = 0, d = 0, f = 0;
      for (unsigned i = 0; i < 3; ++i)
        sum += weights[i] / depth[i];
      for (unsigned i = 0; i < 3; ++i) {
        double w = weights[i] / depth[i] / sum;
        double distance = fog.range_based ? sqrt(world[i][0] * world[i][0] +
                                                 world[i][1] * world[i][1] +
                                                 depth[i] * depth[i])
                                          : depth[i];
        d += w * depth[i];
        f += w * factor(&fog, distance);
      }
      if (fog.table)
        f = factor(&fog, d);
      double colors[] = {.8 * 128 / 255., .6 * 192 / 255., .4};
      unsigned offset = (positions[p][1] * SIZE + positions[p][0]) * 4;
      for (unsigned j = 0; j < 4; ++j) {
        double expected =
            j == 3
                ? .6 * 128
                : 255 * (colors[j] * f +
                         ((fog.color >> (16 - 8 * j)) & 255) / 255. * (1 - f));
        int delta = (int)pixels[offset + j] - (int)lround(expected);
        if (abs(delta) > 2) {
          snprintf(error, 256,
                   "fog case%u pixel%u channel%u actual%u expected%.4f", test,
                   p, j, pixels[offset + j], expected);
          goto done;
        }
      }
      ++cases;
    }
    bk_mesh_destroy(r, mesh);
    mesh = NULL;
  }
  printf("PASS fog: %u perspective samples,80 modes/range/pixel cases, RGB "
         "error<=2/255, preserved alpha and invalid-state rejection\n",
         cases);
  rc = 0;
done:
  if (rc)
    fprintf(stderr, "FAIL fog: %s\n", error);
  bk_mesh_destroy(r, mesh);
  bk_texture_destroy(r, texture);
  bk_light_set_destroy(r, lights);
  bk_renderer_destroy(r);
  return rc;
}
