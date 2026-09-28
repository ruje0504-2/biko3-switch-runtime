/* Actual FTT cache/crop and native multipass text compositing on a colored
 * background. Independent bilinear sampler and per-pass UNORM blend model. */
#include "resource/message.h"
#include "scene/text_render.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#define W 640
#define H 240
#define CHECK(x)                                                               \
  do {                                                                         \
    if (!(x))                                                                  \
      goto done;                                                               \
  } while (0)
static double texel(const BkImage *im, int x, int y) {
  if (x < 0)
    x = 0;
  if (y < 0)
    y = 0;
  if (x >= (int)im->width)
    x = (int)im->width - 1;
  if (y >= (int)im->height)
    y = (int)im->height - 1;
  return im->rgba[((size_t)y * im->width + (unsigned)x) * 4] / 255.;
}
static double sample(const BkImage *im, double sx, double sy) {
  int x = (int)floor(sx), y = (int)floor(sy);
  double fx = sx - x, fy = sy - y;
  return (texel(im, x, y) * (1 - fx) + texel(im, x + 1, y) * fx) * (1 - fy) +
         (texel(im, x, y + 1) * (1 - fx) + texel(im, x + 1, y + 1) * fx) * fy;
}
static void expected(const BkImage *im, const BkTextDraw *draw, unsigned x,
                     unsigned y, int out[3]) {
  out[0] = 70;
  out[1] = 110;
  out[2] = 160;
  for (unsigned i = 0; i < draw->count; ++i) {
    const BkTextPass *p = &draw->passes[i];
    if (x < p->x || y < p->y || x >= p->x + p->width || y >= p->y + p->height)
      continue;
    double mask = sample(im, (x - (double)p->x) / p->width * im->width - .5,
                         (y - (double)p->y) / p->height * im->height - .5);
    for (unsigned c = 0; c < 3; ++c) {
      double source = mask * ((p->argb >> (16 - 8 * c)) & 255) / 255.;
      double result = p->blend == BK_TEXT_ADD ? out[c] + source * 255
                                              : out[c] * (1 - source);
      out[c] = (int)lround(result > 255 ? 255 : result);
    }
  }
}
int main(int argc, char **argv) {
  if (argc != 2 && argc != 3)
    return 2;
  char error[256] = {0}, path[1024];
  BkRenderer *renderer = NULL;
  BkResourceStore *store = bk_resources_create(error);
  BkTextRender *text = NULL;
  BkTexture *background = NULL;
  BkBlob blob = {0};
  uint8_t *pixels = malloc(W * H * 4);
  unsigned frames = 0, checks = 0, nonzero = 0, max_error = 0;
  int rc = 1;
  CHECK(store && pixels);
  CHECK(bk_resources_mount_directory(store, "fonts", argv[1], 4 * 1024 * 1024,
                                     error));
  CHECK(snprintf(path, sizeof(path), "%s/bk3_05.pp", argv[1]) <
        (int)sizeof(path));
  CHECK(bk_resources_mount(store, "bk3_05", path, error));
  CHECK(bk_resources_read(store, "bk3_05", "i00_00.txt", &blob, error) ==
        BK_RESOURCE_OK);
  renderer = bk_renderer_create(W, H, stderr, error);
  CHECK(renderer);
  text = bk_text_render_create(renderer, store, "Type_S.FTT", 256, 96, error);
  CHECK(text);
  assert(!bk_text_render_draw(text, error));
  uint8_t color[4] = {70, 110, 160, 255};
  BkImage bg = {1, 1, color};
  background = bk_texture_create(renderer, &bg, error);
  CHECK(background);
  const BkVertex quad[6] = {
      {-1, -1, 0, 0, 0, 1, 1, 1, 1}, {1, -1, 0, 1, 0, 1, 1, 1, 1},
      {1, 1, 0, 1, 1, 1, 1, 1, 1},   {-1, -1, 0, 0, 0, 1, 1, 1, 1},
      {1, 1, 0, 1, 1, 1, 1, 1, 1},   {-1, 1, 0, 0, 1, 1, 1, 1, 1}};
  BkTextFlow flow = {0};
  for (unsigned g = 0; g < 5; ++g)
    for (unsigned item = 0; item < 5; ++item) {
      BkMessage message = {0};
      CHECK(bk_message_lookup(blob.data, blob.size, (int32_t)(g * 10000 + item),
                              &message, error));
      BkTextStyle style = {16, 24, 256, 96, 16, 16, 1, 1, {1, 1, 1}, 1};
      for (unsigned frame = 0; frame < 6; ++frame) {
        if (frame == 0)
          flow = (BkTextFlow){0, 0, 0, 1, 0};
        if (frame == 1) {
          style.opacity = .6f;
          style.color[0] = .25f;
          style.color[1] = .7f;
          style.color[2] = .95f;
          style.shadow_distance = 2.25f;
        }
        if (frame == 2) {
          style.x = 32;
          flow.enabled = 0;
        }
        if (frame == 3) {
          flow = (BkTextFlow){51, 0, 64, 1, 1};
          style.shadow = 0;
        }
        if (frame == 4) {
          style.step_x = 7;
          style.step_y = 7;
          flow.target = 64;
        }
        if (frame == 5) {
          style.opacity = 0;
        }
        unsigned viewport_width = frame >= 3 ? 320 : W;
        unsigned viewport_x = (W - viewport_width) / 2;
        BkViewport viewport = {viewport_x, 0, viewport_width, H};
        CHECK(bk_text_render_prepare(
            text, &style, message.bytes, message.length,
            frame >= 3 ? .25f : 1.f / 60, viewport_width, H, &flow, error));
        CHECK(bk_renderer_begin(renderer, error));
        CHECK(bk_renderer_draw(renderer, background, quad, 6, bk_identity,
                               error));
        CHECK(bk_renderer_viewport(renderer, &viewport, error));
        CHECK(bk_text_render_draw(text, error));
        CHECK(bk_renderer_end(renderer, error));
        CHECK(bk_renderer_readback(renderer, pixels, W * H * 4, error));
        for (unsigned y = 0; y < H; y += 2)
          for (unsigned x = 0; x < W; x += 2) {
            int wanted[3] = {70, 110, 160};
            if (x >= viewport_x && x < viewport_x + viewport_width)
              expected(bk_text_render_image(text),
                       bk_text_render_snapshot(text), x - viewport_x, y,
                       wanted);
            size_t p = ((size_t)y * W + x) * 4;
            for (unsigned c = 0; c < 3; ++c) {
              unsigned diff = (unsigned)abs((int)pixels[p + c] - wanted[c]);
              if (diff > max_error)
                max_error = diff;
              if (diff > 2) {
                snprintf(
                    error, 256,
                    "text g%u i%u frame%u pixel%u,%u ch%u actual%u expected%d",
                    g, item, frame, x, y, c, pixels[p + c], wanted[c]);
                goto done;
              }
            }
            checks++;
            nonzero +=
                pixels[p] != 70 || pixels[p + 1] != 110 || pixels[p + 2] != 160;
          }
        if (argc == 3 && g == 0 && item == 0 && frame == 1) {
          FILE *f = fopen(argv[2], "wb");
          CHECK(f);
          int ok = fprintf(f, "P6\n%d %d\n255\n", W, H) > 0;
          for (unsigned p = 0; p < W * H && ok; ++p)
            ok = fwrite(pixels + p * 4, 1, 3, f) == 3;
          if (fclose(f))
            ok = 0;
          CHECK(ok);
        }
        frames++;
      }
    }
  assert(nonzero);
  printf("PASS native text compositor frames=%u samples=%u changed=%u "
         "max_error=%u/255\n",
         frames, checks, nonzero, max_error);
  rc = 0;
done:
  if (rc)
    fprintf(stderr, "%s\n", error);
  bk_text_render_destroy(text);
  bk_texture_destroy(renderer, background);
  bk_renderer_destroy(renderer);
  bk_resources_destroy(store);
  bk_blob_free(&blob);
  free(pixels);
  return rc;
}
