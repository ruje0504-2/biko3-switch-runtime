#include "render/renderer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(int argc, char **argv) {
  char error[256] = {0};
  BkImage im = {0};
  BkArchive archive = {0};
  uint8_t *bytes = NULL, *pixels = NULL;
  BkRenderer *r = NULL;
  BkTexture *texture = NULL;
  int result = 1;
  if (argc != 1 && argc != 4) {
    fprintf(stderr, "usage: render-probe [archive image output.rgba]\n");
    return 2;
  }
  if (argc == 4) {
    if (!bk_archive_open(&archive, argv[1], error))
      goto done;
    const BkEntry *entry = bk_archive_find(&archive, argv[2]);
    if (!entry || !bk_archive_read(&archive, entry, &bytes, error))
      goto done;
    if (!bk_image_decode(bytes, entry->size, &im, error))
      goto done;
  } else {
    im.width = 17;
    im.height = 13;
    im.rgba = malloc(17 * 13 * 4);
    if (!im.rgba)
      goto done;
    for (unsigned y = 0; y < 13; y++)
      for (unsigned x = 0; x < 17; x++) {
        uint8_t *p = im.rgba + (y * 17 + x) * 4;
        p[0] = (uint8_t)(x * 15);
        p[1] = (uint8_t)(y * 20);
        p[2] = (x + y) % 2 ? 240 : 10;
        p[3] = 255;
      }
  }
  size_t size = (size_t)im.width * im.height * 4;
  pixels = malloc(size);
  if (!pixels)
    goto done;
  r = bk_renderer_create(im.width, im.height, stderr, error);
  if (!r)
    goto done;
  texture = bk_texture_create(r, &im, error);
  if (!texture)
    goto done;
  BkVertex v[6] = {{-1, -1, 0, 0, 0, 1, 1, 1, 1}, {1, -1, 0, 1, 0, 1, 1, 1, 1},
                   {1, 1, 0, 1, 1, 1, 1, 1, 1},   {-1, -1, 0, 0, 0, 1, 1, 1, 1},
                   {1, 1, 0, 1, 1, 1, 1, 1, 1},   {-1, 1, 0, 0, 1, 1, 1, 1, 1}};
  for (unsigned frame = 0; frame < 16; frame++) {
    if (!bk_renderer_begin(r, error) ||
        !bk_renderer_draw(r, texture, v, 6, bk_identity, error) ||
        !bk_renderer_end(r, error) ||
        !bk_renderer_readback(r, pixels, size, error))
      goto done;
    unsigned max = 0;
    for (size_t i = 0; i < size; i++) {
      /* Pipeline alpha-blends to opaque black; output alpha stays 255. */
      unsigned expected =
          (i % 4 == 3)
              ? 255
              : ((unsigned)im.rgba[i] * im.rgba[(i & ~(size_t)3) + 3] + 127) /
                    255;
      unsigned delta = (unsigned)abs((int)pixels[i] - (int)expected);
      if (delta > max)
        max = delta;
    }
    if (max > 1) {
      snprintf(error, 256, "Vulkan pixel comparison failed: max difference %u",
               max);
      goto done;
    }
  }
  if (argc == 4) {
    FILE *out = fopen(argv[3], "wb");
    if (!out) {
      snprintf(error, 256, "cannot open readback output");
      goto done;
    }
    int ok = fwrite(pixels, 1, size, out) == size, closed = fclose(out) == 0;
    if (!ok || !closed) {
      snprintf(error, 256, "readback output write failed");
      goto done;
    }
  }
  fprintf(stderr,
          "PASS: %ux%u, 16 Vulkan frames, all RGBA components within 1/255\n",
          im.width, im.height);
  result = 0;
done:
  if (result)
    fprintf(stderr, "render probe failed: %s\n",
            error[0] ? error : "allocation/resource missing");
  if (texture)
    bk_texture_destroy(r, texture);
  bk_renderer_destroy(r);
  bk_image_free(&im);
  bk_archive_close(&archive);
  free(bytes);
  free(pixels);
  return result;
}
