/* Real Japanese title images, bg001 PCM and original menu CPU controller.
 * Release is an explicit endpoint observer: this probe does not load the
 * six destination scenes or claim natural new-game/mission completion. */
#include "scene/system_audio.h"
#include "scene/title_menu_render.h"
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
typedef struct {
  uint64_t submitted, consumed, samples, nonzero, hash;
} Sink;
static int submit(void *p, const int16_t *pcm, size_t frames, char e[256]) {
  (void)e;
  Sink *s = p;
  for (size_t i = 0; i < frames * 2; ++i) {
    ++s->samples;
    s->nonzero += pcm[i] != 0;
    uint16_t value = (uint16_t)pcm[i];
    for (unsigned b = 0; b < 2; ++b) {
      s->hash ^= (value >> (b * 8)) & 255;
      s->hash *= UINT64_C(1099511628211);
    }
  }
  s->submitted += frames;
  return 1;
}
static int poll(void *p, uint64_t *consumed, char e[256]) {
  (void)e;
  *consumed = ((Sink *)p)->consumed;
  return 1;
}
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
static int expected(const BkImage im[15], const BkTitleMenuFrame *f, unsigned x,
                    unsigned y, int out[3]) {
  out[0] = 70;
  out[1] = 110;
  out[2] = 160;
  for (unsigned i = 0; i < f->count; ++i) {
    const BkTitleMenuDraw *d = &f->draws[i];
    double l = d->corners[0], t = d->corners[1], width = d->corners[2] - l,
           height = d->corners[3] - t;
    /* Raster edge inclusivity is not the sampler/blend oracle's subject. */
    if (fabs(x - l) < .03 || fabs(y - t) < .03 || fabs(x - l - width) < .03 ||
        fabs(y - t - height) < .03)
      return 0;
    if (x < l || x >= l + width || y < t || y >= t + height)
      continue;
    const BkImage *image = &im[d->slot];
    double u = (x - l) / width * image->width - .5,
           v = (y - t) / height * image->height - .5;
    double alpha = sample(image, u, v, 3) * (unsigned)(d->alpha * 255.) / 255.;
    for (unsigned c = 0; c < 3; ++c)
      out[c] = (int)lround(sample(image, u, v, c) * 255 * alpha +
                           out[c] * (1 - alpha));
  }
  return 1;
}

