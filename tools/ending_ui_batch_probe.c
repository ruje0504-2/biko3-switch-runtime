/* Immutable before/after image owners, shared curtain and repeated slots.
 * Explicit draw fixtures, independent CPU pixels; not an ending playthrough. */
#include "scene/ending_ui_batch.h"
#include <inttypes.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
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
#include "ending_ui_render_oracle.h"
static int same_alloc(BkRenderStats a, BkRenderStats b) {
  return a.live_allocations == b.live_allocations &&
         a.live_bytes == b.live_bytes;
}
static int image(BkResourceStore *s, const char *name, BkImage *out,
                 char e[256]) {
  BkBlob raw = {0};
  if (bk_resources_read(s, "bk3_00", name, &raw, e) != BK_RESOURCE_OK)
    return 0;
  int ok = bk_image_decode(raw.data, raw.size, out, e);
  bk_blob_free(&raw);
  return ok;
}
static BkEndingUiDraw quad(unsigned slot, float x, float y, float w, float h,
                           float alpha) {
  BkEndingUiDraw d = {.slot = slot,
                      .xy = {x, y, x + w, y, x + w, y + h, x, y + h},
                      .uv = {.03f, .02f, .93f, .94f},
                      .alpha = alpha,
                      .rgb = 0xffffff};
  return d;
}
int main(int argc, char **argv) {
  if (argc != 2)
    return 2;
  char e[256] = {0}, path[1024];
  int result = 1;
  BkResourceStore *store = NULL;
  BkRenderer *r = NULL;
  BkTexture *bg = NULL;
  BkCurtainRender *curtain = NULL;
  BkEndingUiBatch *batch = NULL;
  BkEndingUiRender *base = NULL, *before = NULL, *after = NULL,
                   *replacement = NULL;
  BkImage images[75] = {0};
  uint8_t *pixels = malloc(W * H * 4), *again = malloc(W * H * 4);
  unsigned frames = 0, samples = 0, worst = 0, reloads = 0, redraws = 0,
           rejections = 0;
  uint64_t hash = UINT64_C(14695981039346656037);
  CHECK(pixels && again);
  store = bk_resources_create(e);
  CHECK(store);
  snprintf(path, sizeof(path), "%s/bk3_00.pp", argv[1]);
  CHECK(bk_resources_mount(store, "bk3_00", path, e));
  r = bk_renderer_create(W, H, stderr, e);
  CHECK(r);
  uint8_t color[] = {70, 110, 160, 255};
  BkImage solid = {1, 1, color};
  bg = bk_texture_create(r, &solid, e);
  CHECK(bg);
  const BkVertex backdrop[6] = {
      {-1, -1, 0, 0, 0, 1, 1, 1, 1}, {1, -1, 0, 1, 0, 1, 1, 1, 1},
      {1, 1, 0, 1, 1, 1, 1, 1, 1},   {-1, -1, 0, 0, 0, 1, 1, 1, 1},
      {1, 1, 0, 1, 1, 1, 1, 1, 1},   {-1, 1, 0, 0, 1, 1, 1, 1, 1}};
  BkRenderStats baseline = bk_renderer_stats(r);
  curtain = bk_curtain_render_create(r, store, e);
  CHECK(curtain);
  batch = bk_ending_ui_batch_create(curtain, e);
  CHECK(batch);
  CHECK(image(store, bk_ending_ui_image(52), &images[0], e));
  CHECK(image(store, "ma_01.tga", &images[3], e));
  BkRenderStats curtain_only = bk_renderer_stats(r);
  for (unsigned kind = 1; kind <= 5; kind++)
    for (unsigned g = 0; g < 5; g++) {
      unsigned width = g % 2 ? 503 : 640, height = g % 2 ? 377 : 480;
      BkViewport viewport = {(W - width) / 2, (H - height) / 2, width, height};
      BkEndingUi ui = {0};
      BkEndingStageUi stage = {0};
      CHECK(bk_ending_stage_ui_initialize(
          &ui, &stage, (BkEndingUiStageKind)kind, g, 0, width, e));
      unsigned oldslot = kind == 5 ? 74 : 63;
      CHECK(bk_ending_stage_ui_image(&stage, oldslot));
      CHECK(image(store, bk_ending_stage_ui_image(&stage, oldslot), &images[1],
                  e));
      base = bk_ending_ui_render_create(r, store, e);
      CHECK(base);
      before = bk_ending_ui_render_create_stage(r, store, &stage, e);
      CHECK(before);
      for (unsigned tick = 0; tick < 8; tick++) {
        int swap = tick == 7;
        CHECK(bk_ending_ui_batch_begin(batch, base, before, e));
        if (swap) {
          /* Release application references BEFORE preparing old snapshots. */
          bk_ending_ui_render_destroy(before);
          before = NULL;
          CHECK(bk_ending_stage_ui_release(&ui, &stage,
                                           (BkEndingUiStageKind)kind, e));
          CHECK(bk_ending_stage_ui_initialize(
              &ui, &stage, (BkEndingUiStageKind)kind, (g + 1) % 5,
              kind == 2 ? 1 : 0, width, e));
          after = bk_ending_ui_render_create_stage(r, store, &stage, e);
          CHECK(after);
          CHECK(image(store, bk_ending_stage_ui_image(&stage, oldslot),
                      &images[2], e));
          if (g % 2) {
            bk_ending_ui_render_destroy(base);
            base = NULL;
            replacement = bk_ending_ui_render_create(r, store, e);
            CHECK(replacement);
          }
          reloads++;
        }
        BkEndingUiCompositeFrame frame = {
            .complete = 1,
            .curtain_after = 3,
            .curtain = {.curtain_alpha = (float)(tick % 4) * .21f}};
        frame.sprites.count = 6;
        frame.sprites.draws[0] = quad(52, 0, 0, width, height, .13f);
        frame.sprites.draws[1] =
            quad(oldslot, 20, 30, width * .75f, height * .7f, .85f);
        frame.sprites.draws[2] =
            quad(52, 40, 60, width * .6f, height * .55f, .24f);
        frame.sprites.draws[3] =
            quad(52, 10, 20, width * .8f, height * .8f, .15f);
        frame.sprites.draws[4] =
            quad(oldslot, 70, 100, width * .6f, height * .55f, .7f);
        frame.sprites.draws[5] =
            quad(52, 110, 90, width * .35f, height * .45f, .11f);
        BkEndingUiFrame oracle = {0};
        for (unsigned i = 0; i <= frame.sprites.count; i++) {
          if (i == frame.curtain_after) {
            oracle.draws[oracle.count] =
                quad(3, 0, 0, width,
                     (float)(960.0 * ((float)((double)width / 1280))),
                     frame.curtain.curtain_alpha);
            memcpy(oracle.draws[oracle.count++].uv, (float[4]){0, 0, 1, 1},
                   sizeof(float) * 4);
          }
          if (i < frame.sprites.count) {
            BkEndingUiDraw d = frame.sprites.draws[i];
            d.slot =
                d.slot == 52 ? 0 : (i < frame.curtain_after || !swap ? 1 : 2);
            oracle.draws[oracle.count++] = d;
          }
        }
        BkEndingUiRender *newbase = replacement ? replacement : base,
                         *newstage = after ? after : before;
        CHECK(bk_ending_ui_batch_prepare(batch, &frame, newbase, newstage,
                                         width, height, e));
        BkRenderStats prepared = bk_renderer_stats(r);
        for (unsigned pass = 0; pass < 2; pass++) {
          CHECK(bk_renderer_begin(r, e));
          CHECK(bk_renderer_viewport(r, NULL, e));
          CHECK(bk_renderer_draw(r, bg, backdrop, 6, bk_identity, e));
          CHECK(bk_renderer_viewport(r, &viewport, e));
          CHECK(bk_ending_ui_batch_draw(batch, e));
          CHECK(bk_renderer_end(r, e));
          CHECK(bk_renderer_readback(r, pass ? again : pixels, W * H * 4, e));
          if (pass) {
            CHECK(!memcmp(pixels, again, W * H * 4));
            redraws++;
          }
        }
        CHECK(same_alloc(prepared, bk_renderer_stats(r)));
        for (unsigned y = 1; y < height; y += 7)
          for (unsigned x = 1; x < width; x += 7) {
            int rgb[3];
            if (!expected(images, &oracle, x, y, rgb))
              continue;
            for (unsigned c = 0; c < 3; c++) {
              unsigned got =
                  pixels[((size_t)(y + viewport.y) * W + x + viewport.x) * 4 +
                         c];
              unsigned delta = abs((int)got - rgb[c]);
              if (delta > worst)
                worst = delta;
              if (delta > 2) {
                snprintf(e, 256,
                         "kind%u group%u tick%u xy%u,%u c%u got%u want%d", kind,
                         g, tick, x, y, c, got, rgb[c]);
                goto done;
              }
              hash = (hash ^ got) * UINT64_C(1099511628211);
              samples++;
            }
          }
        /* Invalid CPU completion/boundary/image/extent must poison ready. */
        for (unsigned failure = 0; failure < 5; failure++) {
          BkEndingUiCompositeFrame bad = frame;
          if (failure == 0)
            bad.complete = 0;
          if (failure == 1)
            bad.curtain_after = bad.sprites.count + 1;
          if (failure == 2)
            bad.early_return = 1;
          if (failure == 3)
            bad.sprites.draws[4].slot = 75;
          CHECK(!bk_ending_ui_batch_prepare(batch, &bad, newbase, newstage,
                                            failure == 4 ? 0 : width, height,
                                            e));
          CHECK(!bk_ending_ui_batch_draw(batch, e));
          *e = 0;
          rejections++;
        }
        CHECK(bk_ending_ui_batch_prepare(batch, &frame, newbase, newstage,
                                         width, height, e));
        /* Early return pins prefix even when all current owners were released.
         */
        if (swap) {
          frame.early_return = 1;
          frame.sprites.count = frame.curtain_after;
          bk_ending_ui_render_destroy(after);
          after = NULL;
          bk_ending_ui_render_destroy(replacement);
          replacement = NULL;
          bk_ending_ui_render_destroy(base);
          base = NULL;
          CHECK(bk_ending_ui_batch_prepare(batch, &frame, NULL, NULL, width,
                                           height, e));
          CHECK(bk_renderer_begin(r, e));
          CHECK(bk_ending_ui_batch_draw(batch, e));
          CHECK(bk_renderer_end(r, e));
        }
        frames++;
      }
      bk_ending_ui_batch_clear(batch);
      CHECK(!bk_ending_ui_batch_draw(batch, e));
      *e = 0;
      rejections++;
      CHECK(same_alloc(curtain_only, bk_renderer_stats(r)));
      bk_image_free(&images[1]);
      bk_image_free(&images[2]);
    }
  bk_ending_ui_batch_destroy(batch);
  batch = NULL;
  bk_curtain_render_destroy(curtain);
  curtain = NULL;
  CHECK(same_alloc(baseline, bk_renderer_stats(r)));
  printf("ending UI batch GPU PASS frames=%u samples=%u worst=%u redraws=%u "
         "reloads=%u rejections=%u hash=%016" PRIx64 " allocation-stable=1\n",
         frames, samples, worst, redraws, reloads, rejections, hash);
  result = 0;
done:
  if (result)
    fprintf(stderr, "ending UI batch GPU FAIL: %s\n", e);
  bk_ending_ui_batch_destroy(batch);
  bk_ending_ui_render_destroy(after);
  bk_ending_ui_render_destroy(before);
  bk_ending_ui_render_destroy(replacement);
  bk_ending_ui_render_destroy(base);
  bk_curtain_render_destroy(curtain);
  for (unsigned i = 0; i < 75; i++)
    bk_image_free(&images[i]);
  bk_texture_destroy(r, bg);
  bk_renderer_destroy(r);
  bk_resources_destroy(store);
  free(pixels);
  free(again);
  return result;
}
