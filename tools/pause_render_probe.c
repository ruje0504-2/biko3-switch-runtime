/* Real output-file -> pause session -> ordered Vulkan submission. The source
 * capture is an existing GPU fixture, not a claim of full-game rendering.
 * release_other records its boundary; no actor resource owner is present here.
 */
#define _POSIX_C_SOURCE 200809L
#ifdef __APPLE__
#define _DARWIN_C_SOURCE
#endif
#include "app/pause_session.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <inttypes.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
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
typedef struct {
  float point[2], motion[2];
  unsigned releases;
} Services;
static int warp(void *p, float x, float y, char e[256]) {
  (void)e;
  Services *s = p;
  s->point[0] = x;
  s->point[1] = y;
  return 1;
}
static int pointer(void *p, float point[2], float motion[2], char e[256]) {
  (void)e;
  Services *s = p;
  memcpy(point, s->point, sizeof(s->point));
  memcpy(motion, s->motion, sizeof(s->motion));
  return 1;
}
static int release_other(void *p, uint8_t flow, char e[256]) {
  (void)e;
  assert(flow == 2);
  ++((Services *)p)->releases;
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
static int expected(const BkImage im[22], const BkPauseFrame *f, unsigned x,
                    unsigned y, int out[3]) {
  out[0] = 70;
  out[1] = 110;
  out[2] = 160;
  for (unsigned i = 0; i < f->count; ++i) {
    const BkPauseDraw *d = &f->draws[i];
    double l = d->rect[0], t = d->rect[1], width = d->rect[2],
           height = d->rect[3];
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
int main(int argc, char **argv) {
  if (argc != 3 && argc != 4)
    return 2;
  char e[256] = {0}, path[1024], root[] = "/tmp/bk-pause-XXXXXX";
  int rc = 1, made_root = 0;
  BkResourceStore *store = bk_resources_create(e);
  BkCaptureFiles *files = NULL;
  BkPauseSession *session = NULL;
  BkRenderer *r = NULL;
  BkAudio *audio = NULL;
  BkTexture *bg = NULL;
  BkSystemAudio *sounds[8] = {0};
  BkImage images[22] = {{0}};
  BkBlob source = {0}, raw = {0};
  uint8_t *pixels = malloc(W * H * 4);
  unsigned frames = 0, samples = 0, worst = 0, releases = 0, deleted = 0;
  Sink sink = {.hash = UINT64_C(14695981039346656037)};
  CHECK(store && pixels && mkdtemp(root));
  made_root = 1;
  files = bk_capture_files_create(root, e);
  CHECK(files);
  FILE *in = fopen(argv[2], "rb");
  CHECK(in);
  int seek = fseek(in, 0, SEEK_END);
  long size = seek ? -1 : ftell(in);
  if (size <= 0 || size > 16 * 1024 * 1024 || fseek(in, 0, SEEK_SET)) {
    fclose(in);
    CHECK(0);
  }
  source = (BkBlob){malloc((size_t)size), (size_t)size};
  if (!source.data) {
    fclose(in);
    CHECK(0);
  }
  size_t n = fread(source.data, 1, source.size, in);
  int close = fclose(in);
  CHECK(n == source.size && !close);
  CHECK(bk_image_decode(source.data, source.size, &images[19], e));
  const char *packs[] = {"bk3_00", "bk3_02"};
  for (unsigned i = 0; i < 2; ++i) {
    snprintf(path, sizeof(path), "%s/%s.pp", argv[1], packs[i]);
    CHECK(bk_resources_mount(store, packs[i], path, e));
  }
  for (unsigned i = 0; i < 22; ++i) {
    if (i == 19)
      continue;
    CHECK(bk_resources_read(store, "bk3_00", bk_pause_image(i), &raw, e) ==
          BK_RESOURCE_OK);
    CHECK(bk_image_decode(raw.data, raw.size, &images[i], e));
    bk_blob_free(&raw);
  }
  r = bk_renderer_create(W, H, stderr, e);
  CHECK(r);
  BkAudioSink output = {&sink, 48000, 240, 960, submit, poll};
  audio = bk_audio_create(&output, e);
  CHECK(audio);
  const unsigned slots[] = {0, 1, 2, 3, 5};
  for (unsigned i = 0; i < 5; ++i) {
    unsigned slot = slots[i];
    sounds[slot] =
        bk_system_audio_create_slot(store, audio, slot, slot, -600, e);
    CHECK(sounds[slot]);
  }
  uint8_t color[] = {70, 110, 160, 255};
  BkImage solid = {1, 1, color};
  bg = bk_texture_create(r, &solid, e);
  CHECK(bg);
  const BkVertex quad[6] = {
      {-1, -1, 0, 0, 0, 1, 1, 1, 1}, {1, -1, 0, 1, 0, 1, 1, 1, 1},
      {1, 1, 0, 1, 1, 1, 1, 1, 1},   {-1, -1, 0, 0, 0, 1, 1, 1, 1},
      {1, 1, 0, 1, 1, 1, 1, 1, 1},   {-1, 1, 0, 0, 1, 1, 1, 1, 1}};
  for (unsigned variant = 0; variant < 4; ++variant) {
    unsigned vw = (unsigned[]){640, 512, 320, 503}[variant];
    unsigned vh = (unsigned[]){480, 384, 240, 367}[variant];
    BkViewport vp = {(W - vw) / 2, (H - vh) / 2, vw, vh};
    for (unsigned special = 0; special < 2; ++special)
      for (unsigned route = 2; route <= 8; ++route) {
        unsigned choice = route <= 6 ? route : route - 2;
        BkPauseState state = {0};
        BkCommonHudState common = {.curtain = {0, 2, 0}};
        BkFlowTransition flow = {4, 2, 0, 0};
        BkMenuCursor cursor = {0};
        CHECK(bk_menu_cursor_initialize(&cursor, vw, vh));
        uint8_t overlay = 0, latch = 0;
        BkPauseBindings bind = {&common, &flow, &cursor, &overlay, &latch};
        Services services = {0};
        BkPauseSessionOps ops = {&services, warp, pointer, release_other};
        CHECK(bk_capture_file_write(files, 0, "sy_99.bmp", &source, e));
        session =
            bk_pause_session_create(r, store, files, 16 * 1024 * 1024, &state,
                                    &bind, sounds, &ops, vw, vh, e);
        CHECK(session);
        float *q = state.sprites[choice].rect;
        services.point[0] = q[0] + q[2] / 2;
        services.point[1] = q[1] + q[3] / 2;
        for (unsigned tick = 0; tick < 100; ++tick) {
          sink.consumed = sink.submitted;
          CHECK(bk_audio_poll(audio, e));
          unsigned buttons = tick == 8 ? BK_PAUSE_CONFIRM
                             : tick == 16 && choice >= 5 && route <= 6
                                 ? BK_PAUSE_RIGHT
                             : tick == 18 && choice >= 5 ? BK_PAUSE_CONFIRM
                             : tick == 30 && route >= 7  ? BK_PAUSE_CONFIRM
                                                         : 0;
          /* Cancellation returns the pointer to its source row. Then move it
           * to resume, allowing one old-pointer frame before confirming. */
          if (route >= 7 && tick == 26) {
            services.point[0] = 320.f * vw / 640;
            services.point[1] = 240.f * vw / 640;
          }
          BkPauseInput input = {buttons, frames * 250, .25f,
                                (float)((double)vw / 1280), (uint8_t)special};
          BkPauseFrame frame;
          CHECK(bk_pause_session_step(session, &input, &frame, e));
          CHECK(bk_audio_fill(audio, e));
          CHECK(bk_renderer_begin(r, e));
          CHECK(bk_renderer_draw(r, bg, quad, 6, bk_identity, e));
          CHECK(bk_renderer_viewport(r, &vp, e));
          CHECK(bk_pause_session_draw(session, e));
          CHECK(bk_renderer_end(r, e));
          CHECK(bk_renderer_readback(r, pixels, W * H * 4, e));
          for (unsigned y = 1; y < vh; y += 5)
            for (unsigned x = 1; x < vw; x += 5) {
              int want[3];
              if (!expected(images, &frame, x, y, want))
                continue;
              for (unsigned c = 0; c < 3; ++c) {
                unsigned got =
                    pixels[((size_t)(y + vp.y) * W + x + vp.x) * 4 + c];
                unsigned diff = (unsigned)abs((int)got - want[c]);
                if (diff > worst)
                  worst = diff;
                if (diff > 1) {
                  snprintf(e, sizeof(e),
                           "frame%u route%u pixel%u,%u ch%u %u!=%d", frames,
                           route, x, y, c, got, want[c]);
                  goto done;
                }
                ++samples;
              }
            }
          if (argc == 4 && variant == 0 && special == 0 && route == 5 &&
              (tick == 7 || tick == 15)) {
            snprintf(path, sizeof(path), "%s.%s.rgba", argv[3],
                     tick == 7 ? "main" : "confirm");
            FILE *out = fopen(path, "wb");
            CHECK(out);
            n = fwrite(pixels, 1, W * H * 4, out);
            close = fclose(out);
            CHECK(n == W * H * 4 && !close);
          }
          ++frames;
          if (flow.current != 4)
            break;
        }
        CHECK(flow.current == (choice == 4 || route >= 7 ? 2 : 0x50));
        CHECK(!state.loaded);
        CHECK(services.releases == (unsigned)(route == 5 || route == 6));
        releases += services.releases;
        BkResourceResult read =
            bk_capture_file_read_pause(files, 16 * 1024 * 1024, &raw, e);
        CHECK(read == (choice == 2 || choice == 3 ? BK_RESOURCE_OK
                                                  : BK_RESOURCE_MISSING));
        deleted += read == BK_RESOURCE_MISSING;
        bk_blob_free(&raw);
        /* Last frame was submitted above even after logical release4. */
        bk_pause_session_destroy(session);
        session = NULL;
        CHECK(bk_capture_file_remove_pause(files, e));
        BkPauseState before = state;
        CHECK(!bk_pause_session_create(r, store, files, 16 * 1024 * 1024,
                                       &state, &bind, sounds, &ops, vw, vh, e));
        CHECK(!memcmp(&before, &state, sizeof(state)));
        e[0] = 0;
      }
  }
  CHECK(sink.nonzero);
  printf("PASS pause GPU frames=%u channels=%u max-error=%u/255 "
         "game-release-boundaries=%u deleted=%u PCM-samples=%" PRIu64
         " nonzero=%" PRIu64 " hash=%016" PRIx64 "\n",
         frames, samples, worst, releases, deleted, sink.samples, sink.nonzero,
         sink.hash);
  rc = 0;
done:
  if (rc)
    fprintf(stderr, "FAIL pause GPU: %s\n", e);
  bk_pause_session_destroy(session);
  for (unsigned i = 0; i < 8; ++i)
    bk_system_audio_destroy(sounds[i]);
  bk_audio_destroy(audio);
  bk_texture_destroy(r, bg);
  bk_renderer_destroy(r);
  for (unsigned i = 0; i < 22; ++i)
    bk_image_free(&images[i]);
  bk_blob_free(&source);
  bk_blob_free(&raw);
  bk_resources_destroy(store);
  if (files)
    bk_capture_file_remove_pause(files, e);
  bk_capture_files_destroy(files);
  if (made_root) {
    snprintf(path, sizeof(path), "%s/album", root);
    rmdir(path);
    rmdir(root);
  }
  free(pixels);
  return rc;
}
