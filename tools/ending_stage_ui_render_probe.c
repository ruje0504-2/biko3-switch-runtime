/* Actual51 stage images with diagnostic placements/orders, not full4d499b.
 * Independent triangle interpolation + repeat/bilinear + UNORM blend oracle. */
#include "scene/ending_ui_geometry.h"
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
#include "ending_ui_render_oracle.h"
static int same_alloc(BkRenderStats a, BkRenderStats b) {
  return a.live_allocations == b.live_allocations &&
         a.live_bytes == b.live_bytes;
}
static BkEndingUiSprite *element(BkEndingUi *base, BkEndingStageUi *stage,
                                 unsigned slot) {
  return slot == 8 ? &base->sprites[8] : &stage->sprites[slot - 63];
}
int main(int argc, char **argv) {
  int lines = argc == 4 && !strcmp(argv[3], "--lines");
  if (argc != 3 && !lines)
    return 2;
  char e[256] = {0}, path[1024];
  int result = 1;
  BkRenderer *r = NULL;
  BkResourceStore *store = NULL, *bad = NULL;
  BkEndingUiRender *base_render = NULL, *render = NULL, *future = NULL;
  BkTexture *bg = NULL;
  BkBlob raw = {0};
  BkImage images[75] = {0};
  uint8_t *pixels = malloc(W * H * 4), *redraw = malloc(W * H * 4);
  unsigned profiles = 0, frames = 0, samples = 0, worst = 0, redraws = 0,
           rejections = 0, retained = 0, line_draws = 0;
  uint64_t hash = UINT64_C(14695981039346656037);
  unsigned seen_slots = 0;
  CHECK(pixels && redraw);
  store = bk_resources_create(e);
  CHECK(store);
  snprintf(path, sizeof(path), "%s/bk3_00.pp", argv[1]);
  CHECK(bk_resources_mount(store, "bk3_00", path, e));
  r = bk_renderer_create(W, H, stderr, e);
  CHECK(r);
  const uint8_t color[] = {70, 110, 160, 255};
  BkImage solid = {1, 1, (uint8_t *)color};
  bg = bk_texture_create(r, &solid, e);
  CHECK(bg);
  const BkVertex quad[6] = {
      {-1, -1, 0, 0, 0, 1, 1, 1, 1}, {1, -1, 0, 1, 0, 1, 1, 1, 1},
      {1, 1, 0, 1, 1, 1, 1, 1, 1},   {-1, -1, 0, 0, 0, 1, 1, 1, 1},
      {1, 1, 0, 1, 1, 1, 1, 1, 1},   {-1, 1, 0, 0, 1, 1, 1, 1, 1}};
  BkRenderStats baseline = bk_renderer_stats(r);
  base_render = bk_ending_ui_render_create(r, store, e);
  CHECK(base_render);
  CHECK(bk_resources_read(store, "bk3_00", "hs_100.bmp", &raw, e) ==
        BK_RESOURCE_OK);
  CHECK(bk_image_decode(raw.data, raw.size, &images[52], e));
  bk_blob_free(&raw);
  BkRenderStats base_loaded = bk_renderer_stats(r);
  BkEndingUi base = {0};
  BkEndingStageUi stage = {0};
  for (unsigned kind = 1; kind <= 5; kind++)
    for (unsigned group = 0; group < 5; group++)
      for (unsigned v = 0; v < (kind == 2 ? 2u : 1u); v++) {
        unsigned width = group % 2 ? 503 : 640, height = group % 2 ? 377 : 480;
        BkViewport viewport = {(W - width) / 2, (H - height) / 2, width,
                               height};
        CHECK(bk_ending_stage_ui_initialize(
            &base, &stage, (BkEndingUiStageKind)kind, group, v, width, e));
        unsigned active[13], count = 0;
        for (unsigned slot = 0; slot < 75; slot++) {
          const char *name = bk_ending_stage_ui_image(&stage, slot);
          if (!name)
            continue;
          active[count++] = slot;
          seen_slots |= 1u << (slot == 8 ? 0 : slot - 62);
          CHECK(bk_resources_read(store, "bk3_00", name, &raw, e) ==
                BK_RESOURCE_OK);
          CHECK(bk_image_decode(raw.data, raw.size, &images[slot], e));
          bk_blob_free(&raw);
        }
        CHECK(count);
        render = bk_ending_ui_render_create_stage(r, store, &stage, e);
        CHECK(render);
        BkRenderStats loaded = bk_renderer_stats(r);
        float scroll[2] = {.998f, .001f};
        for (unsigned t = 0; t < 16; t++) {
          BkEndingUiFrame f = {0}, bf = {.count = 2}, all = {0};
          for (unsigned i = 0; i < 2; i++) {
            bf.draws[i] = (BkEndingUiDraw){.slot = 52,
                                           .alpha = i ? .13f : .17f,
                                           .rgb = i ? 0xc0e4b0 : 0xe0bdd0};
            memcpy(bf.draws[i].xy,
                   (float[8]){0, 0, width, 0, width, height, 0, height},
                   sizeof(bf.draws[i].xy));
            memcpy(bf.draws[i].uv, (float[4]){.01f, .02f, .91f, .94f},
                   sizeof(bf.draws[i].uv));
          }
          for (unsigned j = 0; j < count; j++) {
            unsigned slot = active[j];
            BkEndingUiSprite *p = element(&base, &stage, slot);
            CHECK(
                bk_fade_sprite_request(&p->transform.fade, t < 10 || t >= 13));
            if (lines && slot == 63) {
              const int32_t anchors[][2] = {
                  {10, 240}, {500, 350}, {240, 0}, {240, 300}};
              int32_t pointer[2] = {(int32_t)width / 2, (int32_t)height / 2};
              float length =
                  t % 2 ? 120 + t * 11.f : (float)(400.0 * width / 1280);
              CHECK(bk_ending_ui_line(&stage, anchors[t % 4], pointer, length,
                                      .13f, &scroll[t % 2], e));
              line_draws++;
            }
            CHECK(bk_ending_stage_ui_sprite_step(&base, &stage, slot, .13f,
                                                 &f.draws[f.count], e));
            BkEndingUiDraw *d = &f.draws[f.count++];
            if (slot != 74 && !(lines && slot == 63)) {
              BkEndingUiSprite diagnostic = *p;
              diagnostic.rect[0] = (50 + (j % 3) * 200) * width / 640.f;
              diagnostic.rect[1] = (70 + (j / 3) * 135) * height / 480.f;
              diagnostic.rect[2] = 130 * width / 640.f;
              diagnostic.rect[3] = 100 * height / 480.f;
              diagnostic.transform.pivot[0] = diagnostic.transform.pivot[1] =
                  .5f;
              diagnostic.transform.scale[0] = t % 7 == 0 ? -.8f : .8f;
              diagnostic.transform.scale[1] = .9f;
              diagnostic.transform.radians = (t - 7.) * .013;
              CHECK(bk_effect_sprite_quad(&diagnostic.transform,
                                          diagnostic.rect, d->xy));
              memcpy(d->uv, (float[4]){-.11f, .03f, 1.07f, .97f},
                     sizeof(d->uv));
            }
          }
          if (lines && bk_ending_stage_ui_image(&stage, 63)) {
            BkEndingUiDraw before = f.draws[0];
            CHECK(bk_ending_ui_line(
                &stage, (int32_t[2]){400, 20},
                (int32_t[2]){(int32_t)width / 2, (int32_t)height / 2}, 350,
                .13f, &scroll[1], e));
            CHECK(bk_ending_ui_dispatch_sprite(&base, &stage, 63, .13f, &f, e));
            CHECK(!memcmp(&before, &f.draws[0], sizeof(before)));
            line_draws++;
          } else if (count > 1) {
            f.draws[f.count] = f.draws[0];
            for (unsigned j = 0; j < 4; j++)
              f.draws[f.count].xy[j * 2] += width * .3f;
            f.draws[f.count++].alpha *= .61f;
          }
          all.draws[all.count++] = bf.draws[0];
          for (unsigned j = 0; j < f.count; j++)
            all.draws[all.count++] = f.draws[j];
          all.draws[all.count++] = bf.draws[1];
          CHECK(
              bk_ending_ui_render_prepare(base_render, &bf, width, height, e));
          CHECK(bk_ending_ui_render_prepare(render, &f, width, height, e));
          if (t == 15) {
            CHECK(bk_ending_stage_ui_release(&base, &stage,
                                             (BkEndingUiStageKind)kind, e));
            CHECK(bk_ending_stage_ui_initialize(&base, &stage,
                                                (BkEndingUiStageKind)kind,
                                                (group + 1) % 5, v, width, e));
            future = bk_ending_ui_render_create_stage(r, store, &stage, e);
            CHECK(future);
            retained++;
          }
          for (unsigned pass = 0; pass < (t % 8 == 0 ? 2u : 1u); pass++) {
            CHECK(bk_renderer_begin(r, e));
            CHECK(bk_renderer_viewport(r, NULL, e));
            CHECK(bk_renderer_draw(r, bg, quad, 6, bk_identity, e));
            CHECK(bk_renderer_viewport(r, &viewport, e));
            CHECK(bk_ending_ui_render_draw_range(base_render, 0, 1, e));
            CHECK(bk_ending_ui_render_draw(render, e));
            CHECK(bk_ending_ui_render_draw_range(base_render, 1, 1, e));
            CHECK(bk_renderer_end(r, e));
            CHECK(
                bk_renderer_readback(r, pass ? redraw : pixels, W * H * 4, e));
            if (pass) {
              CHECK(!memcmp(pixels, redraw, W * H * 4));
              redraws++;
            }
          }
          if (future) {
            bk_ending_ui_render_destroy(future);
            future = NULL;
          }
          CHECK(same_alloc(loaded, bk_renderer_stats(r)));
          for (unsigned y = 1; y < height; y += 5)
            for (unsigned x = 1; x < width; x += 5) {
              int rgb[3];
              if (!expected(images, &all, x, y, rgb))
                continue;
              for (unsigned c = 0; c < 3; c++) {
                unsigned got =
                    pixels[((size_t)(y + viewport.y) * W + x + viewport.x) * 4 +
                           c];
                unsigned delta = abs((int)got - rgb[c]);
                if (delta > worst)
                  worst = delta;
                if (delta > 2) {
                  snprintf(e, 256, "profile%u tick%u xy%u,%u c%u got%u want%d",
                           profiles, t, x, y, c, got, rgb[c]);
                  goto done;
                }
                hash = (hash ^ got) * UINT64_C(1099511628211);
                samples++;
              }
            }
          frames++;
        }
        CHECK(!bk_ending_ui_render_draw_range(render, 1, UINT32_MAX, e));
        rejections++;
        *e = 0;
        BkEndingUiFrame invalid = {.count = 1,
                                   .draws = {{.slot = 52, .alpha = 1}}};
        CHECK(!bk_ending_ui_render_prepare(render, &invalid, width, height, e));
        rejections++;
        *e = 0;
        CHECK(!bk_ending_ui_render_draw(render, e));
        rejections++;
        *e = 0;
        bk_ending_ui_render_destroy(render);
        render = NULL;
        CHECK(same_alloc(base_loaded, bk_renderer_stats(r)));
        CHECK(bk_ending_stage_ui_release(&base, &stage,
                                         (BkEndingUiStageKind)kind, e));
        for (unsigned j = 0; j < count; j++)
          bk_image_free(&images[active[j]]);
        profiles++;
      }
  CHECK(seen_slots == 0x1fff);
  bad = bk_resources_create(e);
  CHECK(bad);
  CHECK(bk_ending_stage_ui_initialize(&base, &stage, BK_ENDING_UI_AUXILIARY, 0,
                                      0, 640, e));
  CHECK(!bk_ending_ui_render_create_stage(r, bad, &stage, e));
  rejections++;
  *e = 0;
  CHECK(same_alloc(base_loaded, bk_renderer_stats(r)));
  snprintf(path, sizeof(path), "%s/bk3_00.pp", argv[1]);
  CHECK(bk_resources_mount(bad, "bk3_00", path, e));
  (void)mkdir(argv[2], 0700);
  snprintf(path, sizeof(path), "%s/hs_108.tga", argv[2]);
  FILE *file = fopen(path, "wb");
  CHECK(file);
  size_t wrote = fwrite("bad", 1, 3, file);
  int closed = fclose(file);
  CHECK(wrote == 3 && !closed);
  CHECK(bk_resources_mount_directory(bad, "bk3_00", argv[2], 4096, e));
  CHECK(!bk_ending_ui_render_create_stage(r, bad, &stage, e));
  rejections++;
  *e = 0;
  CHECK(same_alloc(base_loaded, bk_renderer_stats(r)));
  stage.loaded |= 0x8000;
  CHECK(!bk_ending_ui_render_create_stage(r, store, &stage, e));
  rejections++;
  *e = 0;
  bk_ending_ui_render_destroy(base_render);
  base_render = NULL;
  CHECK(same_alloc(baseline, bk_renderer_stats(r)));
  printf("ending-stage-ui GPU PASS profiles=%u frames=%u samples=%u worst=%u "
         "redraws=%u retained=%u rejections=%u hash=%016" PRIx64
         " allocation-stable=1\n",
         profiles, frames, samples, worst, redraws, retained, rejections, hash);
  if (lines)
    printf("ending-line GPU PASS draws=%u\n", line_draws);
  result = 0;
done:
  if (result)
    fprintf(stderr, "ending-stage-ui GPU FAIL: %s\n", e);
  bk_ending_ui_render_destroy(future);
  bk_ending_ui_render_destroy(render);
  bk_ending_ui_render_destroy(base_render);
  bk_blob_free(&raw);
  for (unsigned i = 0; i < 75; i++)
    bk_image_free(&images[i]);
  bk_texture_destroy(r, bg);
  bk_renderer_destroy(r);
  bk_resources_destroy(bad);
  bk_resources_destroy(store);
  free(pixels);
  free(redraw);
  return result;
}
