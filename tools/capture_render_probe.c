/* Compare identical rendering split at multiple synchronous captures against
 * independent ordinary frames. Covers preserved color/depth, viewport,
 * streamed vertices, alpha blending, empty and repeated capture. */
#include "render/renderer.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define W 96
#define H 72
#define BYTES (W * H * 4)
#define CHECK(x)                                                               \
  do {                                                                         \
    if (!(x))                                                                  \
      goto done;                                                               \
  } while (0)
static int draw(BkRenderer *r, BkTexture *t, unsigned stage, char e[256]) {
  float z = stage == 0 ? .8f : stage == 1 ? .2f : stage == 2 ? .5f : .1f;
  float right = stage == 1 ? 0 : 1;
  float red = stage == 0 || stage == 3, green = stage == 1 || stage == 3,
        blue = stage == 2;
  BkVertex v[6] = {{-1, -1, z, 0, 0, red, green, blue, 1},
                   {right, -1, z, 1, 0, red, green, blue, 1},
                   {right, 1, z, 1, 1, red, green, blue, 1},
                   {-1, -1, z, 0, 0, red, green, blue, 1},
                   {right, 1, z, 1, 1, red, green, blue, 1},
                   {-1, 1, z, 0, 1, red, green, blue, 1}};
  return bk_renderer_draw(r, t, v, 6, bk_identity, e);
}
int main(void) {
  char e[256] = {0};
  int rc = 1;
  unsigned samples = 0, captures = 0;
  BkRenderer *r = bk_renderer_create(W, H, stderr, e);
  BkTexture *t = NULL;
  uint8_t *ref = malloc(BYTES * 5), *got = malloc(BYTES);
  CHECK(r && ref && got);
  uint8_t white[4] = {255, 255, 255, 255};
  BkImage im = {1, 1, white};
  t = bk_texture_create(r, &im, e);
  CHECK(t);
  const BkViewport vp = {8, 12, 64, 48};
  for (unsigned end = 0; end < 5; ++end) {
    CHECK(bk_renderer_begin(r, e));
    for (unsigned j = 0; j < end; ++j) {
      if (j == 3)
        CHECK(bk_renderer_viewport(r, &vp, e));
      CHECK(draw(r, t, j, e));
    }
    CHECK(bk_renderer_end(r, e));
    CHECK(bk_renderer_readback(r, ref + end * BYTES, BYTES, e));
  }
  for (unsigned frame = 0; frame < 32; ++frame) {
    CHECK(bk_renderer_begin(r, e));
    /* Rejected transient data/state must not consume vertices or emit a draw.
     * Every following capture still has to match the clean reference frame. */
    BkVertex invalid[3] = {{0}};
    invalid[1].x = NAN;
    CHECK(!bk_renderer_draw_vertices(r, t, invalid, 3, bk_identity,
                                      (BkDrawState){BK_BLEND_ALPHA, 0, BK_CULL_NONE}, e));
    invalid[1].x = 0;
    CHECK(!bk_renderer_draw_vertices(r, t, invalid, 3, bk_identity,
                                      (BkDrawState){BK_BLEND_COUNT, 0, BK_CULL_NONE}, e));
    CHECK(!bk_renderer_draw_vertices(r, t, invalid, 3, bk_identity,
                                      (BkDrawState){BK_BLEND_ALPHA, 2, BK_CULL_NONE}, e));
    CHECK(!bk_renderer_capture(r, got, BYTES - 1, e));
    for (unsigned stage = 0; stage < 5; ++stage) {
      if (stage) {
        if (stage == 4)
          CHECK(bk_renderer_viewport(r, &vp, e));
        CHECK(draw(r, t, stage - 1, e));
      }
      CHECK(bk_renderer_capture(r, got, BYTES, e));
      ++captures;
      if (memcmp(got, ref + stage * BYTES, BYTES)) {
        snprintf(e, 256, "capture differed at frame%u stage%u", frame, stage);
        goto done;
      }
      samples += BYTES;
      if (stage == 4) {
        /* No viewport call: drawing again must retain the sub-viewport. */
        CHECK(draw(r, t, 3, e));
        CHECK(bk_renderer_capture(r, got, BYTES, e));
        ++captures;
        CHECK(!memcmp(got, ref + stage * BYTES, BYTES));
        samples += BYTES;
      }
    }
    CHECK(bk_renderer_end(r, e));
    CHECK(bk_renderer_readback(r, got, BYTES, e));
    CHECK(!memcmp(got, ref + 4 * BYTES, BYTES));
    samples += BYTES;
    CHECK(!bk_renderer_capture(r, got, BYTES, e));
  }
  printf("PASS active capture: %u captures %u RGBA byte comparisons, exact "
         "color/depth/viewport retention\n",
         captures, samples);
  rc = 0;
done:
  if (rc)
    fprintf(stderr, "capture probe: %s\n", e);
  bk_texture_destroy(r, t);
  bk_renderer_destroy(r);
  free(ref);
  free(got);
  return rc;
}
