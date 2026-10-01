/*Actual Japanese selected/auxiliary node owners, projection and visibility.
 *Action8 and auxiliary idle are explicit boundaries, not a story completion.*/
#define main selected_session_regression_main
#include "ending_selected_session_probe.c"
#undef main

#define NODE_CHECK(x) do { if (!(x)) { \
  fprintf(stderr, "ending nodes group%u variant%u line%d (%s): %s\n", \
          group, variant, __LINE__, #x, e); goto done; } } while (0)

typedef struct {
  unsigned entries, anchors, missing, toggles, absent_special, picks, inputs, gestures, retirements;
  SelectedResult timeline;
} NodeResult;

static int node_profile(BkRenderer *renderer, BkResourceStore *store,
    unsigned group, unsigned variant, NodeResult *out, char e[256]) {
  int ok = 0;
  BkScene *scene = NULL;
  BkAudio *audio = NULL;
  SelectedSink sink = {.hash = UINT64_C(14695981039346656037)};
  BkAudioSink output = {&sink, 48000, 480, 1920, probe_submit, probe_poll};
  NODE_CHECK(audio = bk_audio_create(&output, e));
  BkSceneServices services = {store, renderer, stdout, audio, NULL, NULL};
  uint8_t unlocked[5][8] = {{0}};
  NODE_CHECK(scene = bk_ending_selected_scene_create_gallery(&services, group,
      variant, variant ? 5 : 2, unlocked, NULL, e));
  EndingNormalScene *s = bk_scene_custom_context(scene);
  NODE_CHECK(s && bk_audio_fill(audio, e) && probe_present(scene, renderer, e));
  unsigned opening = 0;
  while (s->state->auxiliary.gate == 4 && opening++ < 1800)
    NODE_CHECK(probe_tick(scene, s, &out->timeline, e));
  NODE_CHECK(s->state->auxiliary.gate == 1 && opening < 1800);
  for (unsigned stage = 0; stage < 2; ++stage) {
    BkActorForest *forest = scene_forest(s);
    BkActorPose *pose = scene_primary(s);
    uint32_t anchor;
    NODE_CHECK(bk_actor_forest_find(forest, scene_root(s, 0), "A_okosi", &anchor, e));
    uint32_t expected = anchor == BK_FRAME_NONE ? 0 : anchor;
    NODE_CHECK((uint32_t)s->state->retained.normal.word_719b40 == expected);
    if (expected) ++out->anchors; else ++out->missing;

    /*Toggle the real optional node through the production presentation,
     *checking that the independent interaction anchor remains visible.*/
    uint32_t special_frame = stage ? bk_ending_auxiliary_assets_oyu(s->auxiliary_assets)
        : bk_ending_selected_assets_special(s->selected_assets);
    uint32_t special = 0;
    NODE_CHECK(optional_primary_node(s, special_frame, &special, e));
    printf("NODE_BIND %u %u %u %u %u\n", group, variant, stage, expected, special);
    if (group == 2) {
      uint32_t original_hidden = 0;
      if (special) NODE_CHECK(bk_actor_pose_hidden(pose, special_frame, &original_hidden));
      for (unsigned show = 0; show < 2; ++show) {
        s->state->control.toggles[5] = (uint8_t)show;
        s->active_seconds = 1.f / 60;
        if (stage) {
          s->state->control.state_721eec = 1;
          NODE_CHECK(auxiliary_presentation_step(s, e));
        } else {
          s->frame_active = 1;
          uint32_t result;
          BkEndingCall call = {.operation = BK_ENDING_STAGE_494015};
          NODE_CHECK(frame_invoke(s, &call, &result, e));
        }
        uint32_t hidden;
        if (special) {
          /*Variant0 OYU is719B44 and must remain unchanged by719B48's
           *toggle; variant1 cris_baiza is the actual controlled node.*/
          uint32_t wanted = !stage && !variant ? original_hidden : !show;
          NODE_CHECK(bk_actor_pose_hidden(pose, special_frame, &hidden) && hidden == wanted);
          ++out->toggles;
        } else {
          NODE_CHECK(stage && special_frame == BK_MODEL_NONE);
          ++out->absent_special;
        }
        if (expected) {
          uint32_t actor, frame;
          NODE_CHECK(bk_actor_forest_binding(forest, anchor, &actor, &frame));
          NODE_CHECK(actor == scene_registry(s, 0) &&
              bk_actor_pose_hidden(pose, frame, &hidden) && !hidden);
        }
        NODE_CHECK(prepare_scene_draw(s, e) && probe_present(scene, renderer, e));
      }
    }
    NODE_CHECK(prepare_ui_geometry(s, s->viewport.width, s->viewport.height, e));
    if (expected) {
      /*Search actual viewport pixels. The picker uses the real cached
       *worlds/projection/radius, with no injected target coordinates.*/
      int32_t picked = 0, point[2] = {-1, -1};
      for (unsigned y = 0; point[0] < 0 && y < s->viewport.height; ++y)
        for (unsigned x = 0; x < s->viewport.width; ++x) {
          float pointer[] = {(float)x, (float)y};
          NODE_CHECK(stage ? auxiliary_state1_pick(s, pointer, 4, &picked, e)
                           : selected_pick(s, pointer, &picked, e));
          if (picked == 2) { point[0] = (int32_t)x; point[1] = (int32_t)y; break; }
        }
      if (point[0] >= 0) {
        ++out->picks;
        s->state->auxiliary.pending = 0;
        s->state->frame.camera_mode = 0;
        s->state->frame.auxiliary_mode = 0;
        int playing;
        BkEndingAudioCall pause = {.operation = BK_ENDING_AUDIO_PAUSE, .slot = 1};
        NODE_CHECK(auxiliary_state1_audio(s, &pause, &playing, e));
        if (stage) s->state->control.state_721eec = 1;
        else s->state->auxiliary.gate = 1;
        BkInput input = {.pointer_active = 1, .pointer_x = s->viewport.x + point[0],
            .pointer_y = s->viewport.y + point[1],
            .pressed = BK_BUTTON_CONFIRM, .held = BK_BUTTON_CONFIRM};
        NODE_CHECK(probe_input_tick(scene, s, &out->timeline, &input, e));
        NODE_CHECK(s->state->frame.camera_event == 2);
        ++out->inputs;
        if (!stage) {
          NODE_CHECK(s->state->auxiliary.gate == 3);
          ++out->gestures;
        }
      }
    }
    out->timeline.state = selected_hash(out->timeline.state,
        &s->state->retained.normal.word_719b40, sizeof(int32_t));
    if (stage == 0) {
      /*Retire the old forest only after its captured final frame; a redraw
       *must not clear the newly installed auxiliary anchor.*/
      BkEndingReloadBindings bindings;
      s->previous_flow = 8;
      s->common->action = 8; s->common->blocked = 1;
      NODE_CHECK(bk_ending_state_reload_bindings(s->state, s->common, &s->previous_flow, &bindings));
      BkEndingReloadOps ops = {s, ui_reload_load, ui_reload_release,
          ui_reload_final_image, ui_reload_leave, ui_reload_lighting,
          ui_reload_schedule, ui_reload_present, ui_reload_status, ui_reload_pause};
      int early = -1;
      NODE_CHECK(bk_ending_reload_transition(&bindings, 3, &ops, &early, e));
      NODE_CHECK(!early && s->auxiliary_assets && s->retired.selected);
      int32_t fresh = s->state->retained.normal.word_719b40;
      NODE_CHECK(probe_present(scene, renderer, e));
      NODE_CHECK(s->state->retained.normal.word_719b40 == fresh);
      /*Explicit idle fixture isolates optional-node interaction.*/
      s->state->control.state_721eec = 1;
      NODE_CHECK(probe_tick(scene, s, &out->timeline, e));
      NODE_CHECK(!s->retired.selected && s->state->retained.normal.word_719b40 == fresh);
      ++out->retirements;
    }
    ++out->entries;
  }
  out->timeline.pcm = selected_hash(out->timeline.pcm, &sink.hash, sizeof(sink.hash));
  ok = 1;
done:
  bk_scene_destroy(scene);
  bk_audio_destroy(audio);
  return ok;
}

int main(int argc, char **argv) {
  if (argc < 2 || argc > 3) return 2;
  int rc = 1;
  char e[256] = {0}, path[1024];
  BkRenderer *renderer = bk_renderer_create(85, 64, stdout, e);
  BkResourceStore *store = bk_resources_create(e);
  if (!renderer || !store) goto done;
  const char *packs[] = {"bk3_00", "bk3_02", "bk3_03", "bk3_04", "bk3_06",
      "bk3_08", "bk3_09", "bk3_10", "bk3_11", "bk3_12", "bk3_13", "bk3_18", "fambom"};
  for (unsigned i = 0; i < sizeof(packs) / sizeof(*packs); ++i)
    if (snprintf(path, sizeof(path), "%s/%s.pp", argv[1], packs[i]) >= (int)sizeof(path) ||
        !bk_resources_mount(store, packs[i], path, e)) goto done;
  BkRenderStats baseline = bk_renderer_stats(renderer);
  NodeResult out = {.timeline = {.state = UINT64_C(14695981039346656037),
                                 .pcm = UINT64_C(14695981039346656037)}};
  for (unsigned group = 0; group < (argc == 3 ? 1u : 5u); ++group)
    for (unsigned variant = 0; variant < 2; ++variant) {
      if (!node_profile(renderer, store, group, variant, &out, e)) goto done;
      BkRenderStats live = bk_renderer_stats(renderer);
      if (live.live_allocations != baseline.live_allocations || live.live_bytes != baseline.live_bytes)
        { snprintf(e, sizeof(e), "node bindings leaked GPU resources"); goto done; }
    }
  if (!out.picks || !out.gestures || out.picks != out.inputs || (argc == 2 &&
      (out.entries != 20 || out.missing != 6 || out.toggles != 4 || out.absent_special != 4))) {
    snprintf(e, sizeof(e), "coverage entries%u anchors%u missing%u toggles%u absent%u picks%u inputs%u gestures%u",
        out.entries, out.anchors, out.missing, out.toggles, out.absent_special, out.picks, out.inputs, out.gestures);
    goto done;
  }
  printf("PASS ending nodes entries%u anchors%u missing%u toggles%u absent%u picks%u inputs%u gestures%u retirements%u frames%u state=%016llx pcm=%016llx\n",
      out.entries, out.anchors, out.missing, out.toggles, out.absent_special, out.picks, out.inputs,
      out.gestures, out.retirements, out.timeline.frames, (unsigned long long)out.timeline.state,
      (unsigned long long)out.timeline.pcm);
  rc = 0;
done:
  if (rc) fprintf(stderr, "%s\n", e);
  bk_renderer_destroy(renderer);
  bk_resources_destroy(store);
  return rc;
}
