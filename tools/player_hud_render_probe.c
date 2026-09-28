/* Full native HUD snapshots against independent wrapped bilinear sampling
 * and per-pass8bit blending. Actual assets, all profiles/modes and overlays. */
#include "scene/player_hud_render.h"
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
static double sample(const BkImage *im, double sx, double sy, unsigned c) {
  int x = (int)floor(sx), y = (int)floor(sy);
  double fx = sx - x, fy = sy - y;
  return (texel(im, x, y, c) * (1 - fx) + texel(im, x + 1, y, c) * fx) *
             (1 - fy) +
         (texel(im, x, y + 1, c) * (1 - fx) + texel(im, x + 1, y + 1, c) * fx) *
             fy;
}
static int expected(const BkImage images[49], const BkPlayerHudFrame *f,
                    unsigned x, unsigned y, int out[3]) {
  out[0] = 70;
  out[1] = 110;
  out[2] = 160;
  for (unsigned i = 0; i < f->count; ++i) {
    const BkPlayerHudSprite *s = &f->draws[i].sprite;
    const BkImage *im = &images[f->draws[i].slot];
    double w = (double)s->width * s->sx, h = (double)s->height * s->sy;
    double l = s->x - w * s->px, t = s->y - h * s->py;
    if (w <= 0 || h <= 0 || s->alpha == 0)
      continue;
    /* Avoid raster edge tie rules in the sampling reference; positions and
     * all six native vertices are checked separately by the x86 oracle. */
    if (fabs(x - l) < .02 || fabs(x - l - w) < .02 || fabs(y - t) < .02 ||
        fabs(y - t - h) < .02)
      return 0;
    if (x < l || x >= l + w || y < t || y >= t + h)
      continue;
    double u = s->uv[0] + (x - l) / w * (s->uv[2] - s->uv[0]);
    double v = s->uv[1] + (y - t) / h * (s->uv[3] - s->uv[1]);
    double sx = u * im->width - .5, sy = v * im->height - .5;
    double alpha = sample(im, sx, sy, 3) * (unsigned)(s->alpha * 255.) / 255.;
    for (unsigned c = 0; c < 3; ++c) {
      double source = sample(im, sx, sy, c) * ((s->rgb >> (16 - 8 * c)) & 255);
      out[c] = (int)lround(source * alpha + out[c] * (1 - alpha));
    }
  }
  return 1;
}
static int capture(void *p, char e[256]) {
  (void)e;
  ++*(unsigned *)p;
  return 1;
}
int main(int argc, char **argv) {
  if (argc != 2 && argc != 3)
    return 2;
  char e[256] = {0}, path[1024];
  int rc = 1;
  unsigned frames = 0, samples = 0, worst = 0, captures = 0;
  BkResourceStore *store = bk_resources_create(e);
  BkRenderer *r = NULL;
  BkPlayerHudRender *hud = NULL;
  BkTexture *bg = NULL;
  BkImage images[49] = {{0}};
  BkBlob raw = {0};
  uint8_t *pixels = malloc(W * H * 4);
  CHECK(store && pixels);
  CHECK(snprintf(path, sizeof(path), "%s/bk3_00.pp", argv[1]) <
        (int)sizeof(path));
  CHECK(bk_resources_mount(store, "bk3_00", path, e));
  r = bk_renderer_create(W, H, stderr, e);
  CHECK(r);
  uint8_t color[] = {70, 110, 160, 255};
  BkImage bi = {1, 1, color};
  bg = bk_texture_create(r, &bi, e);
  CHECK(bg);
  const BkVertex quad[6] = {
      {-1, -1, 0, 0, 0, 1, 1, 1, 1}, {1, -1, 0, 1, 0, 1, 1, 1, 1},
      {1, 1, 0, 1, 1, 1, 1, 1, 1},   {-1, -1, 0, 0, 0, 1, 1, 1, 1},
      {1, 1, 0, 1, 1, 1, 1, 1, 1},   {-1, 1, 0, 0, 1, 1, 1, 1, 1}};
  BkPlayerHudCapture service = {&captures, capture};
  BkPlayerHudState s = {0};
  for (unsigned group = 0; group < 5; ++group)
    for (unsigned special = 0; special < 2; ++special) {
      /* A special reload changes group but retains18 previous textures. */
      unsigned g = special ? (group + 1) % 5 : group,
               vw = group % 2 ? 320 : 640, vh = vw * 3 / 4;
      if (!hud)
        hud = bk_player_hud_render_create(r, store, g, special, e);
      else
        CHECK(bk_player_hud_render_reload(hud, store, g, special, e));
      CHECK(hud);
      CHECK(bk_player_hud_initialize(&s, g, special, vw));
      BkPlayerHudLayout layout[49];
      CHECK(bk_player_hud_layout(layout, g, special, vw));
      for (unsigned i = 0; i < 49; ++i)
        if (layout[i].name) {
          bk_image_free(&images[i]);
          CHECK(bk_resources_read(store, "bk3_00", layout[i].name, &raw, e) ==
                BK_RESOURCE_OK);
          CHECK(bk_image_decode(raw.data, raw.size, &images[i], e));
          bk_blob_free(&raw);
        }
      BkViewport vp = {(W - vw) / 2, (H - vh) / 2, vw, vh};
      for (unsigned tick = 0; tick < 32; ++tick) {
        BkPlayerHudInput in = {.special_mode = special,
                               .counter = (int32_t)(tick * 7),
                               .prop_available = 1,
                               .npc_prompt = 1,
                               .cover_available = 1,
                               .npc_in_view = tick > 15};
        for (unsigned i = 0; i < 21; ++i)
          in.actions[i] = (int32_t)i;
        for (unsigned i = 0; i < 5; ++i)
          in.inventory[i] = (tick >> i) & 1;
        in.interface_mode = 2;
        in.trigger_kind = (int32_t[]){10, 11, 18, 19}[tick / 8];
        in.action = tick % 8 < 4 ? (tick / 8 == 1 ? 13 : 11) : 0;
        if (tick % 8 >= 4)
          in.interface_mode = 0;
        BkScreenPoint point = {{(int32_t)vw / 2, (int32_t)vh / 2}, .5f};
        uint8_t outcome = 0;
        BkPlayerHudFrame frame;
        CHECK(bk_player_hud_update(&s, &in, &outcome, &point, .25f,
                                   1000 + tick * 250, vw, e));
        CHECK(bk_player_hud_draws(&s, &in, vw, &frame, e));
        CHECK(bk_player_hud_render_prepare(hud, &frame, vw, vh, e));
        CHECK(bk_renderer_begin(r, e));
        CHECK(bk_renderer_draw(r, bg, quad, 6, bk_identity, e));
        CHECK(bk_renderer_viewport(r, &vp, e));
        CHECK(bk_player_hud_render_draw(hud, &service, e));
        CHECK(bk_renderer_end(r, e));
        CHECK(bk_renderer_readback(r, pixels, W * H * 4, e));
        for (unsigned y = 0; y < H; y += 5)
          for (unsigned x = 0; x < W; x += 7) {
            int wanted[3] = {70, 110, 160};
            if (x >= vp.x && x < vp.x + vw && y >= vp.y && y < vp.y + vh &&
                !expected(images, &frame, x - vp.x, y - vp.y, wanted))
              continue;
            for (unsigned c = 0; c < 3; ++c) {
              unsigned got = pixels[((size_t)y * W + x) * 4 + c],
                       diff = (unsigned)abs((int)got - wanted[c]);
              if (diff > worst)
                worst = diff;
              if (diff > 2) {
                snprintf(e, 256,
                         "pixel g%u mode%u tick%u xy%u,%u c%u: %u expected%d",
                         g, special, tick, x, y, c, got, wanted[c]);
                goto done;
              }
              ++samples;
            }
          }
        if (argc == 3 && group == 2 && !special && tick == 23) {
          FILE *fp = fopen(argv[2], "wb");
          CHECK(fp);
          fprintf(fp, "P6\n%d %d\n255\n", W, H);
          for (unsigned p = 0; p < W * H; ++p)
            CHECK(fwrite(pixels + p * 4, 1, 3, fp) == 3);
          CHECK(!fclose(fp));
        }
        ++frames;
      }
    }
  assert(captures == 160);
  printf("PASS player HUD GPU: %u frames %u pixel channels max%u/255 capture "
         "boundaries%u\n",
         frames, samples, worst, captures);
  rc = 0;
done:
  if (rc)
    fprintf(stderr, "player HUD GPU: %s\n", e);
  bk_player_hud_render_destroy(hud);
  bk_texture_destroy(r, bg);
  bk_renderer_destroy(r);
  for (unsigned i = 0; i < 49; ++i)
    bk_image_free(&images[i]);
  bk_blob_free(&raw);
  bk_resources_destroy(store);
  free(pixels);
  return rc;
}
