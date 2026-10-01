/*Resource-boundary fixture for actual normal/secondary stage replacement.
 * Only action6/next_mode/curtain request are injected. Phase, models, clocks,
 * controllers and all release/load callbacks use production code. This is
 * not evidence that natural input has reached the transition in a story. */
#include "../runtime/scene/ending_normal_session.c"

typedef struct { uint64_t submitted, consumed, hash; } StageSink;
typedef struct { unsigned entries, reloads, ui_reloads, redraws, frames; uint64_t pcm; } StageResult;
static uint64_t stage_hash(uint64_t h, const void *data, size_t n) {
  const unsigned char *p = data;
  for (size_t i = 0; i < n; ++i) h = (h ^ p[i]) * UINT64_C(1099511628211);
  return h;
}
static int stage_submit(void *context, const int16_t *samples, size_t frames,
                         char e[256]) {
  (void)e;
  StageSink *sink = context;
  sink->hash = stage_hash(sink->hash, samples, frames * 2 * sizeof(*samples));
  sink->submitted += frames;
  return 1;
}
static int stage_poll(void *context, uint64_t *consumed, char e[256]) {
  (void)e;
  StageSink *sink = context;
  uint64_t next = sink->consumed + 800;
  sink->consumed = next < sink->submitted ? next : sink->submitted;
  *consumed = sink->consumed;
  return 1;
}
static int stage_present(BkScene *scene, BkRenderer *renderer, char e[256]) {
  return bk_renderer_begin(renderer, e) &&
         bk_scene_draw(scene, &(BkSceneFrame){0}, e) &&
         bk_renderer_end(renderer, e) &&
         bk_ending_normal_scene_after_present(scene, e);
}
static int stage_tick(BkScene *scene, EndingNormalScene *s, StageResult *out,
                       char e[256]) {
  if (!bk_audio_poll(s->services.audio, e) ||
      !bk_scene_step(scene, 1.0 / 60.0, &(BkInput){0}, e) ||
      !bk_audio_fill(s->services.audio, e) ||
      !stage_present(scene, s->services.renderer, e)) return 0;
  ++out->frames;
  return 1;
}
static BkEndingBackgroundAssets *stage_background(EndingNormalScene *s) {
  return s->assets ? bk_ending_normal_assets_background(s->assets)
                   : bk_ending_secondary_assets_background(s->secondary_assets);
}
#define VERIFY(condition) do { if (!(condition)) { \
  fprintf(stderr, "stage reload group%u line%d (%s): %s\n", \
          group, __LINE__, #condition, e); goto done; } } while (0)
static int stage_profile(BkRenderer *renderer, BkResourceStore *store,
                           unsigned group, StageResult *out, char e[256]) {
  int ok = 0;
  BkScene *scene = NULL;
  BkAudio *audio = NULL;
  BkAudioClip *guard = NULL;
  BkEndingRecords *records = NULL;
  StageSink sink = {.hash = UINT64_C(14695981039346656037)};
  BkAudioSink output = {&sink, 48000, 480, 1920, stage_submit, stage_poll};
  VERIFY(audio = bk_audio_create(&output, e));
  BkSceneServices services = {store, renderer, stdout, audio, NULL, NULL};
  uint8_t unlocked[5][8] = {{0}};
  VERIFY(records = calloc(1, sizeof(*records)));
  VERIFY(scene = bk_ending_normal_scene_create_story(
      &services, group, 0, records, unlocked, NULL, e));
  EndingNormalScene *s = bk_scene_custom_context(scene);
  VERIFY(s && s->state->frame.phase == 1 && s->frame_active);
  VERIFY(bk_audio_fill(audio, e) && stage_present(scene, renderer, e));
  for (unsigned i = 0; i < 8; ++i) VERIFY(stage_tick(scene, s, out, e));
  VERIFY(guard = bk_audio_clip_load(store, "bk3_02", "se002.wav", e));
  unsigned width, height;
  bk_renderer_extent(renderer, &width, &height);
  VERIFY(width == 85 && height == 64);
  uint8_t before[85 * 64 * 4], after[sizeof(before)];
  BkRenderStats normal_stable = {0}, secondary_stable = {0};
  for (unsigned cycle = 0; cycle < 4; ++cycle) {
    const int to_normal = (cycle & 1) != 0;
    VERIFY(s->state->frame.phase == (to_normal ? 2 : 1));
    BkEndingBackgroundAssets *old_background = stage_background(s);
    const BkEndingBackgroundData *old_data =
        bk_ending_background_assets_data(old_background);
    VERIFY(old_data);
    BkClipState old_clip;
    VERIFY(bk_actor_pose_state(old_data->pose, &old_clip));
    uint32_t old_hidden;
    VERIFY(bk_actor_pose_hidden(old_data->pose, old_data->root, &old_hidden));
    BkEndingNormalRender *old_render = s->render;
    BkEndingAudio *audio_owner = s->audio;
    BkEndingUiRender *base_ui = s->ui_render;
    BkEndingUiBatch *batch = s->ui_batch;
    BkEndingNormalControllerRetained normal = *s->normal_controller;
    BkEndingSecondaryControlState control = *s->secondary_controller;
    BkEndingSecondaryPresentationState presentation = *s->secondary_presentation;
    BkEndingPresentationRetained normal_presentation = *s->presentation;
    BkEndingAuxiliaryCycle auxiliary = *s->auxiliary_cycle;
    int32_t duck = *s->duck_transition;
    VERIFY(bk_renderer_readback(renderer, before, sizeof(before), e));
    /*Exercise real speech presence, every effect Stop and independent mixer
     * users. This voice occupancy is an explicit ownership fixture.*/
    for (unsigned i = 0; i < 2; ++i)
      VERIFY(bk_ending_audio_bind(s->audio, i, "bk3_02", "se002.wav", e));
    int present_before[BK_ENDING_SOUND_BUFFERS];
    for (unsigned i = 0; i < BK_ENDING_SOUND_BUFFERS; ++i)
      VERIFY(bk_ending_audio_present(s->audio, i, &present_before[i]));
    for (unsigned i = 0; i < BK_AUDIO_VOICES; ++i)
      if (i != BK_ENDING_SOUND_MUSIC)
        VERIFY(bk_audio_play(audio, i, guard, 1, -2100, 170, e));
    VERIFY(bk_audio_gain(audio, BK_ENDING_SOUND_MUSIC, -1350, -400, e));
    VERIFY(bk_audio_poll(audio, e) && bk_audio_fill(audio, e));
    BkAudioCursor music = {0}, retained_music = {0};
    VERIFY(bk_audio_cursor(audio, BK_ENDING_SOUND_MUSIC, &music));
    VERIFY(music.pcm && music.playing && music.buffered);
    BkAudioStats queued = bk_audio_stats(audio);
    VERIFY(queued.submitted > queued.consumed);
    BkEndingReloadBindings bindings;
    VERIFY(bk_ending_state_reload_bindings(s->state, s->common,
                                           &s->previous_flow, &bindings));
    BkEndingReloadOps ops = {s, ui_reload_load, ui_reload_release,
        ui_reload_final_image, ui_reload_leave, ui_reload_lighting,
        ui_reload_schedule, ui_reload_present, ui_reload_status, ui_reload_pause};
    s->state->next_mode = to_normal;
    s->common->action = 6;
    s->common->blocked = 1;
    int early = -1;
    VERIFY(bk_ending_reload_transition(&bindings, 3, &ops, &early, e));
    VERIFY(!early && !s->common->action && !s->common->blocked);
    VERIFY(s->state->frame.phase == (to_normal ? 1 : 2));
    VERIFY(s->render && s->render != old_render && s->retired.render == old_render &&
           s->snapshot_render == old_render);
    VERIFY(s->audio == audio_owner && s->ui_render == base_ui && s->ui_batch == batch);
    VERIFY(!memcmp(&normal, s->normal_controller, sizeof(normal)) &&
           !memcmp(&control, s->secondary_controller, sizeof(control)) &&
           !memcmp(&presentation, s->secondary_presentation, sizeof(presentation)) &&
           !memcmp(&normal_presentation, s->presentation, sizeof(normal_presentation)) &&
           !memcmp(&auxiliary, s->auxiliary_cycle, sizeof(auxiliary)) &&
           duck == *s->duck_transition);
    for (unsigned i = 0; i < BK_AUDIO_VOICES; ++i) {
      int playing;
      VERIFY(bk_audio_playing(audio, i, &playing));
      int expected = i >= BK_ENDING_SOUND_MUSIC ||
                     (i >= 2 && !present_before[i]);
      if (playing != expected)
        snprintf(e, 256, "voice%u playing%d expected%d", i, playing, expected);
      VERIFY(playing == expected);
    }
    for (unsigned i = 0; i < 2; ++i) {
      int present;
      VERIFY(bk_ending_audio_present(s->audio, i, &present) && !present);
    }
    int32_t volume, pan;
    VERIFY(bk_audio_get_gain(audio, BK_ENDING_SOUND_MUSIC, &volume, &pan) &&
           volume == -1350 && pan == -400);
    VERIFY(bk_audio_cursor(audio, BK_ENDING_SOUND_MUSIC, &retained_music));
    VERIFY(music.pcm == retained_music.pcm &&
           music.source_frame == retained_music.source_frame &&
           music.playing == retained_music.playing &&
           music.buffered == retained_music.buffered &&
           music.pending_change == retained_music.pending_change);
    BkAudioStats after_audio = bk_audio_stats(audio);
    VERIFY(after_audio.submitted == queued.submitted && after_audio.consumed == queued.consumed);
    BkEndingBackgroundAssets *next_background = stage_background(s);
    const BkEndingBackgroundData *next_data = bk_ending_background_assets_data(next_background);
    VERIFY(next_data && ((next_background == old_background) == (group != 1)));
    if (group != 1) {
      BkClipState next_clip;
      uint32_t next_hidden;
      VERIFY(bk_actor_pose_state(next_data->pose, &next_clip));
      VERIFY(!memcmp(&old_clip, &next_clip, sizeof(old_clip)));
      VERIFY(bk_actor_pose_hidden(next_data->pose, next_data->root, &next_hidden) &&
             next_hidden == old_hidden);
      const BkFrameTree *tree = bk_actor_forest_tree(scene_forest(s));
      uint32_t root = bk_actor_forest_node(scene_forest(s), to_normal ? 4 : 3, next_data->root);
      VERIFY(bk_frame_tree_parent(tree, root) == 0 &&
             bk_frame_tree_next(tree, root) != BK_FRAME_NONE);
    } else
      VERIFY(!strcmp(bk_ending_background_assets_name(next_background),
                       to_normal ? "m02_92.xan" : "m02_90.xan"));
    BkEndingState post = (*s->state);
    for (unsigned redraw = 0; redraw < 3; ++redraw) {
      VERIFY(stage_present(scene, renderer, e));
      VERIFY(bk_renderer_readback(renderer, after, sizeof(after), e));
      VERIFY(!memcmp(before, after, sizeof(before)) && !memcmp(&post, s->state, sizeof(post)));
      VERIFY(s->retired.render == old_render && s->snapshot_render == old_render);
      ++out->redraws;
    }
    VERIFY(stage_tick(scene, s, out, e));
    VERIFY(!s->retired.render && s->snapshot_render == s->render);
    for (unsigned i = 0; i < 7; ++i) VERIFY(stage_tick(scene, s, out, e));
    BkRenderStats live = bk_renderer_stats(renderer);
    BkRenderStats *stable = to_normal ? &normal_stable : &secondary_stable;
    if (cycle < 2) *stable = live;
    else VERIFY(live.live_allocations == stable->live_allocations &&
                live.live_bytes == stable->live_bytes);
    ++out->reloads;
  }
  /*Now drive the real UI curtain to its opaque boundary. Request injection
   * remains explicit; the scene chooses when to release/load and captures
   * its original mixed old/new owner UI batch within that same tick.*/
  for (unsigned direction = 0; direction < 2; ++direction) {
    VERIFY(s->state->frame.phase == (direction ? 2 : 1));
    unsigned settle = 0;
    while (s->common->curtain.stage != 0 && settle++ < 240)
      VERIFY(stage_tick(scene, s, out, e));
    if (s->common->curtain.stage || s->common->action || s->common->blocked)
      snprintf(e, 256, "unsettled curtain stage%u alpha%g action%u blocked%u phase%d",
               s->common->curtain.stage, s->common->curtain.alpha,
               s->common->action, s->common->blocked, s->state->frame.phase);
    VERIFY(s->common->curtain.stage == 0);
    VERIFY(!s->common->action && !s->common->blocked);
    BkEndingNormalRender *old_render = s->render;
    s->state->next_mode = direction;
    s->common->action = 6;
    s->common->blocked = 1;
    unsigned wait = 0;
    while (s->render == old_render && wait++ < 240)
      VERIFY(stage_tick(scene, s, out, e));
    VERIFY(s->render != old_render && s->retired.render == old_render &&
           s->snapshot_render == old_render && wait > 1);
    VERIFY(s->state->frame.phase == (direction ? 1 : 2));
    VERIFY(!s->common->action && !s->common->blocked);
    VERIFY(bk_renderer_readback(renderer, before, sizeof(before), e));
    VERIFY(stage_present(scene, renderer, e));
    VERIFY(bk_renderer_readback(renderer, after, sizeof(after), e));
    VERIFY(!memcmp(before, after, sizeof(before)));
    ++out->redraws;
    wait = 0;
    while (s->common->curtain.stage != 0 && wait++ < 240)
      VERIFY(stage_tick(scene, s, out, e));
    VERIFY(s->common->curtain.stage == 0 && s->common->curtain.alpha == 0);
    VERIFY(!s->retired.render && s->snapshot_render == s->render);
    ++out->ui_reloads;
  }
  out->pcm = stage_hash(out->pcm, &sink.hash, sizeof(sink.hash));
  ++out->entries;
  printf("stage reload group%u PASS boundary4 curtain2 redraws14\n", group);
  fflush(stdout);
  ok = 1;
done:
  bk_scene_destroy(scene);
  bk_audio_clip_release(guard);
  bk_audio_destroy(audio);
  free(records);
  return ok;
}
int main(int argc, char **argv) {
  if (argc != 2) return 2;
  char e[256] = {0}, path[1024];
  int rc = 1;
  BkRenderer *renderer = bk_renderer_create(85, 64, stdout, e);
  BkResourceStore *store = bk_resources_create(e);
  if (!renderer || !store) goto done;
  const char *packs[] = {"bk3_00", "bk3_02", "bk3_03", "bk3_04", "bk3_06",
                         "bk3_08", "bk3_09", "bk3_18", "fambom"};
  for (unsigned i = 0; i < sizeof(packs) / sizeof(*packs); ++i)
    if (snprintf(path, sizeof(path), "%s/%s.pp", argv[1], packs[i]) >= (int)sizeof(path) ||
        !bk_resources_mount(store, packs[i], path, e)) goto done;
  BkRenderStats baseline = bk_renderer_stats(renderer);
  StageResult result = {.pcm = UINT64_C(14695981039346656037)};
  for (unsigned group = 0; group < 5; ++group) {
    if (!stage_profile(renderer, store, group, &result, e)) goto done;
    BkRenderStats live = bk_renderer_stats(renderer);
    if (live.live_allocations != baseline.live_allocations ||
        live.live_bytes != baseline.live_bytes) {
      snprintf(e, sizeof(e), "stage reload retained GPU resources after destruction");
      goto done;
    }
  }
  printf("PASS stage reload entries%u boundary%u curtain%u frames%u redraws%u pcm=%016llx\n",
         result.entries, result.reloads, result.ui_reloads, result.frames, result.redraws,
         (unsigned long long)result.pcm);
  rc = 0;
done:
  if (rc) fprintf(stderr, "%s\n", e);
  bk_renderer_destroy(renderer);
  bk_resources_destroy(store);
  return rc;
}
