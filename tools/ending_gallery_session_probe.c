/*Explicit recorded-lane fixtures through the actual Japanese scene. These
 *exercise production48302B/48BCBB, PCM, drawing and loader transitions;
 *they do not claim a naturally recorded story or front-end menu entry.*/
#define main selected_session_regression_main
#include "ending_selected_session_probe.c"
#undef main

typedef struct {
  unsigned entries, transitions, children, completed, redraws;
  SelectedResult timeline;
} GalleryResult;
typedef struct {
  SelectedSink sink;
  uint64_t available;
} GallerySink;
static int gallery_poll(void *p, uint64_t *consumed, char e[256]) {
  (void)e;
  GallerySink *sink = p;
  sink->sink.consumed = sink->available < sink->sink.submitted
      ? sink->available : sink->sink.submitted;
  *consumed = sink->sink.consumed;
  return 1;
}
#define GALLERY_CHECK(x) do { if (!(x)) { \
  fprintf(stderr, "gallery session group%u variant%u line%d (%s): %s\n", \
          group, variant, __LINE__, #x, e); goto done; } } while (0)
static int gallery_profile(BkRenderer *renderer, BkResourceStore *store,
    unsigned group, unsigned variant, int profile, GalleryResult *out, char e[256]) {
  int mixed = profile & 1, selected_only = profile >= 2;
  int ok = 0;
  BkScene *scene = NULL;
  BkAudio *audio = NULL;
  BkEndingRecords *records = calloc(1, sizeof(*records));
  GallerySink sink = {.sink.hash = UINT64_C(14695981039346656037)};
  BkAudioSink output = {&sink, 48000, 480, 1920, probe_submit, gallery_poll};
  GALLERY_CHECK(records && (audio = bk_audio_create(&output, e)));
  /*Native recorded routes return from second stage to normal before
   *loading selected assets; a direct9->12 fixture asks the wrong destructor.*/
  const uint32_t normal[] = {0, 9, 0, 12, 21};
  const uint32_t third[] = {15, 16, 17, 19, 12, 21};
  for (unsigned lane = 0; lane < 2; ++lane) {
    for (unsigned i = 0; i < BK_ENDING_RECORD_CAPACITY; ++i)
      records->groups[group].retained[lane][i] = 21;
    memcpy(records->groups[group].retained[lane], lane ? third : normal,
             lane ? sizeof(third) : sizeof(normal));
    if (selected_only) {
      records->groups[group].retained[lane][0] = 12;
      records->groups[group].retained[lane][1] = 21;
    }
  }
  uint64_t record_hash = selected_hash(0, records, sizeof(*records));
  BkSceneServices services = {store, renderer, stdout, audio, NULL};
  uint8_t unlocked[5][8] = {{0}};
  GALLERY_CHECK(scene = bk_ending_replay_scene_create(&services, group, variant,
                                                      records, unlocked, NULL, e));
  EndingNormalScene *s = bk_scene_custom_context(scene);
  GALLERY_CHECK(s && s->state->frame.phase == 8 && s->frame_active &&
      s->process == &s->diagnostic_process &&
      s->selected_action == &s->process->selected_action);
  GALLERY_CHECK(bk_audio_fill(audio, e) && probe_present(scene, renderer, e));
  ++out->entries;
  unsigned children = 0, transitions = 0;
  int32_t cursor = -1, main_state = -1;
  int32_t selected_state = -1;
  int32_t secondary_state = -1, secondary_opening = -1;
  BkEndingNormalRender *old_render = s->render;
  for (unsigned tick = 0; tick < 100000; ++tick) {
    if (s->common->action == 0x31) { ++out->completed; break; }
    /*Test both fixed60 and explicit mixed millisecond intervals. The model
     *retains the original scheduler; selected gallery control additionally
     *consumes completed chains to avoid the proven endpoint stall.*/
    const unsigned frame_ms[] = {20, 33, 14, 38};
    unsigned ms = frame_ms[tick % 4];
    sink.available += mixed ? ms * 48 : 800;
    GALLERY_CHECK(bk_audio_poll(audio, e) &&
        bk_scene_step(scene, mixed ? ms / 1000.0 : 1.0 / 60.0, &(BkInput){0}, e) &&
        bk_audio_fill(audio, e) && probe_present(scene, renderer, e));
    ++out->timeline.frames;
    int32_t values[] = {s->state->frame.phase, s->state->frame.state_721ee0,
      s->state->final_state, s->state->retained.final.word_6c7f74,
      s->state->retained.final.byte_6ddce0,
      s->state->retained.final.byte_6c7f70, s->state->retained.final.byte_6d1be0,
      s->state->retained.final.byte_6d1bd4, s->state->retained.final.word_6dde54,
      s->selected_action->diagnostic_crossings, s->common->action};
    out->timeline.state = selected_hash(out->timeline.state, values, sizeof(values));
    if (main_state != s->state->frame.state_721ee0 ||
        cursor != s->state->retained.final.word_6c7f74) {
      main_state = s->state->frame.state_721ee0;
      cursor = s->state->retained.final.word_6c7f74;
      printf("GALLERY_STEP %u %u %u phase%d main%d cursor%d action%u sub%d/%u/%u/%u\n",
          group, variant, tick, s->state->frame.phase, main_state, cursor,
          s->state->retained.final.byte_6ddce0, s->state->final_state,
          s->state->retained.final.byte_6c7f70, s->state->retained.final.byte_6d1be0,
          s->state->retained.final.byte_6d1bd4);
      fflush(stdout);
    }
    if (main_state >= 4 && main_state <= 8) children |= 1u << (main_state - 4);
    if (main_state == 5 && (secondary_state != s->state->retained.final.byte_6d1be0 ||
        secondary_opening != s->state->retained.final.byte_6dde58 || !(tick % 10000))) {
      BkClipState clip;
      BkEndingGallerySecondaryClip timing;
      GALLERY_CHECK(bk_actor_pose_state(scene_primary(s), &clip) &&
          gallery_secondary_clip(s, clip.slot, &timing, e));
      secondary_state = s->state->retained.final.byte_6d1be0;
      secondary_opening = s->state->retained.final.byte_6dde58;
      printf("GALLERY_SECONDARY %u %u tick%u state%d opening%d clip%d source%g end%g chain%d loop%d remaining%d\n",
          group, variant, tick, secondary_state, secondary_opening, clip.slot,
          timing.source, timing.end, timing.chain, timing.loop,
          s->process->gallery_secondary.remaining);
      fflush(stdout);
    }
    if (main_state == 6 && (selected_state != s->state->final_state || !(tick % 10000))) {
      BkClipState clip;
      GALLERY_CHECK(bk_actor_pose_state(scene_primary(s), &clip));
      selected_state = s->state->final_state;
      printf("GALLERY_SELECTED %u %u tick%u state%d clip%d source%g elapsed%d cycle%d/%d\n",
          group, variant, tick, selected_state, clip.slot, clip.source,
          s->state->retained.final.word_6dde54, s->process->gallery_selected.counter,
          s->process->gallery_selected.alternate);
      fflush(stdout);
    }
    if (s->render != old_render) { ++transitions; old_render = s->render; }
    if (!(tick % 503)) {
      uint8_t before[85 * 64 * 4], after[sizeof(before)];
      BkEndingState held = *s->state;
      BkEndingProcess process = *s->process;
      GALLERY_CHECK(bk_renderer_readback(renderer, before, sizeof(before), e));
      GALLERY_CHECK(bk_renderer_begin(renderer, e) &&
          bk_scene_draw(scene, &(BkSceneFrame){0}, e) && bk_renderer_end(renderer, e));
      GALLERY_CHECK(bk_renderer_readback(renderer, after, sizeof(after), e));
      GALLERY_CHECK(!memcmp(before, after, sizeof(before)) &&
          !memcmp(&held, s->state, sizeof(held)) &&
          !memcmp(&process, s->process, sizeof(process)));
      ++out->redraws;
    }
  }
  GALLERY_CHECK(s->common->action == 0x31);
  GALLERY_CHECK(children == (selected_only ? 4u : variant ? 28u : 7u) &&
      transitions >= (selected_only ? 1u : 2u));
  GALLERY_CHECK(record_hash == selected_hash(0, records, sizeof(*records)));
  out->children |= children;
  out->transitions += transitions;
  out->timeline.pcm = selected_hash(out->timeline.pcm, &sink.sink.hash, sizeof(sink.sink.hash));
  GALLERY_CHECK(bk_ending_normal_scene_stop(scene, e));
  ok = 1;
done:
  if (!ok && scene) {
    EndingNormalScene *s = bk_scene_custom_context(scene);
    fprintf(stderr, "gallery failed phase%d main%d cursor%d sub%d frames%u\n",
        s->state->frame.phase, s->state->frame.state_721ee0,
        s->state->retained.final.word_6c7f74, s->state->final_state, out->timeline.frames);
  }
  bk_scene_destroy(scene);
  bk_audio_destroy(audio);
  free(records);
  return ok;
}
int main(int argc, char **argv) {
  if (argc < 2 || argc > 5) return 2;
  int group_filter = argc > 2 ? atoi(argv[2]) : -1;
  int variant_filter = argc > 3 ? atoi(argv[3]) : -1;
  int mixed = argc > 4 ? atoi(argv[4]) : 0;
  char e[256] = {0}, path[4096];
  int ok = 0;
  BkRenderer *renderer = bk_renderer_create(85, 64, stdout, e);
  BkResourceStore *store = bk_resources_create(e);
  if (!renderer || !store) goto done;
  const char *packs[] = {"bk3_00", "bk3_02", "bk3_03", "bk3_04", "bk3_06",
      "bk3_08", "bk3_09", "bk3_10", "bk3_11", "bk3_12", "bk3_13", "bk3_18", "fambom"};
  for (unsigned i = 0; i < sizeof(packs) / sizeof(*packs); ++i)
    if (snprintf(path, sizeof(path), "%s/%s.pp", argv[1], packs[i]) >= (int)sizeof(path) ||
        !bk_resources_mount(store, packs[i], path, e)) goto done;
  BkRenderStats baseline = bk_renderer_stats(renderer);
  GalleryResult out = {.timeline = {.state = UINT64_C(14695981039346656037),
                                     .pcm = UINT64_C(14695981039346656037)}};
  for (unsigned group = 0; group < 5; ++group) {
    if (group_filter >= 0 && group != (unsigned)group_filter) continue;
    for (unsigned variant = 0; variant < 2; ++variant) {
      if (variant_filter >= 0 && variant != (unsigned)variant_filter) continue;
      if (!gallery_profile(renderer, store, group, variant, mixed, &out, e)) goto done;
      BkRenderStats live = bk_renderer_stats(renderer);
      if (live.live_allocations != baseline.live_allocations || live.live_bytes != baseline.live_bytes) {
        snprintf(e, sizeof(e), "gallery leaked GPU resources"); goto done;
      }
    }
  }
  printf("PASS gallery session entries%u transitions%u children%u completed%u redraws%u frames%u state=%016llx pcm=%016llx\n",
      out.entries, out.transitions, out.children, out.completed, out.redraws,
      out.timeline.frames, (unsigned long long)out.timeline.state,
      (unsigned long long)out.timeline.pcm);
  ok = 1;
done:
  if (!ok) fprintf(stderr, "%s\n", e);
  bk_resources_destroy(store);
  bk_renderer_destroy(renderer);
  return ok ? 0 : 1;
}
