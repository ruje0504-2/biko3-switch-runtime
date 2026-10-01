/* Real pickup services -> message -> phase1 notice and native FTT display.
 * GPU RGB is checked against independent bilinear/per-pass UNORM blending. */
#include "scene/curtain_render.h"
#include "scene/item_feedback.h"
#include "scene/item_notice_render.h"
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
  if (x < 0)
    x = 0;
  if (y < 0)
    y = 0;
  if (x >= (int)im->width)
    x = (int)im->width - 1;
  if (y >= (int)im->height)
    y = (int)im->height - 1;
  return im->rgba[((size_t)y * im->width + (unsigned)x) * 4 + c] / 255.;
}
static double sample(const BkImage *im, double sx, double sy, unsigned c) {
  int x = (int)floor(sx), y = (int)floor(sy);
  double fx = sx - x, fy = sy - y;
  return (texel(im, x, y, c) * (1 - fx) + texel(im, x + 1, y, c) * fx) *
             (1 - fy) +
         (texel(im, x, y + 1, c) * (1 - fx) + texel(im, x + 1, y + 1, c) * fx) *
             fy;
}
static int inside(unsigned x, unsigned y, float px, float py, float w,
                  float h) {
  return x >= px && y >= py && x < px + w && y < py + h;
}
static void expected(const BkImage images[7],
                     const BkItemNoticeSprite layout[7],
                     const BkItemNoticeState *state,
                     const BkItemNoticeRender *notice,
                     const BkPulseSprite *prompt, unsigned mask, unsigned x,
                     unsigned y, int out[3]) {
  out[0] = 70;
  out[1] = 110;
  out[2] = 160;
  for (unsigned i = 0; i < 7; ++i) {
    if (!(mask & (1u << i)))
      continue;
    const BkItemNoticeSprite *p = &layout[i];
    const BkImage *im = &images[i];
    if (!inside(x, y, p->x, p->y, p->width, p->height))
      continue;
    double sx = (x - (double)p->x) / p->width * im->width - .5;
    double sy = (y - (double)p->y) / p->height * im->height - .5;
    double opacity = (int)((i == 6 ? prompt->fade.alpha
                            : i    ? state->icons[i - 1].alpha
                                   : state->panel.alpha) *
                           255.) /
                     255.;
    double alpha = sample(im, sx, sy, 3) * opacity;
    for (unsigned c = 0; c < 3; ++c)
      out[c] = (int)lround(sample(im, sx, sy, c) * 255 * alpha +
                           out[c] * (1 - alpha));
  }
  if (!bk_item_notice_render_frame(notice)->draw_text)
    return;
  const BkTextRender *text = bk_item_notice_render_text(notice);
  const BkTextDraw *draw = bk_text_render_snapshot(text);
  const BkImage *im = bk_text_render_image(text);
  for (unsigned i = 0; i < draw->count; ++i) {
    const BkTextPass *p = &draw->passes[i];
    if (!inside(x, y, p->x, p->y, p->width, p->height))
      continue;
    double mask = sample(im, (x - (double)p->x) / p->width * im->width - .5,
                         (y - (double)p->y) / p->height * im->height - .5, 0);
    for (unsigned c = 0; c < 3; ++c) {
      double source = mask * ((p->argb >> (16 - 8 * c)) & 255) / 255.;
      double value = p->blend == BK_TEXT_ADD ? out[c] + source * 255
                                             : out[c] * (1 - source);
      out[c] = (int)lround(value > 255 ? 255 : value);
    }
  }
}
typedef struct {
  uint64_t submitted;
} Sink;
static int submit(void *ctx, const int16_t *pcm, size_t frames, char *error) {
  (void)pcm;
  (void)error;
  ((Sink *)ctx)->submitted += frames;
  return 1;
}
static int poll(void *ctx, uint64_t *consumed, char *error) {
  (void)error;
  *consumed = ((Sink *)ctx)->submitted;
  return 1;
}
int main(int argc, char **argv) {
  if (argc < 2 || argc > 4)
    return 2;
  char error[256] = {0}, path[1024];
  int rc = 1;
  BkResourceStore *store = bk_resources_create(error);
  BkRenderer *renderer = NULL;
  BkCurtainRender *curtain = NULL;
  BkItemNoticeRender *notice = NULL;
  BkItemFeedback *feedback = NULL;
  BkAudio *audio = NULL;
  BkTexture *background = NULL;
  BkBlob blob = {0};
  BkDialogueAssets dialogue = {0};
  BkImage images[7] = {{0}};
  uint8_t *pixels = malloc(W * H * 4);
  uint8_t *retained_pixels = malloc(W * H * 4);
  unsigned frames = 0, samples = 0, changed = 0, max_error = 0, pickups = 0;
  CHECK(store && pixels && retained_pixels);
  CHECK(bk_resources_mount_directory(store, "fonts", argv[1], 4 * 1024 * 1024,
                                     error));
  const char *packs[] = {"bk3_00", "bk3_02", "bk3_05"};
  for (unsigned i = 0; i < 3; ++i) {
    CHECK(snprintf(path, sizeof(path), "%s/%s.pp", argv[1], packs[i]) <
          (int)sizeof(path));
    CHECK(bk_resources_mount(store, packs[i], path, error));
  }
  if (argc == 4)
    CHECK(bk_resources_load_patch(store, argv[3], error));
  Sink sink = {0};
  BkAudioSink output = {&sink, 22050, 147, 588, submit, poll};
  audio = bk_audio_create(&output, error);
  CHECK(audio);
  feedback = bk_item_feedback_create(store, audio, 2, 0, error);
  CHECK(feedback);
  BkItemPickupOps ops = bk_item_feedback_ops(feedback);
  renderer = bk_renderer_create(W, H, stderr, error);
  CHECK(renderer);
  uint8_t color[] = {70, 110, 160, 255};
  BkImage bg = {1, 1, color};
  background = bk_texture_create(renderer, &bg, error);
  CHECK(background);
  const BkVertex quad[6] = {
      {-1, -1, 0, 0, 0, 1, 1, 1, 1}, {1, -1, 0, 1, 0, 1, 1, 1, 1},
      {1, 1, 0, 1, 1, 1, 1, 1, 1},   {-1, -1, 0, 0, 0, 1, 1, 1, 1},
      {1, 1, 0, 1, 1, 1, 1, 1, 1},   {-1, 1, 0, 0, 1, 1, 1, 1, 1}};
  for (unsigned group = 0; group < 5; ++group) {
    if (!notice)
      notice = bk_item_notice_render_create(renderer, store, group, error);
    else
      CHECK(bk_item_notice_render_reload(notice, store, group, 0, error));
    CHECK(notice);
    assert(!bk_item_notice_render_draw(notice, error));
    unsigned vw = group % 2 ? 320 : 640, vh = vw * 3 / 4, vx = (W - vw) / 2,
             vy = (H - vh) / 2;
    BkViewport viewport = {vx, vy, vw, vh};
    BkItemNoticeSprite layout[7];
    CHECK(bk_item_notice_layout(layout, group, vw));
    for (unsigned i = 0; i < 6; ++i) {
      bk_image_free(&images[i]);
      CHECK(bk_resources_read(store, "bk3_00", layout[i].name, &blob, error) ==
            BK_RESOURCE_OK);
      CHECK(bk_image_decode(blob.data, blob.size, &images[i], error));
      bk_blob_free(&blob);
    }
    for (unsigned item = 0; item < 5; ++item) {
      BkItemNoticeState state = {0};
      BkItemPickupState pickup = {0};
      BkTextFlow flow = {0};
      CHECK(bk_item_notice_initialize(&state, 0));
      uint32_t now = 1000;
      for (unsigned frame = 0; frame < 12; ++frame) {
        if (frame == 6)
          now += 5000;
        else
          now += 250;
        if (frame == 1 || frame == 3 || frame == 7) {
          BkItemState slots[16] = {{0}};
          slots[0].id = (item + (frame == 3) + (frame == 7) * 2) % 5;
          const float current[3] = {-10, 0, 0}, previous[3] = {10, 0, 0};
          BkItemPickups hits;
          CHECK(bk_item_pickup_run(slots, 1, &pickup, group, current, previous,
                                   &ops, &hits, error));
          CHECK(bk_audio_poll(audio, error) && bk_audio_fill(audio, error));
          assert(hits.count == 1);
          pickups++;
        }
        CHECK(bk_item_notice_render_prepare(
            notice, &state, &pickup, bk_item_feedback_message(feedback), &flow,
            frame == 11 ? 1 : .25f, now, vw, vh, error));
        CHECK(bk_renderer_begin(renderer, error));
        CHECK(bk_renderer_draw(renderer, background, quad, 6, bk_identity,
                               error));
        CHECK(bk_renderer_viewport(renderer, &viewport, error));
        CHECK(bk_item_notice_render_draw(notice, error));
        CHECK(bk_renderer_end(renderer, error));
        CHECK(bk_renderer_readback(renderer, pixels, W * H * 4, error));
        for (unsigned y = 0; y < H; y += 3)
          for (unsigned x = 0; x < W; x += 3) {
            int wanted[3] = {70, 110, 160};
            if (x >= vx && y >= vy && x < vx + vw && y < vy + vh)
              expected(images, layout, &state, notice, NULL, 0x3f, x - vx,
                       y - vy, wanted);
            size_t p = ((size_t)y * W + x) * 4;
            for (unsigned c = 0; c < 3; ++c) {
              unsigned diff = (unsigned)abs((int)pixels[p + c] - wanted[c]);
              if (diff > max_error)
                max_error = diff;
              if (diff > 2) {
                snprintf(error, 256,
                         "notice g%u i%u frame%u pixel%u,%u ch%u actual%u "
                         "expected%d",
                         group, item, frame, x, y, c, pixels[p + c], wanted[c]);
                goto done;
              }
            }
            samples++;
            changed +=
                pixels[p] != 70 || pixels[p + 1] != 110 || pixels[p + 2] != 160;
          }
        if (argc >= 3 && group == 0 && item == 0 && frame == 11) {
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
        if (item == 4 && frame == 11) {
          memcpy(retained_pixels, pixels, W * H * 4);
          /* Retained icon resources must remain visible after changing the
           * requested group; a failed reload also preserves prepared draw. */
          CHECK(bk_item_notice_render_reload(notice, store, (group + 1) % 5, 1,
                                             error));
          CHECK(bk_item_notice_render_prepare(
              notice, &state, &pickup, bk_item_feedback_message(feedback),
              &flow, 0, now, vw, vh, error));
          for (unsigned retry = 0; retry < 2; ++retry) {
            if (retry) {
              BkResourceStore *empty = bk_resources_create(error);
              CHECK(empty);
              assert(!bk_item_notice_render_reload(notice, empty, group, 0,
                                                   error));
              bk_resources_destroy(empty);
            }
            CHECK(bk_renderer_begin(renderer, error));
            CHECK(bk_renderer_draw(renderer, background, quad, 6, bk_identity,
                                   error));
            CHECK(bk_renderer_viewport(renderer, &viewport, error));
            CHECK(bk_item_notice_render_draw(notice, error));
            CHECK(bk_renderer_end(renderer, error));
            CHECK(bk_renderer_readback(renderer, pixels, W * H * 4, error));
            assert(memcmp(pixels, retained_pixels, W * H * 4) == 0);
            frames++;
          }
        }
      }
    }
  }
  BkNoticeTextOps text_ops = bk_item_notice_render_text_ops(notice);
  CHECK(bk_resources_read(store, "bk3_00", "ma_05.tga", &blob, error) ==
        BK_RESOURCE_OK);
  CHECK(bk_image_decode(blob.data, blob.size, &images[6], error));
  bk_blob_free(&blob);
  unsigned opening_frames = 0;
  for (unsigned group = 0; group < 5; ++group) {
    CHECK(bk_item_notice_render_reload(notice, store, group, 0, error));
    BkItemNoticeState state = {0};
    BkPulseSprite prompt = {0};
    BkTextFlow flow = {0};
    CHECK(bk_item_notice_initialize(&state, 0));
    bk_pulse_sprite_initialize(&prompt);
    CHECK(text_ops.recreate(text_ops.context, BK_NOTICE_TEXT_OPENING, error));
    char filename[32];
    snprintf(filename, sizeof(filename), "i0%u_01.txt", group + 1);
    dialogue.state.first_label = (int32_t)(group + 1) * 10000;
    dialogue.state.last_label =
        (int32_t[]){10005, 20006, 30007, 40011, 50007}[group];
    CHECK(bk_dialogue_assets_load(&dialogue, store, filename, error));
    int complete;
    CHECK(bk_dialogue_assets_next(&dialogue, &complete, error));
    CHECK(text_ops.bind(text_ops.context, &dialogue.state.text, error));
    flow.enabled = 1;
    unsigned vw = group % 2 ? 320 : 640, vh = vw * 3 / 4, vx = (W - vw) / 2,
             vy = (H - vh) / 2;
    BkViewport viewport = {vx, vy, vw, vh};
    BkItemNoticeSprite layout[7];
    CHECK(bk_item_notice_layout(layout, group, vw));
    float scale = (float)((double)vw / 1280);
    layout[6] = (BkItemNoticeSprite){"ma_05.tga", 1097 * scale, 890 * scale,
                                     40 * scale, 40 * scale};
    for (unsigned frame = 0; frame < 24; ++frame) {
      uint8_t phase = frame < 20 ? 0 : frame < 22 ? 2 : 3;
      uint8_t visible = frame < 13;
      if (frame == 5 || frame == 9) {
        CHECK(bk_dialogue_assets_next(&dialogue, &complete, error));
        flow.scroll = flow.target;
        flow.started = 0;
        flow.enabled = 1;
      }
      if (frame == 16) {
        CHECK(text_ops.clear(text_ops.context, error));
        CHECK(text_ops.recreate(text_ops.context, BK_NOTICE_TEXT_ITEMS, error));
      }
      CHECK(bk_item_notice_render_prepare_opening(
          notice, &state, &prompt, phase, visible, &flow, .25f, vw, vh, error));
      CHECK(bk_renderer_begin(renderer, error));
      CHECK(
          bk_renderer_draw(renderer, background, quad, 6, bk_identity, error));
      CHECK(bk_renderer_viewport(renderer, &viewport, error));
      CHECK(bk_item_notice_render_draw(notice, error));
      CHECK(bk_renderer_end(renderer, error));
      CHECK(bk_renderer_readback(renderer, pixels, W * H * 4, error));
      for (unsigned y = 0; y < H; y += 3)
        for (unsigned x = 0; x < W; x += 3) {
          int wanted[3] = {70, 110, 160};
          if (x >= vx && y >= vy && x < vx + vw && y < vy + vh)
            expected(images, layout, &state, notice, &prompt,
                     phase == 0   ? 0x41
                     : phase == 2 ? 1
                                  : 0,
                     x - vx, y - vy, wanted);
          size_t p = ((size_t)y * W + x) * 4;
          for (unsigned c = 0; c < 3; ++c) {
            unsigned diff = (unsigned)abs((int)pixels[p + c] - wanted[c]);
            if (diff > max_error)
              max_error = diff;
            if (diff > 2) {
              snprintf(
                  error, 256,
                  "opening g%u frame%u pixel%u,%u ch%u actual%u expected%d",
                  group, frame, x, y, c, pixels[p + c], wanted[c]);
              bk_dialogue_assets_close(&dialogue);
              goto done;
            }
          }
          samples++;
        }
      if (argc >= 3 && group == 0 && frame == 5) {
        char filename[1024];
        snprintf(filename, sizeof(filename), "%s-opening.ppm", argv[2]);
        FILE *f = fopen(filename, "wb");
        CHECK(f);
        int ok = fprintf(f, "P6\n%d %d\n255\n", W, H) > 0;
        for (unsigned p = 0; p < W * H && ok; ++p)
          ok = fwrite(pixels + p * 4, 1, 3, f) == 3;
        if (fclose(f))
          ok = 0;
        CHECK(ok);
      }
      opening_frames++;
    }
    bk_dialogue_assets_close(&dialogue);
  }
  printf("PASS opening notice frames=%u\n", opening_frames);
  curtain = bk_curtain_render_create(renderer, store, error);
  CHECK(curtain);
  unsigned curtain_frames = 0;
  for (unsigned variant = 0; variant < 12; ++variant) {
    unsigned vw = variant % 2 ? 320 : 640, vh = vw * 3 / 4, vx = (W - vw) / 2,
             vy = (H - vh) / 2;
    BkViewport viewport = {vx, vy, vw, vh};
    BkCommonHudFrame frame = {.curtain_alpha = (float[]){
                                  0, 1, .001f, .005f, .5f, .999f}[variant / 2]};
    CHECK(bk_curtain_render_prepare(curtain, &frame, vw, vh, error));
    CHECK(bk_renderer_begin(renderer, error));
    CHECK(bk_renderer_draw(renderer, background, quad, 6, bk_identity, error));
    CHECK(bk_renderer_viewport(renderer, &viewport, error));
    CHECK(bk_curtain_render_draw(curtain, error));
    CHECK(bk_renderer_end(renderer, error));
    CHECK(bk_renderer_readback(renderer, pixels, W * H * 4, error));
    int alpha = (int)((double)frame.curtain_alpha * 255);
    for (unsigned y = 0; y < H; y += 3)
      for (unsigned x = 0; x < W; x += 3) {
        int base[3] = {70, 110, 160};
        for (unsigned c = 0; c < 3; ++c) {
          int wanted = base[c];
          if (x >= vx && y >= vy && x < vx + vw && y < vy + vh)
            wanted = (int)lround(base[c] * (255 - alpha) / 255.);
          unsigned diff =
              (unsigned)abs((int)pixels[((size_t)y * W + x) * 4 + c] - wanted);
          if (diff > max_error)
            max_error = diff;
          if (diff > 1) {
            snprintf(error, 256, "curtain variant%u pixel%u,%u ch%u", variant,
                     x, y, c);
            goto done;
          }
        }
        samples++;
      }
    curtain_frames++;
  }
  printf("PASS curtain frames=%u\n", curtain_frames);
  assert(changed && pickups == 75);
  printf("PASS item notice frames=%u samples=%u changed=%u pickups=%u "
         "max_error=%u/255\n",
         frames, samples, changed, pickups, max_error);
  rc = 0;
done:
  if (rc)
    fprintf(stderr, "%s\n", error);
  bk_dialogue_assets_close(&dialogue);
  bk_item_notice_render_destroy(notice);
  bk_texture_destroy(renderer, background);
  bk_curtain_render_destroy(curtain);
  bk_renderer_destroy(renderer);
  bk_item_feedback_destroy(feedback);
  bk_audio_destroy(audio);
  bk_resources_destroy(store);
  bk_blob_free(&blob);
  for (unsigned i = 0; i < 7; ++i)
    bk_image_free(&images[i]);
  free(pixels);
  free(retained_pixels);
  return rc;
}
