/* Actual62 ending images with diagnostic placements/orders, not full4d499b.
 * Independent triangle interpolation + repeat/bilinear + UNORM blend oracle. */
#include "scene/ending_ui_render.h"
#include <inttypes.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#define W 640
#define H 480
#define CHECK(x)                                                               \
  do {                                                                         \
    if (!(x)) {                                                                \
      if (!*e)                                                                 \
        snprintf(e, 256, "line%d: %s", __LINE__, #x);                          \
      goto done;                                                               \
    }                                                                          \
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
static int coords(const BkEndingUiDraw *d, unsigned x, unsigned y,
                  double out[2]) {
  const unsigned triangles[2][3] = {{0, 1, 2}, {3, 0, 2}};
  const double uv[4][2] = {{d->uv[0], d->uv[1]},
                           {d->uv[2], d->uv[1]},
                           {d->uv[2], d->uv[3]},
                           {d->uv[0], d->uv[3]}};
  for (unsigned i = 0; i < 2; ++i) {
    const unsigned *ix = triangles[i];
    double ax = d->xy[2 * ix[0]], ay = d->xy[2 * ix[0] + 1],
           bx = d->xy[2 * ix[1]] - ax, by = d->xy[2 * ix[1] + 1] - ay,
           cx = d->xy[2 * ix[2]] - ax, cy = d->xy[2 * ix[2] + 1] - ay;
    double det = bx * cy - by * cx;
    if (fabs(det) < 1e-12)
      continue;
    double b = ((x - ax) * cy - (y - ay) * cx) / det,
           c = (bx * (y - ay) - by * (x - ax)) / det, a = 1 - b - c;
    if (a < -.001 || b < -.001 || c < -.001)
      continue;
    /* Leave subpixel raster edge inclusivity to the renderer tests. */
    if (a < .001 || b < .001 || c < .001)
      return -1;
    for (unsigned k = 0; k < 2; ++k)
      out[k] = a * uv[ix[0]][k] + b * uv[ix[1]][k] + c * uv[ix[2]][k];
    return 1;
  }
  return 0;
}
static int expected(const BkImage im[63], const BkEndingUiFrame *f, unsigned x,
                    unsigned y, int rgb[3]) {
  rgb[0] = 70;
  rgb[1] = 110;
  rgb[2] = 160;
  for (unsigned i = 0; i < f->count; ++i) {
    const BkEndingUiDraw *d = &f->draws[i];
    double uv[2];
    int hit = coords(d, x, y, uv);
    if (hit < 0)
      return 0;
    if (!hit)
      continue;
    const BkImage *p = &im[d->slot];
    double u = uv[0] * p->width - .5, v = uv[1] * p->height - .5;
    double alpha = sample(p, u, v, 3) * (unsigned)(d->alpha * 255.) / 255.;
    for (unsigned c = 0; c < 3; ++c) {
      unsigned tint = (d->rgb >> (16 - 8 * c)) & 255;
      rgb[c] =
          (int)lround(sample(p, u, v, c) * tint * alpha + rgb[c] * (1 - alpha));
    }
  }
  return 1;
}
static int held(void *ctx, unsigned key, unsigned mode, uint32_t *out,
                char e[256]) {
  (void)ctx;
  (void)e;
  if (mode != 2 || key > 1)
    return 0;
  *out = 0;
  return 1;
}
static int same_alloc(BkRenderStats a, BkRenderStats b) {
  return a.live_allocations == b.live_allocations &&
         a.live_bytes == b.live_bytes;
}
int main(int argc, char **argv) {
  if (argc != 3)
    return 2;
  char e[256] = {0}, path[1024];
  int result = 1;
  BkRenderer *r = NULL;
  BkResourceStore *store = bk_resources_create(e), *bad = NULL;
  BkEndingUiRender *render = NULL;
  BkTexture *bg = NULL;
  BkImage images[63] = {{0}};
  BkBlob raw = {0};
  uint8_t *pixels = malloc(W * H * 4), *redraw = malloc(W * H * 4);
  unsigned frames = 0, samples = 0, worst = 0, redraws = 0, rejections = 0;
  uint64_t seen = 0, hash = UINT64_C(14695981039346656037);
  CHECK(store && pixels && redraw);
  snprintf(path, sizeof(path), "%s/bk3_00.pp", argv[1]);
  CHECK(bk_resources_mount(store, "bk3_00", path, e));
  r = bk_renderer_create(W, H, stderr, e);
  CHECK(r);
  uint8_t color[] = {70, 110, 160, 255};
  BkImage solid = {1, 1, color};
  bg = bk_texture_create(r, &solid, e);
  CHECK(bg);
  const BkVertex quad[6] = {
      {-1, -1, 0, 0, 0, 1, 1, 1, 1}, {1, -1, 0, 1, 0, 1, 1, 1, 1},
      {1, 1, 0, 1, 1, 1, 1, 1, 1},   {-1, -1, 0, 0, 0, 1, 1, 1, 1},
      {1, 1, 0, 1, 1, 1, 1, 1, 1},   {-1, 1, 0, 0, 1, 1, 1, 1, 1}};
  for (unsigned i = 0; i < 63; ++i) {
    const char *name = bk_ending_ui_image(i);
    if (!name)
      continue;
    CHECK(bk_resources_read(store, "bk3_00", name, &raw, e) == BK_RESOURCE_OK);
    CHECK(bk_image_decode(raw.data, raw.size, &images[i], e));
    bk_blob_free(&raw);
  }
  BkRenderStats baseline = bk_renderer_stats(r);
  render = bk_ending_ui_render_create(r, store, e);
  CHECK(render);
  BkRenderStats loaded = bk_renderer_stats(r);
  for (unsigned variant = 0; variant < 2; ++variant) {
    unsigned width = variant ? 503 : 640, height = variant ? 377 : 480;
    BkViewport viewport = {(W - width) / 2, (H - height) / 2, width, height};
    BkEndingUi s = {0};
    uint8_t flags[6];
    float gauge;
    CHECK(bk_ending_ui_initialize(&s, width, flags, &gauge, e));
    int32_t open = 0, request = 1;
    BkEndingUiHoverOps ops = {NULL, held};
    for (unsigned tick = 0; tick < 120; ++tick) {
      float pointer[2] = {(tick % 50 < 30 ? 1200.f : 1000.f) * width / 1280,
                          20};
      uint8_t visible;
      CHECK(bk_ending_ui_hover(&s, &open, &request, pointer, width / 1280.f,
                               .1f, &ops, &visible, e));
      BkEndingUiDraw states[63];
      for (unsigned i = 0; i < 63; ++i) {
        if (i == 8)
          continue;
        CHECK(bk_fade_sprite_request(&s.sprites[i].transform.fade,
                                     (tick % 50) < 35));
        CHECK(bk_ending_ui_sprite_step(&s, i, .1f, &states[i], e));
      }
      BkEndingUiFrame f = {0};
      /* Visible separate panels exercise every real texture without implying
       * this diagnostic grid is an implemented ending game screen. */
      for (unsigned j = 0; j < 6; ++j) {
        unsigned slot = (tick + j * 11) % 63;
        if (slot == 8)
          slot = 9;
        BkEndingUiSprite p = s.sprites[slot];
        p.rect[0] = (40 + (j % 3) * 190) * width / 640.f;
        p.rect[1] = (80 + (j / 3) * 220) * height / 480.f;
        p.rect[2] = 140 * width / 640.f;
        p.rect[3] = 100 * height / 480.f;
        p.transform.pivot[0] = p.transform.pivot[1] = .5f;
        p.transform.scale[0] = (tick % 13 == 0 ? -.7f : .7f);
        p.transform.scale[1] = .9f;
        p.transform.radians = (float)((tick % 17 - 8.0) * .035);
        BkEndingUiDraw d = {.slot = slot,
                            .alpha = (tick % 7 + 1) / 8.f,
                            .rgb = j % 2 ? 0xc6e4ad : 0xffffff};
        memcpy(d.uv, (float[4]){-.13f, .07f, .87f, 1.03f}, sizeof(d.uv));
        CHECK(bk_effect_sprite_quad(&p.transform, p.rect, d.xy));
        f.draws[f.count++] = d;
        seen |= UINT64_C(1) << slot;
      }
      /* Two snapshots of one actual animated prompt must not alias a mesh. */
      f.draws[f.count] = states[53 + tick % 4];
      ++f.count;
      BkEndingUiDraw copy = f.draws[f.count - 1];
      for (unsigned i = 0; i < 4; ++i)
        copy.xy[2 * i] -= width * .35f;
      copy.alpha *= .63f;
      f.draws[f.count++] = copy;
      CHECK(bk_ending_ui_render_prepare(render, &f, width, height, e));
      BkEndingUi before = s;
      for (unsigned pass = 0; pass < (tick % 30 == 0 ? 2u : 1u); ++pass) {
        CHECK(bk_renderer_begin(r, e));
        CHECK(bk_renderer_viewport(r, NULL, e));
        CHECK(bk_renderer_draw(r, bg, quad, 6, bk_identity, e));
        CHECK(bk_renderer_viewport(r, &viewport, e));
        CHECK(bk_ending_ui_render_draw(render, e));
        CHECK(bk_renderer_end(r, e));
        CHECK(bk_renderer_readback(r, pass ? redraw : pixels, W * H * 4, e));
        if (pass) {
          CHECK(!memcmp(pixels, redraw, W * H * 4) &&
                !memcmp(&s, &before, sizeof(s)));
          ++redraws;
        }
      }
      CHECK(same_alloc(loaded, bk_renderer_stats(r)));
      for (unsigned y = 1; y < height; y += 4)
        for (unsigned x = 1; x < width; x += 4) {
          int rgb[3];
          if (!expected(images, &f, x, y, rgb))
            continue;
          for (unsigned c = 0; c < 3; ++c) {
            unsigned actual =
                pixels[((size_t)(y + viewport.y) * W + x + viewport.x) * 4 + c];
            unsigned delta = abs((int)actual - rgb[c]);
            if (delta > worst)
              worst = delta;
            if (delta > 2) {
              snprintf(e, 256, "pixel v%u tick%u xy%u,%u c%u got%u want%d",
                       variant, tick, x, y, c, actual, rgb[c]);
              goto done;
            }
            hash ^= actual;
            hash *= UINT64_C(1099511628211);
            ++samples;
          }
        }
      ++frames;
      if (tick == 60) {
        BkEndingUiFrame invalid = f;
        invalid.draws[0].slot = 8;
        CHECK(!bk_ending_ui_render_prepare(render, &invalid, width, height, e));
        ++rejections;
        CHECK(!bk_ending_ui_render_draw(render, e));
        ++rejections;
        *e = 0;
        invalid = f;
        invalid.draws[0].xy[3] = NAN;
        CHECK(!bk_ending_ui_render_prepare(render, &invalid, width, height, e));
        ++rejections;
        *e = 0;
        invalid = f;
        invalid.count = BK_ENDING_UI_DRAWS + 1;
        CHECK(!bk_ending_ui_render_prepare(render, &invalid, width, height, e));
        ++rejections;
        *e = 0;
      }
    }
  }
  CHECK(seen == (((UINT64_C(1) << 63) - 1) & ~(UINT64_C(1) << 8)));
  bk_ending_ui_render_destroy(render);
  render = NULL;
  CHECK(same_alloc(baseline, bk_renderer_stats(r)));
  bad = bk_resources_create(e);
  CHECK(bad);
  CHECK(!bk_ending_ui_render_create(r, bad, e));
  ++rejections;
  *e = 0;
  CHECK(same_alloc(baseline, bk_renderer_stats(r)));
  snprintf(path, sizeof(path), "%s/bk3_00.pp", argv[1]);
  CHECK(bk_resources_mount(bad, "bk3_00", path, e));
  /* Late damaged image exercises cleanup after most textures already exist. */
  (void)mkdir(argv[2], 0700);
  snprintf(path, sizeof(path), "%s/lo_18.tga", argv[2]);
  FILE *file = fopen(path, "wb");
  CHECK(file);
  size_t wrote = fwrite("bad", 1, 3, file);
  int closed = fclose(file);
  CHECK(wrote == 3 && !closed);
  CHECK(bk_resources_mount_directory(bad, "bk3_00", argv[2], 4096, e));
  CHECK(!bk_ending_ui_render_create(r, bad, e));
  ++rejections;
  *e = 0;
  CHECK(same_alloc(baseline, bk_renderer_stats(r)));
  printf("ending-ui GPU PASS images=62 frames=%u samples=%u worst=%u "
         "redraws=%u rejections=%u hash=%016" PRIx64 " allocation-stable=1\n",
         frames, samples, worst, redraws, rejections, hash);
  result = 0;
done:
  if (result)
    fprintf(stderr, "ending-ui GPU FAIL: %s\n", e);
  bk_ending_ui_render_destroy(render);
  bk_blob_free(&raw);
  for (unsigned i = 0; i < 63; ++i)
    bk_image_free(&images[i]);
  bk_texture_destroy(r, bg);
  bk_renderer_destroy(r);
  bk_resources_destroy(bad);
  bk_resources_destroy(store);
  free(pixels);
  free(redraw);
  return result;
}
