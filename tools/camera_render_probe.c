/* Pixel checks of viewport/scissor, UI and next-frame restoration, and the
 * original projection's clip interval. No copyrighted data required. */
#include "core/camera.h"
#include "render/renderer.h"
#include <stdlib.h>
#include <string.h>
static int run(unsigned width, unsigned height, BkViewport expected) {
  char error[256] = {0};
  int ok = 0;
  BkRenderer *r = bk_renderer_create(width, height, stderr, error);
  BkTexture *t = NULL;
  uint8_t *pixels = malloc((size_t)width * height * 4);
  uint8_t white[] = {255, 255, 255, 255};
  BkImage image = {1, 1, white};
  if (!r || !pixels)
    goto done;
  t = bk_texture_create(r, &image, error);
  if (!t)
    goto done;
  BkViewport fitted;
  if (!bk_camera_fit(&fitted, width, height, 4, 3) ||
      memcmp(&fitted, &expected, sizeof(fitted)))
    goto done;
  /* Oversized triangle must not color the bars. */
  BkVertex tri[] = {{-3, -3, 0, 0, 0, 0, 0, 1, 1},
                    {9, -3, 0, 0, 0, 0, 0, 1, 1},
                    {-3, 9, 0, 0, 0, 0, 0, 1, 1}};
  if (bk_renderer_viewport(r, &fitted, error))
    goto done;
  for (unsigned pass = 0; pass < 3; pass++) {
    if (!bk_renderer_begin(r, error))
      goto done;
    BkViewport bad = {UINT32_MAX, 0, 2, 2};
    if (bk_renderer_viewport(r, &bad, error))
      goto done;
    if (pass != 1 && !bk_renderer_viewport(r, &fitted, error))
      goto done;
    if (!bk_renderer_draw(r, t, tri, 3, bk_identity, error))
      goto done;
    if (pass == 2) {
      if (!bk_renderer_viewport(r, NULL, error))
        goto done;
      /* Full-width top row tests scissor restoration within the same frame. */
      float y = -1 + 2.0f / height;
      BkVertex ui[] = {
          {-1, -1, 0, 0, 0, 1, 0, 0, 1}, {1, -1, 0, 0, 0, 1, 0, 0, 1},
          {1, y, 0, 0, 0, 1, 0, 0, 1},   {-1, -1, 0, 0, 0, 1, 0, 0, 1},
          {1, y, 0, 0, 0, 1, 0, 0, 1},   {-1, y, 0, 0, 0, 1, 0, 0, 1}};
      if (!bk_renderer_draw(r, t, ui, 6, bk_identity, error))
        goto done;
    }
    if (!bk_renderer_end(r, error) ||
        !bk_renderer_readback(r, pixels, (size_t)width * height * 4, error))
      goto done;
    for (unsigned y = 0; y < height; y++)
      for (unsigned x = 0; x < width; x++) {
        int inside =
            pass == 1 || (x >= expected.x && x < expected.x + expected.width &&
                          y >= expected.y && y < expected.y + expected.height);
        int red = pass == 2 && y == 0;
        uint8_t want[] = {(uint8_t)(red ? 255 : 0), 0,
                          (uint8_t)(inside && !red ? 255 : 0), 255};
        if (memcmp(pixels + ((size_t)y * width + x) * 4, want, 4)) {
          snprintf(error, 256, "viewport %ux%u pass %u pixel %u,%u mismatch",
                   width, height, pass, x, y);
          goto done;
        }
      }
  }
  BkCameraLens lens = {1, .75f, .5f, 126384};
  float projection[16];
  if (!bk_camera_projection(projection, &lens))
    goto done;
  const float depths[] = {-1, .25f, .5f, 2, 126384, 252768};
  for (unsigned i = 0; i < 6; i++) {
    float z = depths[i];
    BkVertex v[] = {{-2 * z, -2 * z, z, 0, 0, 1, 1, 1, 1},
                    {0, 2 * z, z, 0, 0, 1, 1, 1, 1},
                    {2 * z, -2 * z, z, 0, 0, 1, 1, 1, 1}};
    if (!bk_renderer_begin(r, error) ||
        !bk_renderer_viewport(r, &fitted, error) ||
        !bk_renderer_draw(r, t, v, 3, projection, error) ||
        !bk_renderer_end(r, error) ||
        !bk_renderer_readback(r, pixels, (size_t)width * height * 4, error))
      goto done;
    uint8_t expected_color = i >= 2 && i <= 4 ? 255 : 0;
    const uint8_t *p = pixels + ((size_t)(height / 2) * width + width / 2) * 4;
    if (p[0] != expected_color || p[1] != expected_color ||
        p[2] != expected_color) {
      snprintf(error, 256, "projection depth %g mismatch: %u", z, p[0]);
      goto done;
    }
  }
  ok = 1;
done:
  bk_texture_destroy(r, t);
  bk_renderer_destroy(r);
  free(pixels);
  if (!ok)
    fprintf(stderr, "camera render probe: %s\n", error);
  return ok;
}
int main(void) {
  if (!run(128, 72, (BkViewport){16, 0, 96, 72}) ||
      !run(80, 80, (BkViewport){0, 10, 80, 60}) ||
      !run(81, 61, (BkViewport){0, 0, 81, 60}))
    return 1;
  puts("PASS: 3 target shapes, fitted viewport/scissor pixels, same-frame UI "
       "and next-frame reset, 18 original-lens depth cases");
  return 0;
}
