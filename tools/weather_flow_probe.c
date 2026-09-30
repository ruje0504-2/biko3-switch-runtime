/* Actual game composition: same scene/state with real versus transparent rain
 * texture. Only the texture is a test fixture; gameplay/weather are untouched.
 * The upper half (outside opening HUD) must equal independent rain blending. */
#include "app/game_preview.h"
#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#define W 1280
#define H 720
#define SIZE (W * H * 4)
#define REQUIRE(x)                                                             \
  do {                                                                         \
    if (!(x)) {                                                                \
      fprintf(stderr, "weather-flow line%d: %s\n", __LINE__, error);           \
      goto done;                                                               \
    }                                                                          \
  } while (0)
static double texel(const BkImage *im, int x, int y, unsigned c) {
  if (x < 0)
    x = 0;
  if (y < 0)
    y = 0;
  if (x > 255)
    x = 255;
  if (y > 255)
    y = 255;
  return im->rgba[(y * 256 + x) * 4 + c] / 255.0;
}
static void composite(const BkImage *im, const BkRainDraw *draw, unsigned x,
                      unsigned y, int rgb[3]) {
  for (unsigned i = 0; i < draw->count; i++) {
    const BkRainSprite *s = &draw->sprites[i];
    if (x < s->x || x >= s->x + 256 || y < s->y || y >= s->y + 512)
      continue;
    double u = x - (double)s->x - .5, v = (y - (double)s->y) * .5 - .5;
    int tx = (int)floor(u), ty = (int)floor(v);
    double fx = u - tx, fy = v - ty, color[4];
    for (unsigned c = 0; c < 4; c++)
      color[c] =
          (texel(im, tx, ty, c) * (1 - fx) + texel(im, tx + 1, ty, c) * fx) *
              (1 - fy) +
          (texel(im, tx, ty + 1, c) * (1 - fx) +
           texel(im, tx + 1, ty + 1, c) * fx) *
              fy;
    for (unsigned c = 0; c < 3; c++)
      rgb[c] = (int)lround(color[c] * color[3] * 255 + rgb[c] * (1 - color[3]));
  }
}
static int present(BkScene *s, BkRenderer *r, uint8_t *pixels, char e[256]) {
  return bk_renderer_begin(r, e) && bk_scene_draw(s, &(BkSceneFrame){0}, e) &&
         bk_renderer_end(r, e) && bk_renderer_readback(r, pixels, SIZE, e);
}
static int unexpected_release(void *p, uint8_t flow, char e[256]) {
  (void)p;
  snprintf(e, 256, "unexpected failure release%u", flow);
  return 0;
}
static int unexpected_schedule(void *p, uint8_t flow, uint8_t mode,
                               char e[256]) {
  (void)mode;
  return unexpected_release(p, flow, e);
}
int main(int argc, char **argv) {
  if (argc != 3)
    return 2;
  char error[256] = {0}, path[2048];
  int status = 1, max_error = 0;
  unsigned samples = 0, changed = 0, frames = 0;
  uint64_t hash = UINT64_C(1469598103934665603);
  BkResourceStore *store[2] = {0};
  BkScene *game[2] = {0};
  BkGameFrameState state[2];
  BkEntryProgress progress[2];
  BkFailureHudState failure[2] = {0};
  BkCommonHudState common[2];
  uint8_t overlay[2] = {0};
  BkFailureHudOps failure_ops = {.release = unexpected_release,
                                 .schedule = unexpected_schedule};
  BkRenderer *renderer = NULL;
  BkImage image = {0};
  BkBlob blob = {0};
  uint8_t *pixels[2] = {malloc(SIZE), malloc(SIZE)}, *again = malloc(SIZE);
  REQUIRE(pixels[0] && pixels[1] && again);
  REQUIRE(mkdir(argv[2], 0700) == 0 || errno == EEXIST);
  REQUIRE(snprintf(path, sizeof(path), "%s/rain.tga", argv[2]) <
          (int)sizeof(path));
  FILE *f = fopen(path, "wb");
  REQUIRE(f);
  uint8_t header[18] = {0, 0, 2, 0, 0, 0, 0, 0,  0,
                        0, 0, 0, 0, 1, 0, 1, 32, 0x28};
  uint8_t *zero = calloc(256 * 256, 4);
  REQUIRE(zero);
  int wrote = fwrite(header, 1, 18, f) == 18 &&
              fwrite(zero, 4, 256 * 256, f) == 256 * 256;
  int closed = fclose(f);
  free(zero);
  REQUIRE(wrote && !closed);
  renderer = bk_renderer_create(W, H, stderr, error);
  REQUIRE(renderer);
  const char *packs[] = {"bk3_00", "bk3_01", "bk3_02", "bk3_03",
                         "bk3_04", "bk3_05", "bk3_06", "bk3_07",
                         "bk3_15", "bk3_16", "bk3_20"};
  const char *loose[] = {"routes", "faces", "collision", "fonts"};
  for (unsigned k = 0; k < 2; k++) {
    store[k] = bk_resources_create(error);
    REQUIRE(store[k]);
    for (unsigned i = 0; i < sizeof(packs) / sizeof(packs[0]); i++) {
      REQUIRE(snprintf(path, sizeof(path), "%s/%s.pp", argv[1], packs[i]) <
              (int)sizeof(path));
      REQUIRE(bk_resources_mount(store[k], packs[i], path, error));
    }
    for (unsigned i = 0; i < 4; i++)
      REQUIRE(bk_resources_mount_directory(store[k], loose[i], argv[1],
                                           16 * 1024 * 1024, error));
    if (k)
      REQUIRE(bk_resources_mount_directory(store[k], "bk3_20", argv[2],
                                           1024 * 1024, error));
    REQUIRE(bk_scene_game_frame_boot_state(&state[k], &progress[k], 123));
    game[k] = bk_game_preview_create_entry(
        &(BkSceneServices){.resources = store[k], .renderer = renderer},
        &state[k], &progress[k], 4, 0, 8, 1, NULL, error);
    REQUIRE(game[k]);
    bk_common_hud_initialize(&common[k]);
  }
  REQUIRE(bk_resources_read(store[0], "bk3_20", "rain.tga", &blob, error) ==
          BK_RESOURCE_OK);
  REQUIRE(bk_image_decode(blob.data, blob.size, &image, error));
  bk_blob_free(&blob);
  for (frames = 0; frames < 72; frames++) {
    BkGameWeatherSnapshot weather[2];
    for (unsigned k = 0; k < 2; k++) {
      if (frames == 48) {
        /* Explicit original collision outcome, not a natural car collision. */
        state[k].interaction.outcome = 1;
        REQUIRE(bk_game_preview_load_failure(game[k], error));
      }
      if (frames < 48) {
        REQUIRE(bk_scene_step(game[k], 1.0 / 60, &(BkInput){0}, error));
      } else {
        BkFailureHudFrame output;
        REQUIRE(bk_game_preview_failure_step(
            game[k], &failure[k], &common[k], &overlay[k], &failure_ops,
            1.0 / 60, 1 + (frames + 1) / 60.0, &(BkInput){0}, &output, error));
      }
      REQUIRE(bk_game_preview_weather(game[k], &weather[k]));
      REQUIRE(weather[k].rain_draw.count == 16 && !weather[k].snow_present);
      REQUIRE(present(game[k], renderer, pixels[k], error));
    }
    REQUIRE(!memcmp(state, state + 1, sizeof(state[0])) &&
            !memcmp(weather, weather + 1, sizeof(weather[0])));
    for (unsigned y = 0; y < H / 2; y += 3)
      for (unsigned x = 0; x < 960; x += 3) {
        unsigned p = (y * W + x + 160) * 4;
        int rgb[3] = {pixels[1][p], pixels[1][p + 1], pixels[1][p + 2]};
        composite(&image, &weather[0].rain_draw, x, y, rgb);
        for (unsigned c = 0; c < 3; c++) {
          int d = abs(pixels[0][p + c] - rgb[c]);
          if (d > max_error)
            max_error = d;
          if (d > 3) {
            snprintf(error, 256, "frame%u pixel%u,%u channel%u error%d", frames,
                     x, y, c, d);
            REQUIRE(0);
          }
          changed += pixels[0][p + c] != pixels[1][p + c];
          hash = (hash ^ pixels[0][p + c]) * UINT64_C(1099511628211);
          samples++;
        }
      }
    for (unsigned y = 0; y < H; y++)
      for (unsigned x = 0; x < 160; x++) {
        unsigned p = (y * W + x) * 4, q = (y * W + x + 1120) * 4;
        REQUIRE(!memcmp(pixels[0] + p, pixels[1] + p, 4) &&
                !memcmp(pixels[0] + q, pixels[1] + q, 4));
      }
    BkGameFrameState held = state[0];
    REQUIRE(present(game[0], renderer, again, error) &&
            !memcmp(again, pixels[0], SIZE));
    REQUIRE(!memcmp(&held, &state[0], sizeof(held)));
  }
  REQUIRE(changed > 10000);
  printf(
      "PASS actual-game rain frames=%u RGBchecks=%u changed=%u "
      "maxerror=%d/255 fnv=%016llx redraw/letterbox/shared-state/failure24\n",
      frames, samples, changed, max_error, (unsigned long long)hash);
  status = 0;
done:
  for (unsigned k = 0; k < 2; k++) {
    bk_scene_destroy(game[k]);
    bk_resources_destroy(store[k]);
    free(pixels[k]);
  }
  bk_renderer_destroy(renderer);
  bk_image_free(&image);
  bk_blob_free(&blob);
  free(again);
  return status;
}
