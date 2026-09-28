#include "render/renderer.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#define W 48
#define H 40
#define CHECK(x)                                                               \
  do {                                                                         \
    if (!(x)) {                                                                \
      if (!*e)                                                                 \
        snprintf(e, sizeof(e), "failed line%d", __LINE__);                     \
      goto done;                                                               \
    }                                                                          \
  } while (0)
static BkGpuMesh *quad(BkRenderer *r, float z, float red, float green,
                       float blue, char e[256]) {
  BkVertex v[4] = {{-1, -1, z, 0, 0, red, green, blue, 1},
                   {1, -1, z, 0, 0, red, green, blue, 1},
                   {1, 1, z, 0, 0, red, green, blue, 1},
                   {-1, 1, z, 0, 0, red, green, blue, 1}};
  return bk_mesh_create(r, v, 4, (uint16_t[]){0, 1, 2, 0, 2, 3}, 6, e);
}
static int inside(unsigned x, unsigned y, BkViewport v) {
  return x >= v.x && x < v.x + v.width && y >= v.y && y < v.y + v.height;
}
int main(void) {
  char e[256] = {0};
  int rc = 1;
  BkRenderer *r = bk_renderer_create(W, H, stdout, e);
  BkTexture *white = NULL;
  BkGpuMesh *front = NULL, *back = NULL, *marker = NULL;
  uint8_t rgba[W * H * 4], held[W * H * 4], pixel[4] = {255, 255, 255, 255};
  BkDrawState draw = {BK_BLEND_OPAQUE, 1, BK_CULL_NONE};
  CHECK(r);
  white = bk_texture_create(r, &(BkImage){1, 1, pixel}, e);
  CHECK(white);
  front = quad(r, .2f, 0, 0, 1, e);
  back = quad(r, .8f, 1, 0, 0, e);
  marker = quad(r, .1f, 0, 1, 0, e);
  CHECK(front && back && marker);
  CHECK(!bk_renderer_clear_depth(r, 1, e));
  BkRenderStats initial = bk_renderer_stats(r);
  unsigned samples = 0, captures = 0;
  for (unsigned frame = 0; frame < 120; frame++) {
    BkViewport vp = {1 + frame % 11, 2 + (frame / 11) % 7, 10 + frame % 13,
                     8 + (frame / 7) % 17};
    BkViewport stamp = {vp.x + 1, vp.y + 1, 2, 2};
    float depth = (float[]){1, 0, .5f, .9f}[frame % 4];
    CHECK(bk_renderer_begin(r, e));
    CHECK(bk_renderer_draw_mesh(r, white, front, bk_identity, draw, e));
    CHECK(bk_renderer_viewport(r, &vp, e));
    if (frame % 3 == 0) {
      CHECK(bk_renderer_capture(r, held, sizeof(held), e));
      captures++;
      CHECK(!bk_renderer_clear_depth(r, NAN, e));
      CHECK(!bk_renderer_clear_depth(r, -.1f, e));
      CHECK(!bk_renderer_clear_depth(r, 1.1f, e));
      CHECK(bk_renderer_capture(r, rgba, sizeof(rgba), e));
      captures++;
      CHECK(!memcmp(held, rgba, sizeof(rgba)));
    }
    CHECK(bk_renderer_clear_depth(r, depth, e));
    if (frame % 3 == 1) {
      /* LOAD continuation must retain the clear, color and the viewport. */
      CHECK(bk_renderer_capture(r, rgba, sizeof(rgba), e));
      captures++;
      for (unsigned i = 0; i < W * H; i++)
        CHECK(rgba[i * 4] == 0 && rgba[i * 4 + 1] == 0 &&
              rgba[i * 4 + 2] == 255 && rgba[i * 4 + 3] == 255);
    }
    CHECK(bk_renderer_draw_mesh(r, white, back, bk_identity, draw, e));
    /* Restore full viewport: outside the cleared rectangle old depth survives.
     */
    CHECK(bk_renderer_viewport(r, NULL, e));
    CHECK(bk_renderer_draw_mesh(r, white, back, bk_identity, draw, e));
    CHECK(bk_renderer_viewport(r, &stamp, e));
    CHECK(bk_renderer_draw_mesh(r, white, marker, bk_identity, draw, e));
    CHECK(bk_renderer_end(r, e));
    CHECK(bk_renderer_readback(r, rgba, sizeof(rgba), e));
    for (unsigned y = 0; y < H; y++)
      for (unsigned x = 0; x < W; x++) {
        int red = inside(x, y, vp) && depth >= .8f;
        int green = inside(x, y, stamp) && depth >= .1f;
        const uint8_t *p = rgba + (y * W + x) * 4;
        CHECK(p[0] == (red && !green ? 255 : 0) && p[1] == (green ? 255 : 0) &&
              p[2] == (!green && !red ? 255 : 0) && p[3] == 255);
        samples += 4;
      }
  }
  BkRenderStats final = bk_renderer_stats(r);
  CHECK(initial.live_allocations == final.live_allocations &&
        initial.peak_allocations == final.peak_allocations &&
        initial.live_bytes == final.live_bytes);
  printf("PASS viewport depth clear:120 frames %u channels %u capture/resumes, "
         "color/outside-depth/restored-viewport exact, stable allocations\n",
         samples, captures);
  rc = 0;
done:
  if (rc)
    fprintf(stderr, "FAIL depth clear: %s\n", e);
  bk_mesh_destroy(r, marker);
  bk_mesh_destroy(r, back);
  bk_mesh_destroy(r, front);
  bk_texture_destroy(r, white);
  bk_renderer_destroy(r);
  return rc;
}
