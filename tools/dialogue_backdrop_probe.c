/* Actual bk3_00 images and native background state transitions. The actor's
 * hidden acknowledgement is an explicit fixture; this is not full flow8. */
#include "scene/dialogue_backdrop_render.h"
#include <inttypes.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#define W 640
#define H 480
#define CHECK(x)                                                               \
  do {                                                                         \
    if (!(x)) {                                                                \
      fprintf(stderr, "dialogue backdrop line%d: %s (%s)\n", __LINE__, #x, e); \
      goto done;                                                               \
    }                                                                          \
  } while (0)
static const char *names[] = {"SG00000.bmp", "SG00001.bmp", "SG10000.bmp",
                              "g01_10.bmp",  "SG20000.bmp", "SG50000.bmp",
                              "ma_02.tga"};
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
typedef struct {
  BkRenderer *gpu;
  BkDialogueBackdropRender *render;
  BkImage images[7];
  BkViewport vp;
  uint8_t *pixels;
  unsigned current, frames, samples, worst, replacements;
  uint64_t hash;
} Probe;
static int replace(void *p, const char *name, char e[256]) {
  Probe *q = p;
  unsigned i = 0;
  while (i < 6 && strcmp(name, names[i]))
    i++;
  if (i == 6 || !bk_dialogue_backdrop_render_replace(q->render, name, e))
    return 0;
  q->current = i;
  q->replacements++;
  return 1;
}
static int draw(Probe *q, const BkDialogueBackdropFrame *f, int prepare,
                char e[256]) {
  if ((prepare && !bk_dialogue_backdrop_render_prepare(
                      q->render, f, q->vp.width, q->vp.height, e)) ||
      !bk_renderer_begin(q->gpu, e) ||
      !bk_renderer_viewport(q->gpu, &q->vp, e) ||
      !bk_dialogue_backdrop_render_draw(q->render, e) ||
      !bk_renderer_end(q->gpu, e) ||
      !bk_renderer_readback(q->gpu, q->pixels, W * H * 4, e))
    return 0;
  for (unsigned y = 3; y < H; y += 7)
    for (unsigned x = 3; x < W; x += 7) {
      double px = (double)x - q->vp.x, py = (double)y - q->vp.y;
      int out[3] = {0};
      if (px >= 1 && py >= 1 && px < q->vp.width - 1 && py < q->vp.height - 1) {
        double height = q->vp.width * .75;
        if (py < height)
          for (unsigned i = 0; i < 2; i++) {
            const BkImage *im = &q->images[i ? 6 : q->current];
            double sx = px / q->vp.width * im->width - .5,
                   sy = py / height * im->height - .5;
            double a =
                sample(im, sx, sy, 3) *
                (unsigned)((double)(i ? f->curtain_alpha : f->image_alpha) *
                           255) /
                255.;
            for (unsigned c = 0; c < 3; c++)
              out[c] = (int)lround(sample(im, sx, sy, c) * 255 * a +
                                   out[c] * (1 - a));
          }
      } else if (px >= 0 && py >= 0 && px < q->vp.width && py < q->vp.height)
        continue;
      for (unsigned c = 0; c < 3; c++) {
        int d = abs(q->pixels[((size_t)y * W + x) * 4 + c] - out[c]);
        if ((unsigned)d > q->worst)
          q->worst = d;
        if (d > 2) {
          snprintf(e, 256, "frame%u image%u xy%u,%u c%u got%u expected%d",
                   q->frames, q->current, x, y, c,
                   q->pixels[((size_t)y * W + x) * 4 + c], out[c]);
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
  if (argc != 2)
    return 2;
  char e[256] = {0}, path[1024];
  int result = 1;
  Probe q = {.hash = UINT64_C(14695981039346656037)};
  BkResourceStore *store = bk_resources_create(e);
  BkBlob raw = {0};
  BkRenderStats baseline = {0};
  uint8_t *saved = malloc(W * H * 4);
  q.pixels = malloc(W * H * 4);
  CHECK(store && saved && q.pixels);
  snprintf(path, sizeof(path), "%s/bk3_00.pp", argv[1]);
  CHECK(bk_resources_mount(store, "bk3_00", path, e));
  for (unsigned i = 0; i < 7; i++) {
    CHECK(bk_resources_read(store, "bk3_00", names[i], &raw, e) ==
          BK_RESOURCE_OK);
    CHECK(bk_image_decode(raw.data, raw.size, &q.images[i], e));
    bk_blob_free(&raw);
  }
  q.gpu = bk_renderer_create(W, H, stderr, e);
  CHECK(q.gpu);
  baseline = bk_renderer_stats(q.gpu);
  const BkViewport views[] = {
      {0, 0, W, H}, {160, 120, 320, 240}, {40, 50, 320, 200}};
  for (unsigned v = 0; v < 3; v++) {
    q.vp = views[v];
    q.current = 0;
    q.render = bk_dialogue_backdrop_render_create(q.gpu, store, names[0], e);
    CHECK(q.render);
    CHECK(!bk_dialogue_backdrop_render_draw(q.render, e));
    for (unsigned im = 0; im < 6; im++) {
      if (im)
        CHECK(replace(&q, names[im], e));
      for (unsigned a = 0; a < 12; a++) {
        BkDialogueBackdropFrame f = {(a % 4) / 3.f, (a / 4) / 2.f, 0};
        CHECK(draw(&q, &f, 1, e));
        if (a == 7) {
          memcpy(saved, q.pixels, W * H * 4);
          CHECK(!bk_dialogue_backdrop_render_replace(
              q.render, "missing-dialogue-image.bmp", e));
          CHECK(draw(&q, &f, 0, e));
          CHECK(!memcmp(saved, q.pixels, W * H * 4));
          BkDialogueBackdropFrame invalid = f;
          invalid.image_alpha = NAN;
          CHECK(!bk_dialogue_backdrop_render_prepare(
              q.render, &invalid, q.vp.width, q.vp.height, e));
          CHECK(draw(&q, &f, 0, e) && !memcmp(saved, q.pixels, W * H * 4));
        }
      }
    }
    bk_dialogue_backdrop_render_destroy(q.render);
    q.render = NULL;
  }
  for (unsigned hz_index = 0; hz_index < 2; hz_index++) {
    unsigned hz = hz_index ? 53 : 15;
    q.vp = views[hz_index];
    q.current = 0;
    q.render = bk_dialogue_backdrop_render_create(q.gpu, store, names[0], e);
    CHECK(q.render);
    BkDialogueBackdrop s = {{1, 2, 3}, 5, 0, 0, 1};
    BkFadeSprite curtain = {0, 2, 0};
    uint8_t phase = 0, wanted = 0;
    int32_t expression = 5;
    BkDialogueBackdropOps ops = {&q, replace};
    BkDialogueBackdropBindings b = {&phase, &expression, &curtain, &wanted,
                                    NULL};
    for (unsigned change = 1; change <= 3; change++) {
      b.image = names[change];
      phase = 1;
      s.saved_expression = 5;
      s.image_kind = change == 3;
      unsigned frame = 0;
      do {
        if (phase == 1 && expression == -1)
          phase = 2; /* Explicit actor acknowledgement. */
        BkDialogueBackdropFrame f;
        CHECK(bk_dialogue_backdrop_step(&s, &b, &ops, 1.f / hz, &f, e));
        CHECK(draw(&q, &f, 1, e));
        frame++;
      } while (phase && frame < hz * 8);
      CHECK(!phase && expression == 5 && frame > 2);
    }
    bk_dialogue_backdrop_render_destroy(q.render);
    q.render = NULL;
  }
  CHECK(bk_renderer_stats(q.gpu).live_allocations ==
            baseline.live_allocations &&
        bk_renderer_stats(q.gpu).live_bytes == baseline.live_bytes);
  printf("PASS dialogue backdrop GPU: %u frames %u replacements %u RGB samples "
         "max error %u/255 RGBA %016" PRIx64
         "; missing-image/redraw preservation, allocation baseline restored\n",
         q.frames, q.replacements, q.samples, q.worst, q.hash);
  result = 0;
done:
  bk_blob_free(&raw);
  bk_dialogue_backdrop_render_destroy(q.render);
  bk_renderer_destroy(q.gpu);
  bk_resources_destroy(store);
  for (unsigned i = 0; i < 7; i++)
    bk_image_free(&q.images[i]);
  free(saved);
  free(q.pixels);
  return result;
}
