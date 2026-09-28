/* Real rain texture vs independent CPU bilinear sampling and ordered
 * src-alpha blending, using D3D's integer pixel-center coordinates. */
#include "scene/rain_render.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
static double texel(const BkImage *im, int x, int y, unsigned c) {
  if (x < 0)
    x = 0;
  if (y < 0)
    y = 0;
  if (x > 255)
    x = 255;
  if (y > 255)
    y = 255;
  return im->rgba[(y * 256 + x) * 4 + c] / 255.0;
}
static void expected(const BkImage *im, const BkRainDraw *draw, unsigned x,
                     unsigned y, int rgba[4]) {
  for (unsigned c = 0; c < 4; ++c)
    rgba[c] = c == 3 ? 255 : 0;
  for (unsigned i = 0; i < draw->count; ++i) {
    const BkRainSprite *s = &draw->sprites[i];
    if (x < s->x || x >= s->x + 256 || y < s->y || y >= s->y + 512)
      continue;
    double u = x - (double)s->x - .5, v = (y - (double)s->y) * .5 - .5;
    int tx = (int)floor(u), ty = (int)floor(v);
    double fx = u - tx, fy = v - ty, color[4];
    for (unsigned c = 0; c < 4; ++c)
      color[c] =
          (texel(im, tx, ty, c) * (1 - fx) + texel(im, tx + 1, ty, c) * fx) *
              (1 - fy) +
          (texel(im, tx, ty + 1, c) * (1 - fx) +
           texel(im, tx + 1, ty + 1, c) * fx) *
              fy;
    for (unsigned c = 0; c < 4; ++c)
      rgba[c] =
          (int)lround(color[c] * color[3] * 255 + rgba[c] * (1 - color[3]));
  }
}
int main(int argc, char **argv) {
  if (argc != 2 && argc != 3)
    return 2;
  char error[256] = {0}, path[1024];
  BkResourceStore *store = bk_resources_create(error);
  BkRenderer *renderer = NULL;
  BkRainRender *rain = NULL;
  BkBlob blob = {0};
  BkImage image = {0};
  uint8_t *pixels = malloc(640 * 480 * 4);
  int rc = 1, max_error = 0;
  unsigned frames = 0, checks = 0, profiles = 0;
  uint64_t colored = 0;
  if (!store || !pixels)
    goto done;
  snprintf(path, sizeof(path), "%s/bk3_20.pp", argv[1]);
  if (!bk_resources_mount(store, "bk3_20", path, error) ||
      bk_resources_read(store, "bk3_20", "rain.tga", &blob, error) !=
          BK_RESOURCE_OK ||
      !bk_image_decode(blob.data, blob.size, &image, error))
    goto done;
  bk_blob_free(&blob);
  for (unsigned quality = 0; quality < 2; ++quality) {
    unsigned width = quality ? 640 : 320, height = quality ? 480 : 240;
    renderer = bk_renderer_create(width, height, stderr, error);
    if (!renderer)
      goto done;
    for (unsigned area = 0; area < 9; ++area) {
      rain = bk_rain_render_create(renderer, store, 4, area, 1, error);
      if (!rain)
        goto done;
      assert(!bk_rain_render_draw(rain, error));
      uint32_t random = 123 + area;
      for (unsigned frame = 0; frame < 12; ++frame) {
        int enabled = frame != 4 && frame != 5;
        if (!bk_rain_render_prepare(rain, &random, enabled, width, height,
                                    error))
          goto done;
        const BkRainDraw *draw = bk_rain_render_snapshot(rain);
        assert(draw && draw->count == (enabled && area < 7 ? 16 : 0));
        BkRainState saved = *bk_rain_render_state(rain);
        BkRainDraw saved_draw = *draw;
        uint32_t saved_random = random;
        assert(
            !bk_rain_render_prepare(rain, &random, enabled, 0, height, error));
        assert(!memcmp(&saved, bk_rain_render_state(rain), sizeof(saved)) &&
               !memcmp(&saved_draw, bk_rain_render_snapshot(rain),
                       sizeof(saved_draw)) &&
               random == saved_random);
        if (!bk_renderer_begin(renderer, error) ||
            !bk_rain_render_draw(rain, error) ||
            !bk_renderer_end(renderer, error) ||
            !bk_renderer_readback(renderer, pixels, width * height * 4, error))
          goto done;
        for (unsigned p = 0; p < width * height; ++p)
          colored += pixels[p * 4] || pixels[p * 4 + 1] || pixels[p * 4 + 2];
        for (unsigned sample = 0; sample < 128; ++sample) {
          unsigned x = (sample * 37 + frame * 17) % width,
                   y = (sample * 29 + frame * 23) % height;
          int want[4];
          expected(&image, draw, x, y, want);
          for (unsigned c = 0; c < 4; ++c) {
            int actual = pixels[(y * width + x) * 4 + c],
                d = abs(actual - want[c]);
            if (d > max_error)
              max_error = d;
            if (d > 3) {
              snprintf(
                  error, 256,
                  "area%u frame%u pixel%u,%u channel%u actual%d expected%d",
                  area, frame, x, y, c, actual, want[c]);
              goto done;
            }
          }
          ++checks;
        }
        if (argc == 3 && quality == 1 && area == 0 && frame == 8) {
          FILE *out = fopen(argv[2], "wb");
          if (!out)
            goto done;
          fprintf(out, "P6\n%u %u\n255\n", width, height);
          int ok = 1;
          for (unsigned p = 0; p < width * height; ++p)
            if (fwrite(pixels + p * 4, 1, 3, out) != 3) {
              ok = 0;
              break;
            }
          if (fclose(out) || !ok)
            goto done;
        }
        ++frames;
      }
      bk_rain_render_destroy(rain);
      rain = NULL;
      ++profiles;
    }
    bk_renderer_destroy(renderer);
    renderer = NULL;
  }
  assert(colored > 10000);
  printf("PASS rain: %u profiles, %u frames, %u independent pixel samples, "
         "maxRGBAerror%d/255, %llu colored pixels; disabled/absent profiles "
         "and atomic prepare rejection\n",
         profiles, frames, checks, max_error, (unsigned long long)colored);
  rc = 0;
done:
  if (rc)
    fprintf(stderr, "FAIL rain frame%u: %s\n", frames, error);
  bk_rain_render_destroy(rain);
  bk_renderer_destroy(renderer);
  bk_resources_destroy(store);
  bk_blob_free(&blob);
  bk_image_free(&image);
  free(pixels);
  return rc;
}
