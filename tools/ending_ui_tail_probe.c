/* Real five FAM actors/eyes, PCM and packaged notice images. Controller
 * states below are explicit fixtures; no claim of a complete flow16 entry. */
#include "ending_ui_render_oracle.h"
#include "scene/ending_auxiliary.h"
#include "scene/ending_ui_render.h"
#include <inttypes.h>
#include <stdio.h>
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
  uint64_t submitted, consumed, samples, hash;
} Sink;
static uint64_t digest(uint64_t h, const void *d, size_t n) {
  const uint8_t *p = d;
  for (size_t i = 0; i < n; i++)
    h = (h ^ p[i]) * UINT64_C(1099511628211);
  return h;
}
static int submit(void *p, const int16_t *pcm, size_t frames, char e[256]) {
  (void)e;
  Sink *s = p;
  s->hash = digest(s->hash, pcm, frames * 4);
  s->samples += frames * 2;
  s->submitted += frames;
  return 1;
}
static int poll(void *p, uint64_t *out, char e[256]) {
  (void)e;
  *out = ((Sink *)p)->consumed;
  return 1;
}
static int same_alloc(BkRenderStats a, BkRenderStats b) {
  return a.live_allocations == b.live_allocations &&
         a.live_bytes == b.live_bytes;
}
int main(int argc, char **argv) {
  if (argc != 2)
    return 2;
  int result = 1;
  char e[256] = {0}, path[1024], name[32];
  BkResourceStore *store = NULL;
  BkRenderer *r = NULL;
  BkTexture *bg = NULL;
  BkEndingUiRender *render = NULL;
  BkAudio *mix = NULL;
  BkEndingAudio *audio = NULL;
  BkModel *model = NULL;
  BkClipSet *clips = NULL;
  BkActorPose *actor = NULL;
  BkEyeAssets *eyes = NULL;
  BkBlob raw = {0};
  BkImage images[75] = {0};
  Sink sink = {.hash = UINT64_C(14695981039346656037)};
  uint8_t *pixels = malloc(W * H * 4), *again = malloc(W * H * 4);
  uint64_t hash = UINT64_C(14695981039346656037),
           state_hash = UINT64_C(14695981039346656037);
  unsigned frames = 0, draws = 0, samples = 0, worst = 0, redraws = 0,
           named = 0, chains = 0;
  CHECK(pixels && again);
  store = bk_resources_create(e);
  CHECK(store);
  const char *packs[] = {"bk3_00", "bk3_02", "bk3_06", "bk3_08", "fambom"};
  for (unsigned i = 0; i < 5; i++) {
    snprintf(path, sizeof(path), "%s/%s.pp", argv[1], packs[i]);
    CHECK(bk_resources_mount(store, packs[i], path, e));
  }
  BkAudioSink output = {&sink, 48000, 480, 1920, submit, poll};
  mix = bk_audio_create(&output, e);
  CHECK(mix);
  audio = bk_ending_audio_create(store, mix, 50, e);
  CHECK(audio);
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
  render = bk_ending_ui_render_create(r, store, e);
  CHECK(render);
  for (unsigned slot = 52; slot <= 56; slot++) {
    CHECK(bk_resources_read(store, "bk3_00", bk_ending_ui_image(slot), &raw,
                            e) == BK_RESOURCE_OK);
    CHECK(bk_image_decode(raw.data, raw.size, &images[slot], e));
    bk_blob_free(&raw);
  }
  BkRenderStats loaded = bk_renderer_stats(r);
  for (unsigned group = 0; group < 5; group++) {
    snprintf(name, sizeof(name), "h%02u_00.fam", group + 1);
    CHECK(bk_resources_read(store, "fambom", name, &raw, e) == BK_RESOURCE_OK);
    BkFaceConfig config;
    CHECK(bk_face_config_decode(raw.data, raw.size, &config, e));
    bk_blob_free(&raw);
    CHECK(bk_resources_read(store, "bk3_08", config.actor_clip, &raw, e) ==
          BK_RESOURCE_OK);
    clips = bk_clip_set_decode(raw.data, raw.size, e);
    bk_blob_free(&raw);
    CHECK(clips);
    CHECK(bk_resources_read(store, "bk3_08", bk_clip_model_name(clips), &raw,
                            e) == BK_RESOURCE_OK);
    CHECK(bk_model_decode(raw.data, raw.size, &model, e) == BK_MODEL_OK);
    bk_blob_free(&raw);
    uint32_t root = BK_MODEL_NONE;
    for (uint32_t i = 0; i < model->frame_count; i++)
      if (model->frames[i].parent_index == BK_MODEL_NONE) {
        CHECK(root == BK_MODEL_NONE);
        root = i;
      }
    CHECK(root != BK_MODEL_NONE);
    actor = bk_actor_pose_create(model, clips, root, NULL, (float[3]){0}, 0, 4,
                                 0, e);
    CHECK(actor);
    eyes = bk_eye_assets_create(store, "bk3_08", model,
                                bk_clip_model_name(clips), &config, e);
    CHECK(eyes);
    BkEndingAuxiliaryServices services = {actor, eyes, audio};
    for (unsigned profile = 0; profile < 5; profile++) {
      unsigned width = group % 2 ? 503 : 640, height = group % 2 ? 377 : 480;
      float scale = (float)((double)width / 1280), gauge;
      BkViewport viewport = {(W - width) / 2, (H - height) / 2, width, height};
      BkEndingUi ui = {0};
      BkEndingStageUi stage = {0};
      BkEndingUiNormalNotice normal = {0};
      BkEndingUiAuxNotice aux = {0};
      BkEndingUiNoticeState notices = {0};
      BkEndingFrameState f = {.phase = profile < 2   ? 1
                                       : profile < 4 ? 6
                                                     : 8,
                              .group = group,
                              .camera_cached = 11};
      BkEndingControlState c = {0};
      c.toggles[7] = 1;
      BkEndingAuxiliaryState a = {
          .gate = 3, .variant = 0, .selection = profile % 3, .progress = .7f};
      CHECK(bk_ending_ui_initialize(&ui, width, c.pause_flags, &gauge, e));
      int8_t side = 0, final = 9;
      int32_t target = 0, inputs[14] = {0}, processed[14] = {0}, contact = 2,
              volume = -700, aux_inputs[2] = {0}, cfg[5][6] = {{0}};
      uint8_t flash = 1;
      uint32_t random = group + 31;
      char names[2][32] = {{0}};
      BkEndingUiTailBindings b = {.frame = &f,
                                  .control = &c,
                                  .auxiliary = &a,
                                  .notices = &notices,
                                  .flash_wanted = &flash,
                                  .final_state = &final,
                                  .normal_side = &side,
                                  .normal_target = &target,
                                  .normal_inputs = inputs,
                                  .normal_processed = processed,
                                  .gauge_y = &gauge,
                                  .contact_index = &contact,
                                  .aux_inputs = aux_inputs,
                                  .aux_config = cfg,
                                  .random = &random,
                                  .speech_names = names,
                                  .voice_volume = &volume};
      for (unsigned tick = 0; tick < 32; tick++) {
        sink.consumed = sink.submitted;
        CHECK(bk_audio_poll(mix, e));
        if (tick % 8 == 0) {
          if (profile < 2) {
            notices.notices[profile ? 2 : 0] = 1;
            ui.sprites[profile ? 55 : 53].transform.fade.stage = 3;
            normal.frame = 0;
            normal.cycles = 0;
            a.pending = 0;
            if (profile) {
              BkEndingAudioCall stop = {.operation = BK_ENDING_AUDIO_PAUSE,
                                        .slot = 1};
              int playing;
              CHECK(
                  bk_ending_audio_call(audio, group, 0, 0, &stop, &playing, e));
            }
          } else if (profile == 2) {
            aux.mode = 6;
            aux.once = 1;
            aux.sequence_count = 0;
            aux.sequence = 0;
            a.pending = 0;
            notices.notices[0] = 1;
            ui.sprites[53].transform.fade.stage = 3;
            CHECK(bk_actor_pose_request_mode(actor, tick % 16 ? 10 : 6,
                                             BK_CLIP_REQUEST_CONFIGURED, e));
          } else if (profile == 3) {
            aux.mode = 2;
            aux.choice = (tick / 8) % 2;
            aux.cycles = 0;
            notices.notices[0] = 1;
            ui.sprites[53].transform.fade.stage = 3;
            aux.group_seen[group] = 0;
          } else {
            flash = 1;
            ui.sprites[52].transform.fade.stage = 0;
            ui.sprites[52].transform.fade.alpha = 0;
          }
        }
        BkEndingUiFrame frame = {0};
        /* Earlier cursor section advances the shared notices in this order. */
        const unsigned order[] = {54, 53, 56, 55};
        for (unsigned j = 0; j < 4; j++) {
          unsigned slot = order[j];
          CHECK(bk_fade_sprite_request(&ui.sprites[slot].transform.fade,
                                       notices.notices[slot - 53]));
          CHECK(
              bk_ending_ui_dispatch_sprite(&ui, &stage, slot, .13f, &frame, e));
        }
        CHECK(bk_ending_ui_tail_apply(&services, &ui, &stage, &normal, &aux, &b,
                                      scale, .13f, &frame, e));
        for (unsigned i = 0; i < 2; i++)
          named += names[i][0] != 0;
        if (profile == 2 && aux.mode == 4) {
          int32_t chain, next;
          CHECK(bk_actor_pose_clip_link(actor, 15, &chain, &next) &&
                chain == 1);
          CHECK(bk_actor_pose_clip_link(actor, 16, &chain, &next) &&
                chain == 1);
          chains++;
        }
        CHECK(bk_actor_pose_advance(actor, -1, .016f, e));
        bk_actor_pose_publish(actor);
        BkClipState clock;
        CHECK(bk_actor_pose_state(actor, &clock));
        state_hash = digest(state_hash, &clock, sizeof(clock));
        state_hash = digest(state_hash, &normal, sizeof(normal));
        state_hash = digest(state_hash, &aux, sizeof(aux));
        CHECK(bk_audio_fill(mix, e));
        CHECK(bk_ending_ui_render_prepare(render, &frame, width, height, e));
        BkEndingUi saved = ui;
        for (unsigned pass = 0; pass < (tick % 8 == 0 ? 2u : 1u); pass++) {
          CHECK(bk_renderer_begin(r, e));
          CHECK(bk_renderer_viewport(r, NULL, e));
          CHECK(bk_renderer_draw(r, bg, quad, 6, bk_identity, e));
          CHECK(bk_renderer_viewport(r, &viewport, e));
          CHECK(bk_ending_ui_render_draw(render, e));
          CHECK(bk_renderer_end(r, e));
          CHECK(bk_renderer_readback(r, pass ? again : pixels, W * H * 4, e));
          if (pass) {
            CHECK(!memcmp(pixels, again, W * H * 4));
            redraws++;
          }
        }
        CHECK(!memcmp(&saved, &ui, sizeof(ui)) &&
              same_alloc(loaded, bk_renderer_stats(r)));
        for (unsigned y = 1; y < height; y += 5)
          for (unsigned x = 1; x < width; x += 5) {
            int rgb[3];
            if (!expected(images, &frame, x, y, rgb))
              continue;
            for (unsigned ch = 0; ch < 3; ch++) {
              unsigned
                  got = pixels[((size_t)(y + viewport.y) * W + x + viewport.x) *
                                   4 +
                               ch],
                  delta = abs((int)got - rgb[ch]);
              if (delta > worst)
                worst = delta;
              CHECK(delta <= 2);
              hash = (hash ^ got) * UINT64_C(1099511628211);
              samples++;
            }
          }
        draws += frame.count;
        frames++;
      }
      CHECK(bk_ending_audio_stop(audio, e));
    }
    bk_eye_assets_destroy(eyes);
    eyes = NULL;
    bk_actor_pose_destroy(actor);
    actor = NULL;
    bk_model_destroy(model);
    model = NULL;
    bk_clip_set_destroy(clips);
    clips = NULL;
  }
  CHECK(named > 100 && chains > 30);
  static const char *const absent[] = {
#include "game/ending_absent_speech.inc"
  };
  for (unsigned i = 0; i < sizeof(absent) / sizeof(*absent); i++) {
    for (unsigned slot = 0; slot < 2; slot++) {
      CHECK(bk_ending_audio_speech(audio, slot, "PH10217.wav", -700, e));
      CHECK(bk_ending_audio_speech(audio, slot, absent[i], -700, e));
      int present = 1;
      CHECK(bk_ending_audio_present(audio, slot, &present) && !present);
    }
  }
  CHECK(!bk_ending_audio_speech(audio, 0, "missing-not-a-native-cue.wav", -700,
                                e));
  *e = 0;
  char scratch[] = "/tmp/bk-ending-tail-XXXXXX";
  CHECK(mkdtemp(scratch));
  snprintf(path, sizeof(path), "%s/PH10218.wav", scratch);
  FILE *bad = fopen(path, "wb");
  CHECK(bad);
  fputs("corrupt PCM", bad);
  fclose(bad);
  int mounted = bk_resources_mount_directory(store, "bk3_06", scratch, 1024, e);
  int rejected =
      mounted && !bk_ending_audio_speech(audio, 0, "PH10218.wav", -700, e);
  unlink(path);
  rmdir(scratch);
  CHECK(rejected);
  *e = 0;
  bk_ending_ui_render_destroy(render);
  render = NULL;
  CHECK(same_alloc(baseline, bk_renderer_stats(r)));
  printf("ending UI tail GPU/PCM PASS frames=%u draws=%u samples=%u worst=%u "
         "redraws=%u named=%u chains=%u rgba=%016" PRIx64 " state=%016" PRIx64
         " pcm_samples=%" PRIu64 " pcm=%016" PRIx64 " allocation-stable=1\n",
         frames, draws, samples, worst, redraws, named, chains, hash,
         state_hash, sink.samples, sink.hash);
  result = 0;
done:
  if (result)
    fprintf(stderr, "ending UI tail probe FAIL frame%u: %s\n", frames, e);
  if (audio) {
    char ignored[256];
    bk_ending_audio_stop(audio, ignored);
  }
  bk_ending_audio_destroy(audio);
  bk_audio_destroy(mix);
  bk_eye_assets_destroy(eyes);
  bk_actor_pose_destroy(actor);
  bk_model_destroy(model);
  bk_clip_set_destroy(clips);
  bk_blob_free(&raw);
  bk_ending_ui_render_destroy(render);
  for (unsigned i = 0; i < 75; i++)
    bk_image_free(&images[i]);
  bk_texture_destroy(r, bg);
  bk_renderer_destroy(r);
  bk_resources_destroy(store);
  free(pixels);
  free(again);
  return result;
}
