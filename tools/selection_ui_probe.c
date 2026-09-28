/* UI-only fixture: real Japanese images/music/voice; actor replacement and
 * destination release are explicit observers, not loaded game scenes. */
#include "scene/selection_ui_render.h"
#include "scene/system_audio.h"
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
static int expected(const BkImage im[50], const BkSelectionFrame *f, unsigned x,
                    unsigned y, int out[3]) {
  out[0] = 70;
  out[1] = 110;
  out[2] = 160;
  for (unsigned i = 0; i < f->count; ++i) {
    const BkSelectionDraw *d = &f->draws[i];
    double l = d->corners[0], t = d->corners[1], width = d->corners[2] - l,
           height = d->corners[3] - t;
    /* Raster edge inclusivity is not the sampler/blend oracle's subject. */
    if (fabs(x - l) < .03 || fabs(y - t) < .03 || fabs(x - l - width) < .03 ||
        fabs(y - t - height) < .03)
      return 0;
    if (x < l || x >= l + width || y < t || y >= t + height)
      continue;
    const BkImage *image = &im[d->slot];
    double u = (d->uv[0] + (x - l) / width * (d->uv[2] - d->uv[0])) *
                   image->width -
               .5,
           v = (d->uv[1] + (y - t) / height * (d->uv[3] - d->uv[1])) *
                   image->height -
               .5;
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
  BkAudioClip *voices[5];
  BkSystemAudio *sounds[5];
  unsigned selected, replacements, releases;
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
  return slot < 5 && s->sounds[slot] &&
         bk_system_audio_restart(s->sounds[slot], e);
}
static int gain(void *p, int32_t volume, char e[256]) {
  return bk_audio_gain(((Services *)p)->audio, 60, volume, 0, e);
}
static int voice_stop(void *p, char e[256]) {
  return bk_audio_pause(((Services *)p)->audio, 61, e);
}
static int voice_play(void *p, int32_t volume, char e[256]) {
  Services *s = p;
  return bk_audio_play(s->audio, 61, s->voices[s->selected], 0, volume, 0, e);
}
static int voice_status(void *p, int *playing, char e[256]) {
  (void)e;
  return bk_audio_playing(((Services *)p)->audio, 61, playing);
}
static int replace(void *p, unsigned group, char e[256]) {
  Services *s = p;
  if (group >= 5)
    return 0;
  ++s->replacements;
  s->selected = group;
  return bk_audio_play(s->audio, 61, s->voices[group], 0, -700, 0, e) &&
         bk_audio_pause(s->audio, 61, e);
}
static int release(void *p, uint8_t flow, char e[256]) {
  Services *s = p;
  if (flow != 0x38)
    return 0;
  ++s->releases;
  return bk_audio_clear(s->audio, 60, e) && bk_audio_clear(s->audio, 61, e);
}
int main(int argc, char **argv) {
  if (argc != 3)
    return 2;
  char e[256] = {0}, path[1024];
  int result = 1;
  BkResourceStore *store = bk_resources_create(e);
  BkRenderer *r = NULL;
  BkSelectionUiRender *render = NULL;
  BkAudio *audio = NULL;
  BkAudioClip *music = NULL;
  BkTexture *background = NULL;
  BkImage images[50] = {{0}};
  BkBlob raw = {0};
  uint8_t *pixels = malloc(W * H * 4), *redraw = malloc(W * H * 4);
  Sink sink = {.hash = UINT64_C(14695981039346656037)};
  Services services = {0};
  unsigned frames = 0, samples = 0, worst = 0, redraws = 0;
  CHECK(store && pixels && redraw);
  const char *packs[] = {"bk3_00", "bk3_02", "bk3_06"};
  for (unsigned i = 0; i < 3; ++i) {
    snprintf(path, sizeof(path), "%s/%s.pp", argv[1], packs[i]);
    CHECK(bk_resources_mount(store, packs[i], path, e));
  }
  r = bk_renderer_create(W, H, stderr, e);
  CHECK(r);
  BkAudioSink output = {&sink, 48000, 240, 960, submit, poll};
  audio = bk_audio_create(&output, e);
  CHECK(audio);
  services.audio = audio;
  music = bk_audio_clip_load(store, "bk3_02", "bg002.wav", e);
  CHECK(music);
  const char *voices[] = {"PT10000.wav", "PT20029.wav", "PT30011.wav",
                          "PT40001.wav", "PT51000.wav"};
  for (unsigned i = 0; i < 5; ++i) {
    services.voices[i] = bk_audio_clip_load(store, "bk3_06", voices[i], e);
    CHECK(services.voices[i]);
  }
  for (unsigned i = 1; i < 5; ++i) {
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
  for (unsigned i = 0; i < 50; ++i) {
    const char *name = bk_selection_image(i, 0);
    if (!name)
      continue;
    CHECK(bk_resources_read(store, "bk3_00", name, &raw, e) == BK_RESOURCE_OK);
    CHECK(bk_image_decode(raw.data, raw.size, &images[i], e));
    bk_blob_free(&raw);
  }
  render = bk_selection_ui_render_create(r, store, 0, e);
  CHECK(render);
  for (unsigned variant = 0; variant < 2; ++variant) {
    unsigned width = variant ? 503 : 640, height = variant ? 377 : 480;
    BkViewport viewport = {(W - width) / 2, (H - height) / 2, width, height};
    for (unsigned group = 0; group < 5; ++group) {
      BkSelectionUi state = {0};
      BkCommonHudState common = {.curtain = {0, 2, 0}};
      BkMenuCursor cursor = {0};
      CHECK(bk_menu_cursor_initialize(&cursor, width, height));
      BkFlowTransition flow = {0x38, 1, 0, 0};
      uint8_t latch = 0;
      int32_t photos[] = {2, 9, 15, 4, 30}, dest_group = 0, area = 0, count = 0;
      BkSelectionBindings bind = {&common, &flow,       &cursor, &latch,
                                  photos,  &dest_group, &area,   &count};
      BkSelectionOps ops = {&services,    sound,   gain,       position,
                            motion,       warp,    voice_stop, voice_play,
                            voice_status, replace, release};
      CHECK(bk_audio_play(audio, 60, music, 1, -900, 0, e));
      CHECK(replace(&services, 0, e));
      CHECK(bk_selection_ui_initialize(&state, width, 0, -900, e));
      for (unsigned tick = 0; tick < 100; ++tick) {
        unsigned slot = 27 + group * 2;
        uint32_t buttons = 0;
        if (tick == 4)
          buttons = BK_PAUSE_CONFIRM;
        if (tick >= 8 && tick < 12)
          slot = 1;
        if (tick == 9)
          buttons = BK_PAUSE_CONFIRM;
        if (tick >= 12 && tick < 60)
          slot = 40;
        if (tick == 13 || tick == 25)
          buttons = BK_PAUSE_CONFIRM;
        if (tick >= 60 && tick < 64)
          slot = 20;
        if (tick == 61)
          buttons = BK_PAUSE_CONFIRM;
        if (tick >= 64 && tick < 68)
          slot = 17;
        if (tick == 65)
          buttons = BK_PAUSE_CONFIRM;
        if (tick >= 68)
          slot = variant ? 25 : 23;
        if (tick == 72)
          buttons = BK_PAUSE_CONFIRM;
        BkSelectionSprite *p = &state.sprites[slot];
        services.point[0] = p->rect[0] + p->rect[2] / 2;
        services.point[1] = p->rect[1] + p->rect[3] / 2;
        services.relative[0] = tick % 7 == 0 ? 1 : 0;
        BkSelectionInput input = {buttons, frames * 100, .1f, width / 1280.f,
                                  -900,    -700,         0,   1};
        BkSelectionFrame frame;
        sink.consumed = sink.submitted;
        CHECK(bk_audio_poll(audio, e));
        CHECK(bk_selection_ui_view(&state, &bind, &input, &ops, &frame, e));
        CHECK(bk_selection_ui_control(&state, &bind, &input, &ops, e));
        CHECK(bk_audio_fill(audio, e));
        CHECK(bk_selection_ui_render_prepare(render, &frame, width, height, e));
        CHECK(bk_renderer_begin(r, e));
        CHECK(bk_renderer_viewport(r, NULL, e));
        CHECK(bk_renderer_draw(r, background, quad, 6, bk_identity, e));
        CHECK(bk_renderer_viewport(r, &viewport, e));
        CHECK(bk_selection_ui_render_draw(render, e));
        CHECK(bk_renderer_end(r, e));
        CHECK(bk_renderer_readback(r, pixels, W * H * 4, e));
        for (unsigned y = 1; y < height; y += 4)
          for (unsigned x = 1; x < width; x += 4) {
            int rgb[3];
            if (!expected(images, &frame, x, y, rgb))
              continue;
            for (unsigned c = 0; c < 3; ++c) {
              unsigned actual =
                  pixels[((size_t)(y + viewport.y) * W + x + viewport.x) * 4 +
                         c];
              unsigned delta = abs((int)actual - rgb[c]);
              if (delta > worst)
                worst = delta;
              /* Two overlaid labels can accumulate per-pass UNORM rounding. */
              if (delta > 2) {
                snprintf(e, 256,
                         "pixel v%u group%u tick%u xy%u,%u c%u got%u want%d",
                         variant, group, tick, x, y, c, actual, rgb[c]);
                goto done;
              }
              ++samples;
            }
          }
        if (tick == 20 && !variant && group == 0) {
          FILE *file = fopen(argv[2], "wb");
          CHECK(file);
          size_t wrote = fwrite(pixels, 1, W * H * 4, file);
          int closed = fclose(file);
          CHECK(wrote == W * H * 4 && !closed);
          BkSelectionUi before = state;
          BkCommonHudState cb = common;
          BkFlowTransition fb = flow;
          CHECK(bk_renderer_begin(r, e));
          CHECK(bk_renderer_viewport(r, NULL, e));
          CHECK(bk_renderer_draw(r, background, quad, 6, bk_identity, e));
          CHECK(bk_renderer_viewport(r, &viewport, e));
          CHECK(bk_selection_ui_render_draw(render, e));
          CHECK(bk_renderer_end(r, e));
          CHECK(bk_renderer_readback(r, redraw, W * H * 4, e));
          CHECK(!memcmp(pixels, redraw, W * H * 4) &&
                !memcmp(&state, &before, sizeof(state)) &&
                !memcmp(&common, &cb, sizeof(common)) &&
                !memcmp(&flow, &fb, sizeof(flow)));
          ++redraws;
        }
        ++frames;
        if (flow.current != 0x38)
          break;
      }
      CHECK(flow.current == 0x50 && flow.target == (variant ? 1 : 8) &&
            !state.loaded);
      if (!variant)
        CHECK(dest_group == (int32_t)group && area == 0 &&
              count == photos[group]);
    }
  }
  CHECK(sink.nonzero && services.releases == 10 &&
        services.replacements == 18 && redraws == 1);
  printf("selection-ui GPU PASS frames=%u samples=%u worst=%u releases=%u "
         "replacements=%u redraws=%u PCM=%" PRIu64 " nonzero=%" PRIu64
         " hash=%016" PRIx64 "\n",
         frames, samples, worst, services.releases, services.replacements,
         redraws, sink.samples, sink.nonzero, sink.hash);
  result = 0;
done:
  if (result)
    fprintf(stderr, "selection-ui GPU FAIL: %s\n", e);
  bk_blob_free(&raw);
  for (unsigned i = 0; i < 50; ++i)
    bk_image_free(&images[i]);
  bk_selection_ui_render_destroy(render);
  bk_texture_destroy(r, background);
  for (unsigned i = 0; i < 5; ++i) {
    bk_system_audio_destroy(services.sounds[i]);
    bk_audio_clip_release(services.voices[i]);
  }
  bk_audio_clip_release(music);
  bk_audio_destroy(audio);
  bk_renderer_destroy(r);
  bk_resources_destroy(store);
  free(pixels);
  free(redraw);
  return result;
}
