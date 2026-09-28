/* Real loading art and captured flow snapshots, independently sampled/blended.
 * Loader/confirm callbacks here only record their CPU service boundary. */
#include "scene/flow_loading_render.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#define W 640
#define H 480
#define CHECK(x)                                                               \
  do {                                                                         \
    if (!(x))                                                                  \
      goto done;                                                               \
  } while (0)
static double texel(const BkImage *im, int x, int y, unsigned c) {
  int w = (int)im->width, h = (int)im->height;
  x = (x % w + w) % w;
  y = (y % h + h) % h;
  return im->rgba[((size_t)y * w + x) * 4 + c] / 255.;
}
static double sample(const BkImage *im, double x, double y, unsigned c) {
  int l = (int)floor(x), t = (int)floor(y);
  double fx = x - l, fy = y - t;
  return (texel(im, l, t, c) * (1 - fx) + texel(im, l + 1, t, c) * fx) *
             (1 - fy) +
         (texel(im, l, t + 1, c) * (1 - fx) + texel(im, l + 1, t + 1, c) * fx) *
             fy;
}
static int expected(const BkImage im[5], const BkFlowLoadingFrame *f,
                    unsigned w, unsigned h, unsigned x, unsigned y,
                    int out[3]) {
  out[0] = 70;
  out[1] = 110;
  out[2] = 160;
  for (unsigned i = 0; i < f->count; ++i) {
    const BkFlowLoadingDraw *d = &f->draws[i];
    float sx = (float)((double)w / 1024), sy = (float)((double)h / 768);
    float l = 0, t = 0, width = (float)(1280.0 * (float)((double)w / 1280));
    float height = (float)(960.0 * (float)((double)w / 1280));
    if (d->asset >= BK_LOADING_PROMPT) {
      l = 976 * sx;
      t = 720 * sy;
      width = 48 * sx;
      height = 48 * sy;
    }
    if (fabs(x - l) < .02 || fabs(y - t) < .02 || fabs(x - l - width) < .02 ||
        fabs(y - t - height) < .02)
      return 0;
    if (x < l || x >= l + width || y < t || y >= t + height)
      continue;
    const BkImage *image = &im[d->asset];
    double u = (x - l) / width * image->width - .5,
           v = (y - t) / height * image->height - .5;
    double alpha = sample(image, u, v, 3) * (unsigned)(d->alpha * 255.) / 255.;
    for (unsigned c = 0; c < 3; ++c)
      out[c] = (int)lround(sample(image, u, v, c) * 255 * alpha +
                           out[c] * (1 - alpha));
  }
  return 1;
}
static int load(void *p, uint8_t target, char e[256]) {
  (void)e;
  (void)target;
  ++*(unsigned *)p;
  return 1;
}
static int confirm(void *p, char e[256]) {
  (void)e;
  ++*(unsigned *)p;
  return 1;
}
int main(int argc, char **argv) {
  if (argc != 2 && argc != 3)
    return 2;
  char e[256] = {0}, path[1024];
  int rc = 1;
  BkResourceStore *store = bk_resources_create(e);
  BkRenderer *r = NULL;
  BkFlowLoadingRender *loading = NULL;
  BkTexture *bg = NULL;
  BkImage images[5] = {{0}};
  BkBlob raw = {0};
  uint8_t *pixels = malloc(W * H * 4);
  unsigned calls = 0, frames = 0, samples = 0, worst = 0;
  CHECK(store && pixels);
  snprintf(path, sizeof(path), "%s/bk3_00.pp", argv[1]);
  CHECK(bk_resources_mount(store, "bk3_00", path, e));
  for (unsigned i = 0; i < 5; ++i) {
    CHECK(bk_resources_read(store, "bk3_00",
                            bk_flow_loading_image((BkFlowLoadingAsset)i), &raw,
                            e) == BK_RESOURCE_OK);
    CHECK(bk_image_decode(raw.data, raw.size, &images[i], e));
    bk_blob_free(&raw);
  }
  r = bk_renderer_create(W, H, stderr, e);
  CHECK(r);
  loading = bk_flow_loading_render_create(r, store, 1, e);
  CHECK(loading);
  uint8_t color[] = {70, 110, 160, 255};
  BkImage solid = {1, 1, color};
  bg = bk_texture_create(r, &solid, e);
  CHECK(bg);
  const BkVertex quad[6] = {
      {-1, -1, 0, 0, 0, 1, 1, 1, 1}, {1, -1, 0, 1, 0, 1, 1, 1, 1},
      {1, 1, 0, 1, 1, 1, 1, 1, 1},   {-1, -1, 0, 0, 0, 1, 1, 1, 1},
      {1, 1, 0, 1, 1, 1, 1, 1, 1},   {-1, 1, 0, 0, 1, 1, 1, 1, 1}};
  BkFlowLoadingOps ops = {&calls, load, confirm};
  for (unsigned variant = 0; variant < 4; ++variant) {
    unsigned vw = (unsigned[]){640, 512, 320, 503}[variant],
             vh = (unsigned[]){480, 384, 240, 367}[variant];
    BkViewport vp = {(W - vw) / 2, (H - vh) / 2, vw, vh};
    for (unsigned tick = 0; tick < 40; ++tick) {
      BkFlowLoadingState state = {0};
      bk_flow_loading_initialize(&state, 1);
      state.background =
          (BkFadeSprite){.alpha = (tick % 7) / 7.f, .speed = 2, .stage = 1};
      state.special_background = state.background;
      state.prompt_base = (BkFadeSprite){.alpha = 1, .speed = 2, .stage = 3};
      state.prompt = (BkPulseSprite){
          {.alpha = (tick % 5) / 5.f, .speed = 2, .stage = 3}, tick % 2};
      state.awaiting = 1;
      BkCommonHudState common = {
          .curtain = {.alpha = (tick % 9) / 9.f, .speed = 2, .stage = 5}};
      BkFlowTransition flow = {0x50, 0x38, 2, 1};
      if (tick % 4 == 0)
        flow.mode = 0;
      unsigned special = tick % 3 ? 1 : 0;
      BkFlowLoadingFrame frame;
      CHECK(bk_flow_loading_step(&state, &common, &flow, (uint8_t)special,
                                 tick % 7 == 0, .05f, &ops, &frame, e));
      CHECK(bk_flow_loading_render_prepare(loading, &frame, vw, vh, e));
      CHECK(bk_renderer_begin(r, e));
      CHECK(bk_renderer_draw(r, bg, quad, 6, bk_identity, e));
      CHECK(bk_renderer_viewport(r, &vp, e));
      CHECK(bk_flow_loading_render_draw(loading, e));
      CHECK(bk_renderer_end(r, e));
      CHECK(bk_renderer_readback(r, pixels, W * H * 4, e));
      for (unsigned y = 1; y < vh; y += 2)
        for (unsigned x = 1; x < vw; x += 2) {
          int want[3];
          if (!expected(images, &frame, vw, vh, x, y, want))
            continue;
          for (unsigned c = 0; c < 3; ++c) {
            unsigned got = pixels[((size_t)(y + vp.y) * W + x + vp.x) * 4 + c];
            unsigned diff = (unsigned)abs((int)got - want[c]);
            if (diff > worst)
              worst = diff;
            if (diff > 1) {
              snprintf(e, sizeof(e), "frame%u pixel%u,%u ch%u %u!=%d", frames,
                       x, y, c, got, want[c]);
              goto done;
            }
            ++samples;
          }
        }
      ++frames;
      if (argc == 3 && variant == 0 && tick == 38) {
        FILE *out = fopen(argv[2], "wb");
        CHECK(out);
        size_t n = fwrite(pixels, 1, W * H * 4, out);
        int closed = fclose(out);
        CHECK(n == W * H * 4 && !closed);
      }
    }
  }
  printf(
      "PASS loading GPU frames=%u channels=%u max-error=%u/255 callbacks=%u\n",
      frames, samples, worst, calls);
  rc = 0;
done:
  if (rc)
    fprintf(stderr, "FAIL loading GPU: %s\n", e);
  bk_flow_loading_render_destroy(loading);
  bk_texture_destroy(r, bg);
  bk_renderer_destroy(r);
  for (unsigned i = 0; i < 5; ++i)
    bk_image_free(&images[i]);
  bk_blob_free(&raw);
  bk_resources_destroy(store);
  free(pixels);
  return rc;
}
