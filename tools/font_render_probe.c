/* Actual FTT pickup masks through Vulkan upload/sampling/alpha blending.
 * Uses an explicit diagnostic quad; original UI scrolling/outline/notice
 * visibility rules are not implied by this resource/rendering check. */
#include "render/renderer.h"
#include "resource/font.h"
#include "resource/message.h"
#include "resource/store.h"
#include "ui/text_image.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#define WIDTH 640
#define HEIGHT 160
#define CHECK(x)                                                               \
  do {                                                                         \
    if (!(x))                                                                  \
      goto done;                                                               \
  } while (0)
static double alpha(const BkImage *image, int x, int y) {
  if (x < 0)
    x = 0;
  if (y < 0)
    y = 0;
  if (x >= (int)image->width)
    x = (int)image->width - 1;
  if (y >= (int)image->height)
    y = (int)image->height - 1;
  return image->rgba[((size_t)y * image->width + (unsigned)x) * 4 + 3] / 255.;
}
int main(int argc, char **argv) {
  if (argc != 2 && argc != 3)
    return 2;
  char error[256] = {0}, path[1024];
  BkResourceStore *store = bk_resources_create(error);
  BkRenderer *renderer = NULL;
  BkTexture *texture = NULL;
  BkFont *font = NULL;
  BkBlob blob = {0}, text = {0};
  BkImage image = {0};
  uint8_t *pixels = malloc(WIDTH * HEIGHT * 4);
  unsigned frames = 0, checks = 0, nonzero = 0, max_error = 0;
  int rc = 1;
  CHECK(store && pixels);
  CHECK(bk_resources_mount_directory(store, "fonts", argv[1], 4 * 1024 * 1024,
                                     error));
  CHECK(bk_resources_read(store, "fonts", "Type_S.FTT", &blob, error) ==
        BK_RESOURCE_OK);
  font = bk_font_decode(blob.data, blob.size, error);
  CHECK(font);
  bk_blob_free(&blob);
  CHECK(snprintf(path, sizeof(path), "%s/bk3_05.pp", argv[1]) <
        (int)sizeof(path));
  CHECK(bk_resources_mount(store, "bk3_05", path, error));
  CHECK(bk_resources_read(store, "bk3_05", "i00_00.txt", &text, error) ==
        BK_RESOURCE_OK);
  renderer = bk_renderer_create(WIDTH, HEIGHT, stderr, error);
  CHECK(renderer);
  BkTextLayout layout = {0, 0, 632, 32, 32};
  for (unsigned group = 0; group < 5; ++group)
    for (unsigned item = 0; item < 5; ++item) {
      BkMessage message = {0};
      CHECK(bk_message_lookup(text.data, text.size,
                              (int32_t)(group * 10000 + item), &message,
                              error));
      CHECK(bk_text_image(font, message.bytes, message.length, &layout, 632, 96,
                          &image, error));
      texture = bk_texture_create(renderer, &image, error);
      CHECK(texture);
      for (unsigned pass = 0; pass < 2; ++pass) {
        double scale = pass ? .5 : 1, left = 4, top = 8,
               w = image.width * scale, h = image.height * scale;
        float x0 = (float)(left * 2 / WIDTH - 1),
              y0 = (float)(top * 2 / HEIGHT - 1),
              x1 = (float)((left + w) * 2 / WIDTH - 1),
              y1 = (float)((top + h) * 2 / HEIGHT - 1);
        BkVertex v[6] = {
            {x0, y0, 0, 0, 0, 1, 1, 1, 1}, {x1, y0, 0, 1, 0, 1, 1, 1, 1},
            {x1, y1, 0, 1, 1, 1, 1, 1, 1}, {x0, y0, 0, 0, 0, 1, 1, 1, 1},
            {x1, y1, 0, 1, 1, 1, 1, 1, 1}, {x0, y1, 0, 0, 1, 1, 1, 1, 1}};
        CHECK(bk_renderer_begin(renderer, error));
        CHECK(bk_renderer_draw(renderer, texture, v, 6, bk_identity, error));
        CHECK(bk_renderer_end(renderer, error));
        CHECK(
            bk_renderer_readback(renderer, pixels, WIDTH * HEIGHT * 4, error));
        for (unsigned y = 0; y < HEIGHT; ++y)
          for (unsigned x = 0; x < WIDTH; ++x) {
            double value = 0;
            if (x >= left && x < left + w && y >= top && y < top + h) {
              double sx = (x + .5 - left) / scale - .5,
                     sy = (y + .5 - top) / scale - .5;
              int ix = (int)floor(sx), iy = (int)floor(sy);
              double fx = sx - ix, fy = sy - iy;
              value = ((alpha(&image, ix, iy) * (1 - fx) +
                        alpha(&image, ix + 1, iy) * fx) *
                           (1 - fy) +
                       (alpha(&image, ix, iy + 1) * (1 - fx) +
                        alpha(&image, ix + 1, iy + 1) * fx) *
                           fy) *
                      255;
            }
            size_t p = ((size_t)y * WIDTH + x) * 4;
            for (unsigned c = 0; c < 3; ++c) {
              unsigned diff =
                  (unsigned)abs((int)pixels[p + c] - (int)lround(value));
              if (diff > max_error)
                max_error = diff;
              if (diff > 1) {
                snprintf(error, 256,
                         "font g%u i%u pass%u pixel%u,%u actual%u expected%.2f",
                         group, item, pass, x, y, pixels[p + c], value);
                goto done;
              }
            }
            assert(pixels[p + 3] == 255);
            checks++;
            nonzero += pixels[p] != 0;
          }
        if (argc == 3 && group == 0 && item == 0 && pass == 0) {
          FILE *f = fopen(argv[2], "wb");
          CHECK(f);
          int ok = fprintf(f, "P6\n%d %d\n255\n", WIDTH, HEIGHT) > 0;
          for (unsigned p = 0; p < WIDTH * HEIGHT && ok; ++p)
            ok = fwrite(pixels + p * 4, 1, 3, f) == 3;
          if (fclose(f))
            ok = 0;
          CHECK(ok);
        }
        frames++;
      }
      bk_texture_destroy(renderer, texture);
      texture = NULL;
      bk_image_free(&image);
    }
  assert(nonzero);
  printf("PASS FTT Vulkan frames=%u pixels=%u nonzero=%u max_error=%u/255\n",
         frames, checks, nonzero, max_error);
  rc = 0;
done:
  if (rc)
    fprintf(stderr, "%s\n", error);
  bk_texture_destroy(renderer, texture);
  bk_renderer_destroy(renderer);
  bk_image_free(&image);
  bk_font_destroy(font);
  bk_blob_free(&blob);
  bk_blob_free(&text);
  bk_resources_destroy(store);
  free(pixels);
  return rc;
}
