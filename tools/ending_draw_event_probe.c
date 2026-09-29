/* Production draw adapter with explicitly controlled phase/mode/audio
 * fixtures. The real scene owns resources, geometry, viewports and mixer;
 * this is not a natural controller walkthrough or a production cheat API. */
#include "../runtime/scene/ending_normal_session.c"

#define CHECK(x) do { if (!(x)) { \
  fprintf(stderr, "ending draw group%u variant%u line%d (%s): %s\n", \
          group, variant, __LINE__, #x, e); goto done; } } while (0)

static unsigned profiles, frames, dual_views, empty_views, gain_checks, redraws;
typedef struct { uint64_t submitted; } Sink;
static int submit(void *p, const int16_t *pcm, size_t n, char e[256]) {
  (void)pcm; (void)e;
  ((Sink *)p)->submitted += n;
  return 1;
}
static int poll(void *p, uint64_t *n, char e[256]) {
  (void)e;
  *n = ((Sink *)p)->submitted;
  return 1;
}
static int event_draw_snapshot(BkRenderer *renderer, EndingNormalScene *s,
                                char e[256]) {
  return bk_renderer_begin(renderer, e) &&
         bk_renderer_viewport(renderer, &s->viewport, e) &&
         bk_ending_normal_render_draw(s->render, e) &&
         bk_renderer_end(renderer, e);
}
static int sample(BkRenderer *renderer, EndingNormalScene *s,
                    unsigned phase, int32_t mode, int playing, char e[256]) {
  s->state->frame.phase = (int32_t)phase;
  s->state->control.mode_721ec4 = mode;
  s->active_seconds = .125f;
  s->now_ms += 125;
  uint32_t movie = bk_ending_normal_render_movie_frame(s->render);
  int32_t volume, pan;
  if (!bk_audio_get_gain(s->services.audio, 0, &volume, &pan))
    return fail(e, "missing commanded voice gain");
  int32_t latch = *s->duck_transition;
  if (!prepare_scene_draw(s, e))
    return 0;
  unsigned regular = phase == 7 ? 0 : phase == 1 ? 3 : 2;
  int dual = phase != 7 && (mode == 1 || (mode == 0 && playing));
  unsigned expected = regular + (dual ? regular - 1 : 0);
  if (bk_ending_normal_render_pass_count(s->render) != expected)
    return fail(e, "wrong live phase/mode/speech1 draw dispatch");
  for (unsigned i = 0; i < expected; i++)
    if (bk_ending_normal_render_pass_view(s->render, i) != (i >= regular))
      return fail(e, "draw pass used the wrong viewport snapshot");
  if (phase == 7 || mode < 0 || mode > 2) {
    int32_t after_volume, after_pan;
    if (!bk_audio_get_gain(s->services.audio, 0, &after_volume, &after_pan) ||
        after_volume != volume || after_pan != pan ||
        *s->duck_transition != latch ||
        (phase == 7 && movie != bk_ending_normal_render_movie_frame(s->render)))
      return fail(e, "suppressed event changed retained audio/video");
  }
  if (!event_draw_snapshot(renderer, s, e))
    return 0;
  uint8_t first[64 * 48 * 4], again[sizeof(first)];
  if (!bk_renderer_readback(renderer, first, sizeof(first), e))
    return 0;
  if (!expected)
    empty_views++;
  /* Pure redraws consume neither the event stage nor the native camera
   * setters. Pixel equality also catches replaying mutable BOM twice. */
  BkMenuCamera camera = s->camera;
  uint32_t rng = *s->random;
  movie = bk_ending_normal_render_movie_frame(s->render);
  latch = *s->duck_transition;
  if (!bk_audio_get_gain(s->services.audio, 0, &volume, &pan) ||
      !event_draw_snapshot(renderer, s, e) ||
      !bk_renderer_readback(renderer, again, sizeof(again), e))
    return 0;
  int32_t after_volume, after_pan;
  if (!bk_audio_get_gain(s->services.audio, 0, &after_volume, &after_pan) ||
      memcmp(first, again, sizeof(first)) ||
      memcmp(&camera, &s->camera, sizeof(camera)) || *s->random != rng ||
      movie != bk_ending_normal_render_movie_frame(s->render) ||
      *s->duck_transition != latch || after_volume != volume || after_pan != pan)
    return fail(e, "pure redraw changed picture, camera, movie or audio");
  frames++;
  redraws++;
  dual_views += dual;
  return 1;
}
static int restart(EndingNormalScene *s, unsigned slot, int32_t volume,
                     char e[256]) {
  BkEndingAudioCall command = {.operation = BK_ENDING_AUDIO_RESTART,
                                .slot = slot, .flags = 1, .volume = volume};
  int playing = 0;
  return bk_ending_audio_call(s->audio, s->group, s->variant, 0,
                               &command, &playing, e);
}
static int profile(BkRenderer *renderer, BkResourceStore *store,
                    unsigned group, unsigned variant, char e[256]) {
  int ok = 0;
  Sink sink = {0};
  BkAudioSink output = {&sink, 48000, 240, 960, submit, poll};
  BkAudio *audio = NULL;
  BkScene *scene = NULL;
  BkRenderStats baseline = bk_renderer_stats(renderer);
  CHECK(audio = bk_audio_create(&output, e));
  BkSceneServices services = {store, renderer, NULL, audio, NULL};
  CHECK(scene = bk_ending_normal_scene_create(&services, group, variant, e));
  EndingNormalScene *s = bk_scene_custom_context(scene);
  CHECK(bk_renderer_begin(renderer, e) &&
          bk_scene_draw(scene, &(BkSceneFrame){0}, e) &&
          bk_renderer_end(renderer, e) &&
          bk_ending_normal_scene_after_present(scene, e));
  int present;
  CHECK(bk_ending_audio_present(s->audio, 1, &present) && !present);
  /* Only gain/status routing matters in these explicit fixtures. Use the
   * same real PCM effect in both slots, so no absent speech table entry can
   * masquerade as a stopped/present buffer. Parent voice choice is tested
   * separately by the real-input framing probe. */
  CHECK(bk_ending_audio_bind(s->audio, 0, "bk3_02", "se002.wav", e));
  CHECK(restart(s, 0, -1000, e));
  CHECK(bk_audio_gain(audio, 0, -1000, -321, e));
  for (int32_t mode = -1; mode <= 3; mode++) {
    CHECK(sample(renderer, s, 1, mode, 0, e));
    CHECK(sample(renderer, s, 9, mode, 0, e));
    CHECK(sample(renderer, s, 7, mode, 0, e));
  }
  CHECK(bk_ending_audio_bind(s->audio, 1, "bk3_02", "se002.wav", e));
  CHECK(bk_ending_audio_present(s->audio, 1, &present) && present);
  int playing;
  CHECK(bk_audio_playing(audio, 1, &playing) && !playing);
  CHECK(sample(renderer, s, 1, 0, 0, e));
  CHECK(restart(s, 1, -1700, e));
  CHECK(bk_audio_gain(audio, 1, -1700, 234, e));
  for (int32_t mode = -1; mode <= 3; mode++) {
    CHECK(sample(renderer, s, 1, mode, 1, e));
    CHECK(sample(renderer, s, 9, mode, 1, e));
    CHECK(sample(renderer, s, 7, mode, 1, e));
  }
  int32_t voice0, pan0, voice1, pan1;
  CHECK(bk_audio_get_gain(audio, 0, &voice0, &pan0));
  CHECK(voice0 < -1000 && voice0 >= -3000 && pan0 == -321);
  CHECK(bk_audio_get_gain(audio, 1, &voice1, &pan1));
  CHECK(voice1 == -1700 && pan1 == 234);
  CHECK(bk_audio_playing(audio, 0, &playing) && playing);
  gain_checks++;
  for (unsigned step = 0; step < 10; step++)
    CHECK(sample(renderer, s, 1, 2, 1, e));
  CHECK(bk_audio_get_gain(audio, 0, &voice0, &pan0) &&
          voice0 == -3000 && pan0 == -321 && !*s->duck_transition);
  gain_checks++;
  BkEndingAudioCall stop = {.operation = BK_ENDING_AUDIO_PAUSE, .slot = 1};
  CHECK(bk_ending_audio_call(s->audio, group, variant, 0, &stop, &playing, e));
  CHECK(bk_audio_playing(audio, 1, &playing) && !playing);
  for (unsigned step = 0; step < 10; step++)
    CHECK(sample(renderer, s, 1, 0, 0, e));
  CHECK(bk_audio_get_gain(audio, 0, &voice0, &pan0) &&
          voice0 == -1000 && pan0 == -321 && !*s->duck_transition);
  CHECK(bk_audio_get_gain(audio, 1, &voice1, &pan1) &&
          voice1 == -1700 && pan1 == 234);
  gain_checks++;
  /* The unused special rectangle must not break a regular/empty branch;
   * selecting it must fail instead of silently drawing the ordinary view. */
  unsigned saved_width = s->special_viewport.width;
  s->special_viewport.width = 0;
  CHECK(sample(renderer, s, 7, 1, 0, e));
  CHECK(sample(renderer, s, 1, 2, 0, e));
  s->state->control.mode_721ec4 = 1;
  CHECK(!prepare_scene_draw(s, e));
  CHECK(!bk_ending_normal_render_pass_count(s->render));
  s->special_viewport.width = saved_width;
  CHECK(sample(renderer, s, 1, 1, 0, e));
  bk_scene_destroy(scene);
  scene = NULL;
  BkRenderStats final = bk_renderer_stats(renderer);
  CHECK(final.live_allocations == baseline.live_allocations &&
          final.live_bytes == baseline.live_bytes);
  profiles++;
  ok = 1;
done:
  bk_scene_destroy(scene);
  bk_audio_destroy(audio);
  return ok;
}
int main(int argc, char **argv) {
  if (argc != 2)
    return 2;
  char e[256] = {0}, path[1024];
  BkRenderer *renderer = bk_renderer_create(64, 48, stdout, e);
  BkResourceStore *store = bk_resources_create(e);
  int result = 1;
  if (!renderer || !store)
    goto done;
  const char *packs[] = {"bk3_00", "bk3_02", "bk3_03", "bk3_04",
                          "bk3_06", "bk3_08", "bk3_18", "fambom"};
  for (unsigned i = 0; i < sizeof(packs) / sizeof(*packs); i++) {
    if (snprintf(path, sizeof(path), "%s/%s.pp", argv[1], packs[i]) >=
        (int)sizeof(path) || !bk_resources_mount(store, packs[i], path, e))
      goto done;
  }
  for (unsigned group = 0; group < 5; group++)
    for (unsigned variant = 0; variant < 2; variant++)
      if (!profile(renderer, store, group, variant, e))
        goto done;
  printf("PASS ending draw events: profiles%u frames%u dual%u empty%u "
         "gain_checks%u exact_redraws%u; real PCM status, retained duck, "
         "mode/phase dispatch and resource retirement\n",
         profiles, frames, dual_views, empty_views, gain_checks, redraws);
  result = 0;
done:
  if (result)
    fprintf(stderr, "%s\n", e);
  bk_resources_destroy(store);
  bk_renderer_destroy(renderer);
  return result;
}
