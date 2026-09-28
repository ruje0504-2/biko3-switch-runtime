/* Actual toolbar + branch hints, using packaged images. Projected points and
 * active-clip timing are explicit fixtures, not a complete ending scene. */
#include "ending_ui_render_oracle.h"
#include "game/ending_normal.h"
#include "scene/ending_audio.h"
#include "scene/ending_ui_cursor.h"
#include "scene/ending_ui_hints.h"
#include "scene/ending_ui_render.h"
#include "scene/ending_ui_select.h"
#include "scene/ending_ui_toolbar.h"
#include <inttypes.h>
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
static int same_alloc(BkRenderStats a, BkRenderStats b) {
  return a.live_allocations == b.live_allocations &&
         a.live_bytes == b.live_bytes;
}
static int key(void *ctx, unsigned code, unsigned mode, uint32_t *out,
               char e[256]) {
  (void)ctx;
  (void)e;
  if (code > 1 || mode != 2)
    return 0;
  *out = 0;
  return 1;
}
typedef struct {
  BkEndingAudio *audio;
  uint64_t submitted, consumed, samples, hash;
  unsigned status_calls;
} AudioContext;
static int submit(void *p, const int16_t *pcm, size_t frames, char e[256]) {
  (void)e;
  AudioContext *a = p;
  for (size_t i = 0; i < frames * 2; i++) {
    uint16_t v = (uint16_t)pcm[i];
    a->hash = (a->hash ^ (v & 255)) * UINT64_C(1099511628211);
    a->hash = (a->hash ^ (v >> 8)) * UINT64_C(1099511628211);
  }
  a->submitted += frames;
  a->samples += frames * 2;
  return 1;
}
static int poll(void *p, uint64_t *frames, char e[256]) {
  (void)e;
  *frames = ((AudioContext *)p)->consumed;
  return 1;
}
static int voice(void *p, int *playing, char e[256]) {
  AudioContext *a = p;
  a->status_calls++;
  BkEndingAudioCall call = {.operation = BK_ENDING_AUDIO_STATUS, .slot = 1};
  return bk_ending_audio_call(a->audio, 0, 0, 0, &call, playing, e);
}
int main(int argc, char **argv) {
  int selectors = argc == 3 && !strcmp(argv[2], "--selectors");
  int cursors = selectors || (argc == 3 && !strcmp(argv[2], "--cursors"));
  if (argc != 2 && !cursors)
    return 2;
  int result = 1;
  char e[256] = {0}, path[1024];
  BkRenderer *r = NULL;
  BkResourceStore *store = NULL;
  BkAudio *mix = NULL;
  AudioContext audio = {.hash = UINT64_C(14695981039346656037)};
  BkEndingUiRender *base_render = NULL, *stage_render = NULL;
  BkTexture *bg = NULL;
  BkImage images[75] = {0};
  BkBlob raw = {0};
  uint8_t *pixels = malloc(W * H * 4), *again = malloc(W * H * 4);
  unsigned profiles = 0, frames = 0, samples = 0, worst = 0, redraws = 0,
           draws = 0, lines = 0, rings = 0, meters = 0;
  unsigned cursor_draws = 0, popup_draws = 0, notice_draws = 0;
  uint64_t hash = UINT64_C(14695981039346656037);
  CHECK(pixels && again);
  store = bk_resources_create(e);
  CHECK(store);
  snprintf(path, sizeof(path), "%s/bk3_00.pp", argv[1]);
  CHECK(bk_resources_mount(store, "bk3_00", path, e));
  if (selectors) {
    snprintf(path, sizeof(path), "%s/bk3_06.pp", argv[1]);
    CHECK(bk_resources_mount(store, "bk3_06", path, e));
    BkAudioSink sink = {&audio, 48000, 480, 1920, submit, poll};
    mix = bk_audio_create(&sink, e);
    CHECK(mix);
    audio.audio = bk_ending_audio_create(store, mix, 50, e);
    CHECK(audio.audio);
    CHECK(bk_ending_audio_bind(audio.audio, 1, "bk3_06", "PH12105.wav", e));
  }
  r = bk_renderer_create(W, H, stderr, e);
  CHECK(r);
  uint8_t color[] = {70, 110, 160, 255};
  BkImage solid = {1, 1, color};
  bg = bk_texture_create(r, &solid, e);
  CHECK(bg);
  const BkVertex quad[6] = {
      {-1, -1, 0, 0, 0, 1, 1, 1, 1}, {1, -1, 0, 1, 0, 1, 1, 1, 1},
      {1, 1, 0, 1, 1, 1, 1, 1, 1},   {-1, -1, 0, 0, 0, 1, 1, 1, 1},
      {1, 1, 0, 1, 1, 1, 1, 1, 1},   {-1, 1, 0, 0, 1, 1, 1, 1, 1}};
  BkRenderStats baseline = bk_renderer_stats(r);
  base_render = bk_ending_ui_render_create(r, store, e);
  CHECK(base_render);
  for (unsigned slot = 0; slot < 63; slot++) {
    const char *name = bk_ending_ui_image(slot);
    if (!name)
      continue;
    CHECK(bk_resources_read(store, "bk3_00", name, &raw, e) == BK_RESOURCE_OK);
    CHECK(bk_image_decode(raw.data, raw.size, &images[slot], e));
    bk_blob_free(&raw);
  }
  BkRenderStats base_loaded = bk_renderer_stats(r);
  BkEndingUi ui = {0};
  BkEndingStageUi stage = {0};
  BkEndingUiHintsState hints = {.fixed_scroll = .998f,
                                .variable_scroll = .001f};
  for (unsigned kind = 1; kind <= 4; kind++)
    for (unsigned group = 0; group < 5; group++)
      for (unsigned variant = 0; variant < (kind == 2 ? 2u : 1u); variant++) {
        unsigned width = group % 2 ? 503 : 640, height = group % 2 ? 377 : 480;
        BkViewport viewport = {(W - width) / 2, (H - height) / 2, width,
                               height};
        float scale = (float)((double)width / 1280), gauge;
        BkEndingFrameState f = {.phase = kind == 1   ? 2
                                         : kind == 2 ? 5
                                         : kind == 3 ? 3
                                                     : 4,
                                .group = group,
                                .state_721ee4 = 3,
                                .camera_event = 1,
                                .camera_cached = 4,
                                .camera_request = 1};
        BkEndingControlState c = {.variant = kind == 1 ? 1 : 0,
                                  .state_721eec = 3};
        BkEndingAuxiliaryState a = {.gate = 3};
        int32_t gate = 3, clip = 2, open = 0;
        BkEndingUiToolbarBindings tb = {&f, &c, &a, &open};
        int32_t normal_ready = 1;
        BkEndingUiNoticeState notices = {0};
        BkEndingUiCursorBindings cb = {&f, &a, &clip, &normal_ready, &notices};
        CHECK(bk_ending_ui_initialize(&ui, width, c.pause_flags, &gauge, e));
        CHECK(bk_ending_stage_ui_initialize(
            &ui, &stage, (BkEndingUiStageKind)kind, group, variant, width, e));
        for (unsigned slot = 0; slot < 75; slot++) {
          const char *name = bk_ending_stage_ui_image(&stage, slot);
          if (!name)
            continue;
          CHECK(bk_resources_read(store, "bk3_00", name, &raw, e) ==
                BK_RESOURCE_OK);
          CHECK(bk_image_decode(raw.data, raw.size, &images[slot], e));
          bk_blob_free(&raw);
        }
        stage_render = bk_ending_ui_render_create_stage(r, store, &stage, e);
        CHECK(stage_render);
        BkRenderStats loaded = bk_renderer_stats(r);
        int32_t points[3][2] = {{(int32_t)width / 4, (int32_t)height / 3},
                                {(int32_t)width / 2, (int32_t)height / 2},
                                {(int32_t)width / 3, (int32_t)height * 2 / 3}};
        int32_t choices[3] = {0, 1, 2},
                targets[39][2] = {
                    {20, 20},
                    {50, 60},
                    {70, 80},
                    {110, 120},
                    {(int32_t)width * 3 / 4, (int32_t)height / 4}};
        int32_t alternate[2] = {(int32_t)width / 2, (int32_t)height / 5};
        BkClipTiming timing = {.start = 100, .end = 120, .source = 100};
        BkEndingUiHintsBindings hb = {&f,    &c,        &a,      &gate,
                                      &clip, points,    choices, targets,
                                      39,    alternate, &gauge,  &timing};
        BkEndingNormalConfig config;
        CHECK(bk_ending_normal_config(&config, group, 1));
        int32_t unavailable[2] = {0};
        uint8_t item = 1;
        float worlds[39][16], camera_position[3] = {0, 0, -100};
        uint8_t present[39];
        memset(present, 1, sizeof(present));
        for (unsigned i = 0; i < 39; i++) {
          memcpy(worlds[i], bk_identity, 64);
          worlds[i][12] = i * 10;
          worlds[i][13] = (i % 4) * 20;
          if (selectors) {
            targets[i][0] = 20 + (i * 13) % width;
            targets[i][1] = 20 + (i * 7) % height;
          }
        }
        BkEndingUiPickBindings pick = {
            worlds,      present,     39,          camera_position,
            bk_identity, bk_identity, bk_identity, ui.sprites[50].rect[2]};
        BkEndingUiSelectBindings sb = {
            &f,      &c, &a,          &gate, &clip,       &open, config.actions,
            targets, 39, bk_identity, &pick, unavailable, &item};
        BkEndingUiSelectOps select_ops = {&audio, key, voice};
        BkEndingUiHoverOps hover = {NULL, key};
        for (unsigned tick = 0; tick < 24; tick++) {
          float pointer[2] = {(float)points[tick % 3][0],
                              (float)points[tick % 3][1]},
                motion[2] = {2400, -1600};
          BkEndingUiFrame all = {0}, base_frame = {0}, stage_frame = {0};
          unsigned owners[128], indices[128];
          uint8_t visible = 0;
          c.hover = (int32_t[]){12, 17, 59, 61}[tick % 4];
          for (unsigned j = 0; j < 6; j++)
            c.pause_flags[j] = (tick + j) % 3 == 0;
          a.progress = (tick % 6) / 5.f;
          gauge = (float)(840.0 * scale);
          clip = kind == 3 ? (int32_t[]){7, 9, 2}[tick % 3]
                           : (int32_t[]){2, 6, 7, 10, 11, 17, 18}[tick % 7];
          timing.source = timing.start + (tick % 12) * 2;
          f.camera_event = kind == 2 ? (tick % 3 == 0 ? 0 : 2) : 1;
          f.camera_cached = kind == 1 ? 0 : 4;
          if (selectors) {
            f.state_721ee4 = (tick % 4) ? 2 : 3;
            c.state_721eec = (tick % 4) ? 2 : 3;
            a.gate = (tick % 4) ? 2 : 3;
            gate = (tick % 4) ? 1 : 3;
            if (kind == 3)
              f.camera_cached = (int32_t[]){-1, 6, config.actions[0],
                                            config.actions[5]}[tick % 4];
            /* Hints require a real target only at stage3_state3. */
            if (kind == 3 && gate == 3)
              f.camera_cached = 6;
            audio.consumed = audio.submitted;
            CHECK(bk_audio_poll(mix, e));
            BkEndingAudioCall call = {.operation = tick % 8 < 4
                                                       ? BK_ENDING_AUDIO_RESTART
                                                       : BK_ENDING_AUDIO_PAUSE,
                                      .slot = 1,
                                      .volume = -1000};
            int playing;
            CHECK(
                bk_ending_audio_call(audio.audio, 0, 0, 0, &call, &playing, e));
          }
          for (unsigned j = 0; j < 3; j++)
            choices[j] = (tick + j) % (kind == 1 ? 6 : kind == 4 ? 1 : 4);
          CHECK(bk_ending_ui_hints_start(&hints, scale, e));
          CHECK(bk_ending_ui_toolbar(&ui, &stage, &tb, pointer, scale, .13f,
                                     &hover, &visible, &all, e));
          CHECK(bk_ending_ui_hints(&ui, &stage, &hints, &hb, pointer, motion,
                                   scale, .13f, &all, e));
          int32_t selected = (int32_t)(tick % 9);
          if (selectors) {
            CHECK(bk_ending_ui_select(&ui, &stage, &sb, pointer, .13f, visible,
                                      &select_ops, &selected, &all, e));
            CHECK(bk_audio_fill(mix, e));
          }
          if (cursors) {
            for (unsigned j = 0; j < 4; j++)
              notices.notices[j] = (tick + j * 3) % 16 < 10;
            for (unsigned j = 0; j < 2; j++)
              notices.popups[j] = (tick + j * 4) % 14 < 8;
            unsigned first = all.count;
            CHECK(bk_ending_ui_cursor(&ui, &stage, &cb, selected, visible,
                                      pointer, .13f, &all, e));
            for (unsigned i = first; i < all.count; i++) {
              cursor_draws += all.draws[i].slot < 9;
              popup_draws += all.draws[i].slot >= 72;
              notice_draws +=
                  all.draws[i].slot >= 53 && all.draws[i].slot <= 56;
            }
          }
          for (unsigned i = 0; i < all.count; i++) {
            const BkEndingUiDraw *d = &all.draws[i];
            owners[i] = d->slot >= 63 || d->slot == 8;
            BkEndingUiFrame *owner = owners[i] ? &stage_frame : &base_frame;
            indices[i] = owner->count;
            owner->draws[owner->count++] = *d;
            lines += d->slot == 63;
            rings += d->slot == 50;
            meters += d->slot == 71;
          }
          draws += all.count;
          CHECK(bk_ending_ui_render_prepare(base_render, &base_frame, width,
                                            height, e));
          CHECK(bk_ending_ui_render_prepare(stage_render, &stage_frame, width,
                                            height, e));
          BkEndingUi saved_ui = ui;
          BkEndingStageUi saved_stage = stage;
          BkEndingUiHintsState saved_hints = hints;
          for (unsigned pass = 0; pass < (tick % 8 == 0 ? 2u : 1u); pass++) {
            CHECK(bk_renderer_begin(r, e));
            CHECK(bk_renderer_viewport(r, NULL, e));
            CHECK(bk_renderer_draw(r, bg, quad, 6, bk_identity, e));
            CHECK(bk_renderer_viewport(r, &viewport, e));
            for (unsigned i = 0; i < all.count; i++)
              CHECK(bk_ending_ui_render_draw_range(
                  owners[i] ? stage_render : base_render, indices[i], 1, e));
            CHECK(bk_renderer_end(r, e));
            CHECK(bk_renderer_readback(r, pass ? again : pixels, W * H * 4, e));
            if (pass) {
              CHECK(!memcmp(pixels, again, W * H * 4));
              redraws++;
            }
          }
          CHECK(!memcmp(&ui, &saved_ui, sizeof(ui)) &&
                !memcmp(&stage, &saved_stage, sizeof(stage)) &&
                !memcmp(&hints, &saved_hints, sizeof(hints)));
          CHECK(same_alloc(loaded, bk_renderer_stats(r)));
          for (unsigned y = 1; y < height; y += 5)
            for (unsigned x = 1; x < width; x += 5) {
              int rgb[3];
              if (!expected(images, &all, x, y, rgb))
                continue;
              for (unsigned channel = 0; channel < 3; channel++) {
                unsigned got =
                    pixels[((size_t)(y + viewport.y) * W + x + viewport.x) * 4 +
                           channel];
                unsigned delta = abs((int)got - rgb[channel]);
                if (delta > worst)
                  worst = delta;
                if (delta > 2) {
                  snprintf(e, 256, "profile%u tick%u xy%u,%u c%u got%u want%d",
                           profiles, tick, x, y, channel, got, rgb[channel]);
                  goto done;
                }
                hash = (hash ^ got) * UINT64_C(1099511628211);
                samples++;
              }
            }
          frames++;
        }
        bk_ending_ui_render_destroy(stage_render);
        stage_render = NULL;
        for (unsigned i = 0; i < 75; i++)
          if (bk_ending_stage_ui_image(&stage, i))
            bk_image_free(&images[i]);
        CHECK(bk_ending_stage_ui_release(&ui, &stage, (BkEndingUiStageKind)kind,
                                         e));
        CHECK(same_alloc(base_loaded, bk_renderer_stats(r)));
        profiles++;
      }
  bk_ending_ui_render_destroy(base_render);
  base_render = NULL;
  CHECK(same_alloc(baseline, bk_renderer_stats(r)));
  printf("ending-ui-hints GPU PASS profiles=%u frames=%u draws=%u lines=%u "
         "rings=%u meters=%u samples=%u worst=%u redraws=%u hash=%016" PRIx64
         " allocation-stable=1\n",
         profiles, frames, draws, lines, rings, meters, samples, worst, redraws,
         hash);
  result = 0;
  if (cursors)
    printf("ending-ui-cursor GPU PASS cursors=%u popups=%u notices=%u\n",
           cursor_draws, popup_draws, notice_draws);
  if (selectors)
    printf("ending-ui-select GPU/PCM PASS status=%u samples=%" PRIu64
           " pcm_hash=%016" PRIx64 "\n",
           audio.status_calls, audio.samples, audio.hash);
done:
  if (result)
    fprintf(stderr, "ending-ui-hints GPU FAIL: %s\n", e);
  bk_ending_ui_render_destroy(stage_render);
  bk_ending_ui_render_destroy(base_render);
  for (unsigned i = 0; i < 75; i++)
    bk_image_free(&images[i]);
  bk_blob_free(&raw);
  bk_texture_destroy(r, bg);
  bk_renderer_destroy(r);
  if (audio.audio)
    bk_ending_audio_stop(audio.audio, e);
  bk_ending_audio_destroy(audio.audio);
  bk_audio_destroy(mix);
  bk_resources_destroy(store);
  free(pixels);
  free(again);
  return result;
}
