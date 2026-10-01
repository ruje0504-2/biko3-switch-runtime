/* Real ma_04/ma_05, Type_S FTT and shared ma_01 compositor. The reference
 * independently samples CPU masks and blends; original glyph/geometry/state
 * fidelity is covered by the native oracles, not inferred from this probe. */
#include "scene/curtain_render.h"
#include "scene/dialogue_ui_render.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#define W 640
#define H 480
#define CHECK(v)                                                               \
  do {                                                                         \
    if (!(v)) {                                                                \
      fprintf(stderr, "UI probe line%d: %s (%s)\n", __LINE__, #v, e);          \
      goto done;                                                               \
    }                                                                          \
  } while (0)
static double texel(const BkImage *im, int x, int y, unsigned c, int repeat) {
  int w = im->width, h = im->height;
  if (repeat) {
    x = (x % w + w) % w;
    y = (y % h + h) % h;
  } else {
    if (x < 0)
      x = 0;
    if (y < 0)
      y = 0;
    if (x >= w)
      x = w - 1;
    if (y >= h)
      y = h - 1;
  }
  return im->rgba[((size_t)y * w + x) * 4 + c] / 255.;
}
static double sample(const BkImage *im, double x, double y, unsigned c,
                     int repeat) {
  int l = floor(x), t = floor(y);
  double fx = x - l, fy = y - t;
  return (texel(im, l, t, c, repeat) * (1 - fx) +
          texel(im, l + 1, t, c, repeat) * fx) *
             (1 - fy) +
         (texel(im, l, t + 1, c, repeat) * (1 - fx) +
          texel(im, l + 1, t + 1, c, repeat) * fx) *
             fy;
}
static int near_edge(double px, double py, double x, double y, double w,
                     double h) {
  return px >= x - 1 && px <= x + w + 1 && py >= y - 1 && py <= y + h + 1 &&
         (fabs(px - x) < 1 || fabs(px - x - w) < 1 || fabs(py - y) < 1 ||
          fabs(py - y - h) < 1);
}
static void sprite(const BkImage *im, double x, double y, double w, double h,
                   double px, double py, float alpha, int repeat, int out[3]) {
  if (px < x || py < y || px >= x + w || py >= y + h)
    return;
  double sx = (px - x) / w * im->width - .5,
         sy = (py - y) / h * im->height - .5;
  double a =
      sample(im, sx, sy, 3, repeat) * (unsigned)((double)alpha * 255) / 255.;
  for (unsigned c = 0; c < 3; c++)
    out[c] =
        (int)lround(sample(im, sx, sy, c, repeat) * 255 * a + out[c] * (1 - a));
}
typedef struct {
  BkRenderer *gpu;
  BkDialogueUiRender *ui;
  BkCurtainRender *curtain;
  BkTexture *background;
  BkImage images[3];
  BkViewport viewport;
  uint8_t *pixels;
  unsigned frames, samples, worst;
  uint64_t hash;
} Probe;
static int draw(Probe *q, const BkDialogueUiFrame *f, int prepare,
                char e[256]) {
  static const BkVertex quad[6] = {
      {-1, -1, 0, 0, 0, 1, 1, 1, 1}, {1, -1, 0, 1, 0, 1, 1, 1, 1},
      {1, 1, 0, 1, 1, 1, 1, 1, 1},   {-1, -1, 0, 0, 0, 1, 1, 1, 1},
      {1, 1, 0, 1, 1, 1, 1, 1, 1},   {-1, 1, 0, 0, 1, 1, 1, 1, 1}};
  if (prepare && (!bk_dialogue_ui_render_prepare(q->ui, f, q->viewport.width,
                                                 q->viewport.height, e) ||
                  !bk_curtain_render_prepare(
                      q->curtain, &(BkCommonHudFrame){f->curtain_alpha},
                      q->viewport.width, q->viewport.height, e)))
    return 0;
  if (!bk_renderer_begin(q->gpu, e) ||
      !bk_renderer_draw(q->gpu, q->background, quad, 6, bk_identity, e) ||
      !bk_renderer_viewport(q->gpu, &q->viewport, e) ||
      !bk_dialogue_ui_render_draw(q->ui, e) ||
      !bk_curtain_render_draw(q->curtain, e) || !bk_renderer_end(q->gpu, e) ||
      !bk_renderer_readback(q->gpu, q->pixels, W * H * 4, e))
    return 0;
  BkDialogueUiSprite layout[2];
  assert(bk_dialogue_ui_layout(layout, q->viewport.width));
  const BkImage *text = bk_dialogue_ui_render_text_image(q->ui);
  const BkTextDraw *passes = bk_dialogue_ui_render_text_snapshot(q->ui);
  for (unsigned y = 1; y < H; y += 3)
    for (unsigned x = 1; x < W; x += 3) {
      int out[3] = {70, 110, 160}, skip = 0;
      double px = (double)x - q->viewport.x, py = (double)y - q->viewport.y;
      if (near_edge(px, py, 0, 0, q->viewport.width, q->viewport.height))
        continue;
      if (px >= 0 && py >= 0 && px < q->viewport.width &&
          py < q->viewport.height) {
        for (unsigned i = 0; i < 2; i++) {
          BkDialogueUiSprite *r = &layout[i];
          skip |= near_edge(px, py, r->x, r->y, r->width, r->height);
          sprite(&q->images[i], r->x, r->y, r->width, r->height, px, py,
                 i ? f->prompt_alpha : f->panel_alpha, 1, out);
        }
        for (unsigned i = 0; i < passes->count; i++) {
          const BkTextPass *p = &passes->passes[i];
          skip |= near_edge(px, py, p->x, p->y, p->width, p->height);
          if (px < p->x || py < p->y || px >= p->x + p->width ||
              py >= p->y + p->height)
            continue;
          double mask =
              sample(text, (px - p->x) / p->width * text->width - .5,
                     (py - p->y) / p->height * text->height - .5, 0, 0);
          for (unsigned c = 0; c < 3; c++) {
            double source = mask * ((p->argb >> (16 - c * 8)) & 255) / 255.;
            double value = p->blend == BK_TEXT_ADD ? out[c] + source * 255
                                                   : out[c] * (1 - source);
            out[c] = (int)lround(fmin(255, value));
          }
        }
        double height = q->viewport.width * .75;
        skip |= near_edge(px, py, 0, 0, q->viewport.width, height);
        sprite(&q->images[2], 0, 0, q->viewport.width, height, px, py,
               f->curtain_alpha, 0, out);
      }
      if (skip)
        continue;
      for (unsigned c = 0; c < 3; c++) {
        unsigned diff = abs(q->pixels[((size_t)y * W + x) * 4 + c] - out[c]);
        if (diff > q->worst)
          q->worst = diff;
        if (diff > 3) {
          snprintf(e, 256, "frame%u xy%u,%u ch%u got%u want%d", q->frames, x, y,
                   c, q->pixels[((size_t)y * W + x) * 4 + c], out[c]);
          return 0;
        }
        q->samples++;
      }
    }
  for (unsigned i = 0; i < W * H * 4; i++) {
    q->hash ^= q->pixels[i];
    q->hash *= UINT64_C(1099511628211);
  }
  q->frames++;
  return 1;
}
int main(int argc, char **argv) {
  if (argc < 2 || argc > 3)
    return 2;
  char e[256] = {0}, path[1024];
  int rc = 1;
  Probe q = {.hash = UINT64_C(14695981039346656037)};
  BkResourceStore *store = bk_resources_create(e);
  BkBlob raw = {0}, script = {0};
  uint8_t *saved = malloc(W * H * 4);
  q.pixels = malloc(W * H * 4);
  BkRenderStats baseline = {0};
  CHECK(store && saved && q.pixels);
  const char *packs[] = {"bk3_00", "bk3_05"};
  for (unsigned i = 0; i < 2; i++) {
    snprintf(path, sizeof(path), "%s/%s.pp", argv[1], packs[i]);
    CHECK(bk_resources_mount(store, packs[i], path, e));
  }
  CHECK(bk_resources_mount_directory(store, "fonts", argv[1], 4 * 1024 * 1024,
                                     e));
  if (argc == 3)
    CHECK(bk_resources_load_patch(store, argv[2], e));
  CHECK(bk_resources_read(store, "bk3_05", "i00_00.txt", &script, e) ==
        BK_RESOURCE_OK);
  const char *names[] = {"ma_04.tga", "ma_05.tga", "ma_01.tga"};
  for (unsigned i = 0; i < 3; i++) {
    CHECK(bk_resources_read(store, "bk3_00", names[i], &raw, e) ==
          BK_RESOURCE_OK);
    CHECK(bk_image_decode(raw.data, raw.size, &q.images[i], e));
    bk_blob_free(&raw);
  }
  q.gpu = bk_renderer_create(W, H, stderr, e);
  CHECK(q.gpu);
  baseline = bk_renderer_stats(q.gpu);
  q.ui = bk_dialogue_ui_render_create(q.gpu, store, e);
  q.curtain = bk_curtain_render_create(q.gpu, store, e);
  CHECK(q.ui && q.curtain);
  uint8_t color[] = {70, 110, 160, 255};
  q.background = bk_texture_create(q.gpu, &(BkImage){1, 1, color}, e);
  CHECK(q.background);
  CHECK(!bk_dialogue_ui_render_prepare(q.ui, &(BkDialogueUiFrame){1, 1, 0}, 640,
                                       480, e));
  for (unsigned view = 0; view < 3; view++) {
    q.viewport = (BkViewport[]){
        {0, 0, 640, 480}, {160, 120, 320, 240}, {40, 50, 320, 200}}[view];
    for (unsigned group = 0; group < 5; group++)
      for (unsigned item = 0; item < 5; item++) {
        BkMessage message = {0};
        CHECK(bk_message_lookup(script.data, script.size, group * 10000 + item,
                                &message, e));
        BkTextFlow flow = {0, 0, 0, 1, 0};
        for (unsigned frame = 0; frame < 6; frame++) {
          if (frame == 2)
            flow = (BkTextFlow){51, 0, 32, 1, 1};
          if (frame == 4)
            flow = (BkTextFlow){0, 32, 32, 1, 0};
          BkDialogueUiFrame f = {(float[]){0, .2f, .6f, 1, 1, 1}[frame],
                                 (float[]){1, .7f, .1f, 0, .3f, 1}[frame],
                                 frame == 5 ? .43f : 0};
          CHECK(bk_dialogue_ui_render_text(q.ui, &message, &flow, .1f,
                                           q.viewport.width, q.viewport.height,
                                           e));
          CHECK(draw(&q, &f, 1, e));
          if (frame == 5 && item == 4) {
            memcpy(saved, q.pixels, W * H * 4);
            CHECK(draw(&q, &f, 0, e));
            assert(!memcmp(saved, q.pixels, W * H * 4));
          }
        }
      }
  }
  bk_dialogue_ui_render_destroy(q.ui);
  q.ui = NULL;
  bk_curtain_render_destroy(q.curtain);
  q.curtain = NULL;
  bk_texture_destroy(q.gpu, q.background);
  q.background = NULL;
  BkRenderStats final = bk_renderer_stats(q.gpu);
  assert(final.live_allocations == baseline.live_allocations &&
         final.live_bytes == baseline.live_bytes);
  printf("PASS dialogue UI GPU: %u frames %u RGB samples max error%u/255 RGBA "
         "%016llx; text/alpha/viewport/redraw, owned allocations restored\n",
         q.frames, q.samples, q.worst, (unsigned long long)q.hash);
  rc = 0;
done:
  if (rc)
    fprintf(stderr, "%s\n", e);
  bk_blob_free(&raw);
  bk_blob_free(&script);
  for (unsigned i = 0; i < 3; i++)
    bk_image_free(&q.images[i]);
  bk_dialogue_ui_render_destroy(q.ui);
  bk_curtain_render_destroy(q.curtain);
  bk_texture_destroy(q.gpu, q.background);
  bk_renderer_destroy(q.gpu);
  bk_resources_destroy(store);
  free(saved);
  free(q.pixels);
  return rc;
}
