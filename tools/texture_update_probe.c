#include "render/renderer.h"
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
static const BkVertex quad[6] = {
    {-1, -1, 0, 0, 0, 1, 1, 1, 1}, {1, -1, 0, 1, 0, 1, 1, 1, 1},
    {1, 1, 0, 1, 1, 1, 1, 1, 1},   {-1, -1, 0, 0, 0, 1, 1, 1, 1},
    {1, 1, 0, 1, 1, 1, 1, 1, 1},   {-1, 1, 0, 0, 1, 1, 1, 1, 1}};
static void pattern(BkImage *im, unsigned seed) {
  for (unsigned y = 0; y < im->height; y++)
    for (unsigned x = 0; x < im->width; x++) {
      uint8_t *p = im->rgba + 4 * ((size_t)y * im->width + x);
      p[0] = (uint8_t)(seed * 71 + x * 11);
      p[1] = (uint8_t)(seed * 23 + y * 17);
      p[2] = (uint8_t)(seed * 29 + (x + y) * 7);
      p[3] = (uint8_t)(100 + (x * 9 + y * 3 + seed) % 156);
    }
}
int main(void) {
  char error[256] = {0};
  BkRenderer *r = NULL, *other = NULL;
  BkTexture *a = NULL, *b = NULL, *temporary = NULL;
  uint8_t first[17 * 13 * 4], second[17 * 13 * 4], pixels[34 * 13 * 4],
      capture[sizeof(pixels)];
  BkImage ia = {17, 13, first}, ib = {17, 13, second};
  pattern(&ia, 0);
  pattern(&ib, 1);
  r = bk_renderer_create(34, 13, stderr, error);
  other = bk_renderer_create(1, 1, stderr, error);
  if (!r || !other ||
      !(a = bk_texture_create_sampled(r, &ia, BK_WRAP_REPEAT, error)) ||
      !(b = bk_texture_create(r, &ib, error)))
    goto fail;
  uint32_t key = bk_texture_sort_key(a);
  uint64_t hash = UINT64_C(14695981039346656037);
  unsigned max_error = 0;
  for (unsigned frame = 0; frame < 80; frame++) {
    pattern(&ia, frame);
    if (!bk_texture_update(r, a, &ia, error))
      goto fail;
    pattern(&ia, frame + 10); /* coalesce preceding update */
    if (!bk_texture_update(r, a, &ia, error))
      goto fail;
    if (!(frame % 7)) {
      pattern(&ib, frame + 100);
      if (!bk_texture_update(r, b, &ib, error))
        goto fail;
    }
    if (!(frame % 11)) {
      temporary = bk_texture_create(r, &ia, error);
      if (!temporary || !bk_texture_update(r, temporary, &ia, error))
        goto fail;
      bk_texture_destroy(r, temporary);
      temporary = NULL; /* pending list removal */
    }
    BkImage bad = ia;
    bad.width++;
    if (bk_texture_update(r, a, &bad, error) ||
        bk_texture_update(other, a, &ia, error) ||
        bk_texture_sort_key(a) != key) {
      snprintf(error, 256, "invalid update accepted or sort identity changed");
      goto fail;
    }
    if (!bk_renderer_begin(r, error))
      goto fail;
    if (bk_texture_update(r, a, &ia, error)) {
      snprintf(error, 256, "active-frame update accepted");
      goto fail;
    }
    for (unsigned side = 0; side < 2; side++) {
      BkViewport viewport = {side * 17, 0, 17, 13};
      if (!bk_renderer_viewport(r, &viewport, error) ||
          !bk_renderer_draw(r, side ? b : a, quad, 6, bk_identity, error))
        goto fail;
    }
    if (frame == 39 && !bk_renderer_capture(r, capture, sizeof(capture), error))
      goto fail;
    if (!bk_renderer_end(r, error))
      goto fail;
    /* Alternate frames deliberately remain in flight when updated next. */
    if (frame % 2 == 0)
      continue;
    if (!bk_renderer_readback(r, pixels, sizeof(pixels), error))
      goto fail;
    if (frame == 39 && memcmp(capture, pixels, sizeof(pixels))) {
      snprintf(error, 256, "capture/resume changed pending texture output");
      goto fail;
    }
    for (unsigned y = 0; y < 13; y++)
      for (unsigned x = 0; x < 34; x++)
        for (unsigned c = 0; c < 4; c++) {
          const uint8_t *source =
              (x < 17 ? first : second) + 4 * (y * 17 + x % 17);
          unsigned want = c == 3 ? 255 : (source[c] * source[3] + 127) / 255;
          uint8_t got = pixels[4 * (y * 34 + x) + c];
          unsigned delta = (unsigned)abs((int)got - (int)want);
          if (delta > max_error)
            max_error = delta;
          if (delta > 1) {
            snprintf(error, 256,
                     "texture pixel mismatch frame%u (%u,%u,%u): %u/%u", frame,
                     x, y, c, got, want);
            goto fail;
          }
          hash = (hash ^ got) * UINT64_C(1099511628211);
        }
  }
  /* Removing a middle pending entry must preserve its successor. */
  if (!bk_texture_update(r, b, &ib, error) ||
      !bk_texture_update(r, a, &ia, error))
    goto fail;
  bk_texture_destroy(r, b);
  b = NULL;
  if (!bk_renderer_begin(r, error) ||
      !bk_renderer_draw(r, a, quad, 6, bk_identity, error) ||
      !bk_renderer_end(r, error))
    goto fail;
  bk_texture_destroy(r, a);
  a = NULL; /* waits for submitted staging reads */
  bk_renderer_destroy(other);
  bk_renderer_destroy(r);
  printf("PASS texture update81frames/40readbacks, coalesce, pending deletion, "
         "capture, identity, pixel_error%u hash%016" PRIx64 "\n",
         max_error, hash);
  return 0;
fail:
  fprintf(stderr, "texture update probe FAILED: %s\n", error);
  if (r) {
    bk_texture_destroy(r, temporary);
    bk_texture_destroy(r, b);
    bk_texture_destroy(r, a);
  }
  bk_renderer_destroy(other);
  bk_renderer_destroy(r);
  return 1;
}
