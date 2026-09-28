/* Real five introduction scripts from entry through natural range completion.
 * Automated page input and an offline consumed-PCM clock; no Switch device or
 * Windows screenshot claim. Never writes original resources or displays them.
 */
#include "scene/dialogue_session.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#define W 320
#define H 240
#define CHECK(v)                                                               \
  do {                                                                         \
    if (!(v))                                                                  \
      goto done;                                                               \
  } while (0)
typedef struct {
  uint64_t consumed, submitted, samples, nonzero, hash;
} Sink;
static int submit(void *p, const int16_t *samples, size_t frames, char e[256]) {
  (void)e;
  Sink *s = p;
  for (size_t i = 0; i < frames * 2; i++) {
    uint16_t v = (uint16_t)samples[i];
    s->nonzero += v != 0;
    s->hash = (s->hash ^ (v & 255)) * UINT64_C(1099511628211);
    s->hash = (s->hash ^ (v >> 8)) * UINT64_C(1099511628211);
  }
  s->samples += frames * 2;
  s->submitted += frames;
  return 1;
}
static int poll(void *p, uint64_t *out, char e[256]) {
  (void)e;
  *out = ((Sink *)p)->consumed;
  return 1;
}
typedef struct {
  BkDialogueSession *session;
  unsigned transitions, unlocks;
} Output;
static int schedule(void *p, uint8_t target, uint8_t mode, char e[256]) {
  Output *o = p;
  if (target != 2 || mode != 1 || o->transitions) {
    snprintf(e, 256, "unexpected introduction destination %u/%u", target, mode);
    return 0;
  }
  if (!bk_dialogue_session_stop(o->session, e))
    return 0;
  o->transitions++;
  return 1;
}
static int unlock(void *p, unsigned group, char e[256]) {
  (void)group;
  ((Output *)p)->unlocks++;
  snprintf(e, 256, "new-game introduction must not invoke persistent unlock");
  return 0;
}
int main(int argc, char **argv) {
  if (argc != 2)
    return 2;
  char e[256] = {0}, path[1024];
  int rc = 1;
  BkResourceStore *store = bk_resources_create(e);
  BkRenderer *r = NULL;
  BkAudio *audio = NULL;
  BkCurtainRender *curtain = NULL;
  BkDialogueSession *session = NULL;
  uint8_t *pixels = malloc(W * H * 4), *repeat = malloc(W * H * 4);
  Sink sink = {.hash = UINT64_C(1469598103934665603)};
  BkDialogueSessionState state = {0};
  BkMenuCamera camera = {0};
  BkCommonHudState common = {0};
  BkFadeSprite background;
  bk_fade_sprite_initialize(&background);
  uint8_t wanted = 0;
  BkDialogueResult result = {0};
  BkVoiceEnvelope envelope = {0};
  uint32_t random = 12345;
  uint64_t rgba = UINT64_C(1469598103934665603);
  unsigned frames = 0, pages = 0, visible = 0, mouth = 0, redraws = 0,
           phase_mask = 0, transitions = 0;
  CHECK(store && pixels && repeat);
  const char *packs[] = {"bk3_00", "bk3_01", "bk3_02", "bk3_05", "bk3_06"};
  for (unsigned i = 0; i < 5; i++) {
    snprintf(path, sizeof(path), "%s/%s.pp", argv[1], packs[i]);
    CHECK(bk_resources_mount(store, packs[i], path, e));
  }
  CHECK(bk_resources_mount_directory(store, "faces", argv[1], 20480, e));
  CHECK(bk_resources_mount_directory(store, "fonts", argv[1], 8 * 1024 * 1024,
                                     e));
  r = bk_renderer_create(W, H, stderr, e);
  CHECK(r);
  curtain = bk_curtain_render_create(r, store, e);
  CHECK(curtain);
  audio =
      bk_audio_create(&(BkAudioSink){&sink, 48000, 480, 4800, submit, poll}, e);
  CHECK(audio);
  BkRenderStats baseline = bk_renderer_stats(r);
  static const int32_t ends[] = {10200, 20205, 30228, 40144, 50033};
  for (unsigned group = 0; group < 5; group++) {
    Output out = {0};
    common.curtain = (BkFadeSprite){1, 2, 3};
    common.blocked = 0;
    uint32_t now = 1000 + frames * 50;
    BkDialogueSessionConfig config = {
        .state = &state,
        .camera = &camera,
        .common = &common,
        .common_render = curtain,
        .backdrop_curtain = &background,
        .backdrop_curtain_wanted = &wanted,
        .result = &result,
        .envelope = &envelope,
        .random = &random,
        .context = &out,
        .unlock = unlock,
        .schedule = schedule,
        .group = group,
        .width = W,
        .height = H,
        .music_voice = 60,
        .speech_voice = 61,
        .area = 0,
        .previous = 0x38,
        .clocks = {now, now + 1, now + 2, now + 3}};
    session = bk_dialogue_session_create(r, store, audio, &config, e);
    CHECK(session);
    out.session = session;
    int32_t last_label = -1, final_label = -1;
    unsigned group_frames = 0;
    for (; group_frames < 6000 && !out.transitions; group_frames++) {
      now = 1000 + frames * 50;
      BkDialogueSessionInput input = {
          .seconds = .1f,
          .timestamp_ms = now,
          .timer_clock_ms = now + 1,
          .face_clocks = {now + 2, now + 3, now + 4},
          .advance = group_frames % 3 == 2,
          .music_volume = 0,
          .voice_volume = 0};
      sink.consumed += 2400;
      if (sink.consumed > sink.submitted)
        sink.consumed = sink.submitted;
      CHECK(bk_audio_poll(audio, e));
      CHECK(bk_dialogue_session_step(session, &input, e));
      CHECK(state.timer.duration == 1000);
      if (state.script.state.current_label != last_label) {
        pages++;
        last_label = state.script.state.current_label;
      }
      final_label = state.script.state.current_label;
      visible += state.actor.visibility > 0;
      mouth += state.actor.mouth > 0;
      if (state.phase < 32)
        phase_mask |= 1u << state.phase;
      CHECK(bk_audio_fill(audio, e));
      CHECK(bk_renderer_begin(r, e) && bk_dialogue_session_draw(session, e) &&
            bk_renderer_end(r, e));
      CHECK(bk_renderer_readback(r, pixels, W * H * 4, e));
      for (unsigned i = 0; i < W * H * 4; i++)
        rgba = (rgba ^ pixels[i]) * UINT64_C(1099511628211);
      if (group_frames == 20) {
        BkDialogueSessionState old = state;
        CHECK(bk_renderer_begin(r, e) && bk_dialogue_session_draw(session, e) &&
              bk_renderer_end(r, e));
        CHECK(bk_renderer_readback(r, repeat, W * H * 4, e));
        CHECK(!memcmp(pixels, repeat, W * H * 4) &&
              !memcmp(&old, &state, sizeof(old)));
        redraws++;
      }
      CHECK(bk_dialogue_session_after_present(session, e));
      frames++;
    }
    CHECK(out.transitions == 1 && out.unlocks == 0 &&
          final_label == ends[group]);
    CHECK(bk_dialogue_session_released(session) && !state.script.raw.data);
    transitions += out.transitions;
    fprintf(stderr, "dialogue group%u complete: %u frames label%d\n", group,
            group_frames, final_label);
    bk_dialogue_session_destroy(session);
    session = NULL;
    BkRenderStats after = bk_renderer_stats(r);
    CHECK(after.live_allocations == baseline.live_allocations &&
          after.live_bytes == baseline.live_bytes);
  }
  fprintf(stderr,
          "observed visible%u mouth%u nonzero%llu redraws%u transitions%u\n",
          visible, mouth, (unsigned long long)sink.nonzero, redraws,
          transitions);
  CHECK(visible > 100 && mouth > 0 && sink.nonzero > 0 && redraws == 5 &&
        transitions == 5);
  printf("PASS dialogue session: %u frames %u pages %u visible %u mouth %u "
         "redraws %u transitions phases%08x; RGBA %016" PRIx64
         " PCM %016" PRIx64 " samples%" PRIu64 "\n",
         frames, pages, visible, mouth, redraws, transitions, phase_mask, rgba,
         sink.hash, sink.samples);
  rc = 0;
done:
  if (rc)
    fprintf(stderr, "dialogue session probe failed: %s (frames%u pages%u)\n", e,
            frames, pages);
  bk_dialogue_session_destroy(session);
  bk_audio_destroy(audio);
  bk_curtain_render_destroy(curtain);
  bk_renderer_destroy(r);
  bk_resources_destroy(store);
  free(pixels);
  free(repeat);
  return rc;
}