typedef struct {
  float point[2], relative[2];
  BkAudio *audio;
  BkSystemAudio *sounds[4];
  unsigned releases;
} Services;
static int warp(void *p, float x, float y, char e[256]) {
  (void)e;
  Services *s = p;
  s->point[0] = x;
  s->point[1] = y;
  return 1;
}
static int position(void *p, float out[2], char e[256]) {
  (void)e;
  memcpy(out, ((Services *)p)->point, 2 * sizeof(float));
  return 1;
}
static int motion(void *p, float out[2], char e[256]) {
  (void)e;
  memcpy(out, ((Services *)p)->relative, 2 * sizeof(float));
  return 1;
}
static int sound(void *p, unsigned slot, char e[256]) {
  Services *s = p;
  if (slot >= 4 || !s->sounds[slot]) {
    snprintf(e, 256, "unexpected sound%u", slot);
    return 0;
  }
  return bk_system_audio_restart(s->sounds[slot], e);
}
static int gain(void *p, int32_t volume, char e[256]) {
  return bk_audio_gain(((Services *)p)->audio, 60, volume, 0, e);
}
static int release(void *p, uint8_t flow, char e[256]) {
  Services *s = p;
  if (flow != 1) {
    snprintf(e, 256, "unexpected flow%u", flow);
    return 0;
  }
  ++s->releases;
  return bk_audio_clear(s->audio, 60, e);
}
int main(int argc, char **argv) {
  if (argc != 3)
    return 2;
  char e[256] = {0}, path[1024];
  int result = 1;
  BkResourceStore *store = bk_resources_create(e);
  BkRenderer *r = NULL;
  BkTitleMenuRender *render = NULL;
  BkAudio *audio = NULL;
  BkAudioClip *music = NULL;
  BkTexture *background = NULL;
  BkImage images[15] = {{0}};
  BkBlob raw = {0};
  uint8_t *pixels = malloc(W * H * 4), *redraw = malloc(W * H * 4);
  Sink sink = {.hash = UINT64_C(14695981039346656037)};
  Services services = {0};
  unsigned frames = 0, samples = 0, worst = 0, redraws = 0;
  CHECK(store && pixels && redraw);
  const char *packs[] = {"bk3_00", "bk3_02"};
  for (unsigned i = 0; i < 2; ++i) {
    snprintf(path, sizeof(path), "%s/%s.pp", argv[1], packs[i]);
    CHECK(bk_resources_mount(store, packs[i], path, e));
  }
  r = bk_renderer_create(W, H, stderr, e);
  CHECK(r);
  BkAudioSink output = {&sink, 48000, 240, 960, submit, poll};
  audio = bk_audio_create(&output, e);
  CHECK(audio);
  services.audio = audio;
  music = bk_audio_clip_load(store, "bk3_02", "bg001.wav", e);
  CHECK(music);
  for (unsigned i = 1; i <= 3; i += 2) {
    services.sounds[i] =
        bk_system_audio_create_slot(store, audio, 48 + i, i, -600, e);
    CHECK(services.sounds[i]);
  }
  uint8_t color[] = {70, 110, 160, 255};
  BkImage solid = {1, 1, color};
  background = bk_texture_create(r, &solid, e);
  CHECK(background);
  const BkVertex quad[6] = {
      {-1, -1, 0, 0, 0, 1, 1, 1, 1}, {1, -1, 0, 1, 0, 1, 1, 1, 1},
      {1, 1, 0, 1, 1, 1, 1, 1, 1},   {-1, -1, 0, 0, 0, 1, 1, 1, 1},
      {1, 1, 0, 1, 1, 1, 1, 1, 1},   {-1, 1, 0, 0, 1, 1, 1, 1, 1}};
  for (unsigned variant = 0; variant < 2; ++variant) {
    unsigned width = variant ? 503 : 640, height = variant ? 377 : 480;
    BkViewport viewport = {(W - width) / 2, (H - height) / 2, width, height};
    for (unsigned special = 0; special < 1; ++special) {
      for (unsigned i = 0; i < 15; ++i) {
        bk_image_free(&images[i]);
        const char *name = bk_title_menu_image(i, (uint8_t)special);
        if (!name)
          continue;
        CHECK(bk_resources_read(store, "bk3_00", name, &raw, e) ==
              BK_RESOURCE_OK);
        CHECK(bk_image_decode(raw.data, raw.size, &images[i], e));
        bk_blob_free(&raw);
      }
      render = bk_title_menu_render_create(r, store, (uint8_t)special, e);
      CHECK(render);
      for (unsigned target = 1; target <= 11; target += 2) {
        if (special && target != 1 && target != 9)
          continue;
        BkTitleMenuState state = {0};
        BkCommonHudState common = {.curtain = {0, 2, 0}};
        BkMenuCursor cursor = {0};
        CHECK(bk_menu_cursor_initialize(&cursor, width, height));
        BkFlowTransition flow = {1, 0, 0, 0};
        uint8_t latch = 0;
        BkTitleMenuBindings bind = {&common, &flow, &cursor, &latch};
        BkTitleMenuOps ops = {&services, sound,  gain,   warp,
                              position,  motion, release};
        CHECK(bk_audio_play(audio, 60, music, 1, -900, 0, e));
        CHECK(bk_title_menu_initialize(&state, width, (uint8_t)special, -900,
                                       &ops, e));
        for (unsigned tick = 0; tick < 40; ++tick) {
          if (tick == 23) {
            services.point[0] = state.sprites[target].rect[0];
            services.point[1] = state.sprites[target].rect[1];
          }
          BkTitleMenuInput input = {tick == 24 ? BK_PAUSE_CONFIRM : 0,
                                    frames * 250,
                                    .25f,
                                    width / 1280.f,
                                    -900,
                                    (uint8_t)special};
          BkTitleMenuFrame frame;
          sink.consumed = sink.submitted;
          CHECK(bk_audio_poll(audio, e));
          CHECK(bk_title_menu_step(&state, &bind, &input, &ops, &frame, e));
          CHECK(bk_audio_fill(audio, e));
          CHECK(bk_title_menu_render_prepare(render, &frame, width, height, e));
          CHECK(bk_renderer_begin(r, e));
          CHECK(bk_renderer_viewport(r, NULL, e));
          CHECK(bk_renderer_draw(r, background, quad, 6, bk_identity, e));
          CHECK(bk_renderer_viewport(r, &viewport, e));
          CHECK(bk_title_menu_render_draw(render, e));
          CHECK(bk_renderer_end(r, e));
          CHECK(bk_renderer_readback(r, pixels, W * H * 4, e));
          for (unsigned y = 1; y < height; y += 3)
            for (unsigned x = 1; x < width; x += 3) {
              int rgb[3];
              if (!expected(images, &frame, x, y, rgb))
                continue;
              for (unsigned c = 0; c < 3; ++c) {
                unsigned pixel =
                    pixels[((size_t)(y + viewport.y) * W + x + viewport.x) * 4 +
                           c];
                unsigned delta = (unsigned)abs((int)pixel - rgb[c]);
                if (delta > worst)
                  worst = delta;
                if (delta > 1) {
                  snprintf(e, 256,
                           "pixel mismatch v%u s%u target%u tick%u xy%u,%u c%u "
                           "got%u want%d",
                           variant, special, target, tick, x, y, c, pixel,
                           rgb[c]);
                  goto done;
                }
                ++samples;
              }
            }
          if (tick == 22 && !variant && !special && target == 1) {
            FILE *file = fopen(argv[2], "wb");
            CHECK(file);
            size_t wrote = fwrite(pixels, 1, W * H * 4, file);
            int closed = fclose(file);
            CHECK(wrote == W * H * 4 && !closed);
            BkTitleMenuState before = state;
            BkCommonHudState cb = common;
            BkFlowTransition fb = flow;
            CHECK(bk_renderer_begin(r, e));
            CHECK(bk_renderer_viewport(r, NULL, e));
            CHECK(bk_renderer_draw(r, background, quad, 6, bk_identity, e));
            CHECK(bk_renderer_viewport(r, &viewport, e));
            CHECK(bk_title_menu_render_draw(render, e));
            CHECK(bk_renderer_end(r, e));
            CHECK(bk_renderer_readback(r, redraw, W * H * 4, e));
            CHECK(!memcmp(pixels, redraw, W * H * 4) &&
                  !memcmp(&state, &before, sizeof(state)) &&
                  !memcmp(&common, &cb, sizeof(common)) &&
                  !memcmp(&flow, &fb, sizeof(flow)));
            ++redraws;
          }
          ++frames;
          if (flow.current != 1)
            break;
        }
        static const uint8_t targets[] = {0x38, 0x28, 0x30, 0x18, 0x58, 0x60};
        CHECK(flow.current == 0x50 &&
              flow.target == targets[(target - 1) / 2] && !state.loaded_mask);
      }
      bk_title_menu_render_destroy(render);
      render = NULL;
    }
  }
  /* Retail Japanese data has no demo te_10.bmp. Missing art must fail. */
  render = bk_title_menu_render_create(r, store, 1, e);
  CHECK(!render && strstr(e, "te_10.bmp"));
  e[0] = 0;
  CHECK(sink.nonzero && services.releases == 12 && redraws == 1);
  printf("title-menu GPU PASS frames=%u samples=%u worst=%u releases=%u "
         "redraws=%u PCM=%" PRIu64 " nonzero=%" PRIu64 " hash=%016" PRIx64 "\n",
         frames, samples, worst, services.releases, redraws, sink.samples,
         sink.nonzero, sink.hash);
  result = 0;
done:
  if (result)
    fprintf(stderr, "title-menu GPU FAIL: %s\n", e);
  bk_blob_free(&raw);
  for (unsigned i = 0; i < 15; ++i)
    bk_image_free(&images[i]);
  bk_title_menu_render_destroy(render);
  bk_texture_destroy(r, background);
  for (unsigned i = 0; i < 4; ++i)
    bk_system_audio_destroy(services.sounds[i]);
  bk_audio_clip_release(music);
  bk_audio_destroy(audio);
  bk_renderer_destroy(r);
  bk_resources_destroy(store);
  free(pixels);
  free(redraw);
  return result;
}
