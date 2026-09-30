/*Production service boundaries with actual Japanese actor/UI/audio owners.
 *Explicit completion/replay states are fixtures, not natural game completion.*/
#define main selected_session_regression_main
#include "ending_selected_session_probe.c"
#undef main

#define CHECK(x) do { if (!(x)) { \
  fprintf(stderr, "selected adapters group%u variant%u line%d (%s): %s\n", \
          group, variant, __LINE__, #x, e); goto done; } } while (0)

typedef struct {
  unsigned entries, manual, restart, flash, uv, cancel, replay, completion, auxiliary, rejects;
  SelectedResult timeline;
} AdapterResult;

static int adapter_profile(BkRenderer *renderer, BkResourceStore *store,
    unsigned group, unsigned variant, AdapterResult *out, char e[256]) {
  int ok = 0;
  BkScene *scene = NULL;
  BkAudio *audio = NULL;
  SelectedSink sink = {.hash = UINT64_C(14695981039346656037)};
  BkAudioSink output = {&sink, 48000, 480, 1920, probe_submit, probe_poll};
  CHECK(audio = bk_audio_create(&output, e));
  BkSceneServices services = {store, renderer, stdout, audio, NULL};
  uint8_t unlocked[5][8] = {{0}};
  CHECK(scene = bk_ending_selected_scene_create_gallery(&services, group,
      variant, variant ? 5 : 2, unlocked, NULL, e));
  EndingNormalScene *s = bk_scene_custom_context(scene);
  CHECK(s && bk_audio_fill(audio, e) && probe_present(scene, renderer, e));
  unsigned opening = 0;
  while (s->state->auxiliary.gate == 4 && opening++ < 1800)
    CHECK(probe_tick(scene, s, &out->timeline, e));
  CHECK(s->state->auxiliary.gate == 1 && opening < 1800);
  BkActorPose *pose = scene_primary(s);
  BkClipState before, after;
  /*4974D1 uses401B0A's ten-tick request, even after a tiny401D24
   *selection has changed this descriptor's configured transition.*/
  const unsigned manual_slots[] = {4, 6, 10, 17};
  for (unsigned proposed = 0; proposed < 4; ++proposed) {
    unsigned slot = manual_slots[proposed];
    CHECK(bk_actor_pose_select(pose, slot, 1, e));
    CHECK(bk_actor_pose_select(pose, 20, 1, e));
    int accepted = -1;
    CHECK(selected_manual(s, (int32_t)proposed, &accepted, e) && accepted == 1);
    BkPlaybackEffects effects;
    int submitted;
    CHECK(bk_actor_pose_advance_effects(pose, .05f, &effects, &submitted, e) && submitted);
    CHECK(bk_actor_pose_advance_effects(pose, .05f, &effects, &submitted, e) && submitted);
    CHECK(effects.pose.blend && fabsf(effects.pose.weight - .3f) < 1e-6f);
    CHECK(bk_actor_pose_state(pose, &before) && before.slot == (int32_t)slot && !before.blend_done);
    CHECK(selected_manual(s, (int32_t)proposed, &accepted, e) && accepted == 1);
    CHECK(bk_actor_pose_state(pose, &after) && !memcmp(&before, &after, sizeof(before)));
    ++out->manual;
  }
  int absent, stop_playing;
  CHECK(bk_ending_audio_present(s->audio, 46, &absent) && !absent);
  CHECK(!selected_stop(s, 46, e)); e[0] = 0; ++out->rejects;
  BkEndingAudioCall nullable_pause = {.operation = BK_ENDING_AUDIO_PAUSE, .slot = 46};
  CHECK(selected_audio(s, &nullable_pause, &stop_playing, e) && !stop_playing);
  /*Restart a real active terminal clip. It must reset its clock without
   *advancing/publishing any pose, or trying to access audio slot20.*/
  CHECK(bk_actor_pose_select(pose, 20, 1, e));
  CHECK(bk_actor_pose_advance(pose, -1, .1f, e));
  CHECK(bk_actor_pose_advance(pose, -1, .1f, e));
  CHECK(bk_actor_pose_state(pose, &before) && before.slot == 20 && before.elapsed > 0);
  size_t floats = 0;
  const float *world = bk_actor_pose_world(pose, &floats);
  CHECK(world && floats);
  uint64_t world_before = selected_hash(UINT64_C(14695981039346656037),
                                        world, floats * sizeof(float));
  CHECK(selected_repeat(s, 20, e));
  CHECK(bk_actor_pose_state(pose, &after) && after.slot == 20 &&
      after.requested == 20 && after.elapsed == 0 && after.blend_elapsed == 0 &&
      after.blend_from == before.source);
  uint64_t world_after = selected_hash(UINT64_C(14695981039346656037),
                                       world, floats * sizeof(float));
  CHECK(world_before == world_after);
  ++out->restart;
  CHECK(!selected_repeat(s, BK_CLIP_SLOTS, e)); e[0] = 0; ++out->rejects;
  CHECK(bk_actor_pose_state(pose, &before) && !memcmp(&before, &after, sizeof(before)));

  /*These updates must reach the actual draw owner, not a shadow array.*/
  s->ui_notices.notices[0] = 1;
  CHECK(selected_ui_byte(s, 53, BK_ENDING_SELECTED_UI_BYTE_166, 0, e));
  CHECK(s->ui_notices.notices[0] == 1);
  CHECK(selected_ui_byte(s, 52, BK_ENDING_SELECTED_UI_BYTE_167, 1, e));
  CHECK(selected_ui_byte(s, 52, BK_ENDING_SELECTED_UI_BYTE_134, 3, e));
  CHECK(selected_ui_fade(s, 52, 1, e));
  CHECK(s->ui_flash_wanted == 1 && s->ui.sprites[52].transform.fade.stage == 3 &&
      s->ui.sprites[52].transform.fade.alpha == 1);
  BkEndingUiFrame draw = {0};
  CHECK(bk_ending_ui_dispatch_sprite(&s->ui, &s->stage_ui, 52, 1.f / 60, &draw, e));
  CHECK(draw.count == 1 && draw.draws[0].slot == 52 && draw.draws[0].alpha == 1);
  CHECK(bk_ending_ui_render_prepare(s->ui_render, &draw, s->viewport.width, s->viewport.height, e));
  CHECK(bk_renderer_begin(renderer, e) && bk_ending_ui_render_draw(s->ui_render, e) &&
      bk_renderer_end(renderer, e));
  uint8_t flash[85 * 64 * 4];
  CHECK(bk_renderer_readback(renderer, flash, sizeof(flash), e));
  out->timeline.state = selected_hash(out->timeline.state, flash, sizeof(flash));
  uint8_t hidden[sizeof(flash)];
  draw.draws[0].alpha = 0;
  CHECK(bk_ending_ui_render_prepare(s->ui_render, &draw, s->viewport.width, s->viewport.height, e));
  CHECK(bk_renderer_begin(renderer, e) && bk_ending_ui_render_draw(s->ui_render, e) &&
      bk_renderer_end(renderer, e));
  CHECK(bk_renderer_readback(renderer, hidden, sizeof(hidden), e));
  CHECK(memcmp(flash, hidden, sizeof(flash)) != 0);
  ++out->flash;
  for (unsigned slot = 53; slot <= 54; ++slot) {
    BkEndingUiSprite old = s->ui.sprites[slot];
    memcpy(s->ui.sprites[slot].uv, (float[4]){.2f, .3f, .4f, .5f}, 16);
    CHECK(selected_ui_uv_reset(s, slot, e));
    CHECK(!memcmp(s->ui.sprites[slot].uv, (float[4]){0, 0, 1, 1}, 16));
    CHECK(!memcmp(&s->ui.sprites[slot].transform, &old.transform, sizeof(old.transform)));
    ++out->uv;
  }
  CHECK(!selected_ui_byte(s, 53, BK_ENDING_SELECTED_UI_BYTE_167, 1, e)); e[0] = 0; ++out->rejects;
  CHECK(!selected_ui_uv_reset(s, 75, e)); e[0] = 0; ++out->rejects;
  CHECK(!selected_ui_fade(s, 52, NAN, e)); e[0] = 0; ++out->rejects;

  /*Mode4 no-input cancellation executes the real Stop and both UV resets.*/
  s->state->auxiliary.gate = 3;
  s->state->ui_controller.auxiliary.mode = 4;
  s->ui_flash_wanted = 0;
  s->state->auxiliary.progress = .8f;
  for (unsigned slot = 53; slot <= 54; ++slot)
    memcpy(s->ui.sprites[slot].uv, (float[4]){.1f, .2f, .6f, .7f}, 16);
  CHECK(probe_tick(scene, s, &out->timeline, e));
  CHECK(s->state->auxiliary.gate == 1 && s->state->auxiliary.progress < .6f);
  for (unsigned slot = 53; slot <= 54; ++slot)
    CHECK(!memcmp(s->ui.sprites[slot].uv, (float[4]){0, 0, 1, 1}, 16));
  ++out->cancel;

  /*Mode7 is a boundary fixture, then production controller/presentation/UI
   *and real PCM proceed together. This used to treat clip20 as audio20.*/
  CHECK(bk_actor_pose_select(pose, 20, 1, e));
  BkClipTiming timing;
  CHECK(bk_actor_pose_timing(pose, 20, &timing));
  CHECK(selected_source(s, 20, timing.end, e));
  CHECK(selected_stop(s, 0, e));
  s->state->auxiliary.gate = 3;
  s->state->ui_controller.auxiliary.mode = 7;
  s->state->frame.camera_mode = 4;
  s->selected_action.counter = -1;
  s->selected_action.replay_variant = 0;
  s->state->retained.auxiliary.word_6dde90 = 1;
  CHECK(probe_tick(scene, s, &out->timeline, e));
  CHECK(s->selected_action.counter == 0 && s->state->ui_controller.auxiliary.mode == 7);
  CHECK(bk_actor_pose_state(pose, &after) && after.slot == 20 && after.source < timing.end);
  for (unsigned i = 0; i < 30; ++i) CHECK(probe_tick(scene, s, &out->timeline, e));
  ++out->replay;

  /*Supply only the last interaction threshold, then let the real finish
   *animation, replay, voice waits and20-second hold produce action49.*/
  CHECK(bk_actor_pose_select(pose, 17, 1, e));
  s->state->auxiliary.gate = 3;
  s->state->ui_controller.auxiliary.mode = 4;
  s->state->ui_controller.auxiliary.reset_c = 11;
  s->selected_plain_scheduled = 0;
  s->selected_action.counter = 0;
  s->selected_action.previous_clock = 0;
  s->state->retained.auxiliary.word_6ea348 = 0;
  BkInput finish_input = {.held = BK_BUTTON_CONFIRM, .pointer_motion_x = 1};
  CHECK(probe_input_tick(scene, s, &out->timeline, &finish_input, e));
  CHECK(s->state->ui_controller.auxiliary.mode == 5 ||
      s->state->ui_controller.auxiliary.mode == 7);
  int found_flash = 0;
  for (unsigned i = 0; i < s->ui_frame.sprites.count; ++i)
    found_flash |= s->ui_frame.sprites.draws[i].slot == 52 &&
                   s->ui_frame.sprites.draws[i].alpha == 1;
  CHECK(found_flash);
  unsigned finish_frames = 0;
  while (s->common->action != 49 && finish_frames++ < 6000)
    CHECK(probe_tick(scene, s, &out->timeline, e));
  if (finish_frames >= 6000) snprintf(e, 256, "finish stalled mode%d stage%d gate%d camera%d",
      s->state->ui_controller.auxiliary.mode, s->state->retained.auxiliary.word_6ea348,
      s->state->auxiliary.gate, s->state->frame.camera_mode);
  CHECK(finish_frames < 6000 && s->state->auxiliary.gate == -1 &&
      s->common->blocked == 1);
  ++out->completion;

  /*Cross-loader action8 is explicitly supplied; then actual phase4 input
   *picks the published actor and opens its real menu.*/
  s->previous_flow = 8;
  s->common->action = 8; s->common->blocked = 1;
  BkEndingReloadBindings bindings;
  CHECK(bk_ending_state_reload_bindings(s->state, s->common, &s->previous_flow, &bindings));
  BkEndingReloadOps ops = {s, ui_reload_load, ui_reload_release,
      ui_reload_final_image, ui_reload_leave, ui_reload_lighting,
      ui_reload_schedule, ui_reload_present, ui_reload_status, ui_reload_pause};
  int early = -1;
  CHECK(bk_ending_reload_transition(&bindings, 3, &ops, &early, e));
  CHECK(!early && s->auxiliary_assets && s->state->frame.phase == 4);
  for (unsigned i = 0; i < 180; ++i) CHECK(probe_tick(scene, s, &out->timeline, e));
  int32_t point[2];
  CHECK(prepare_ui_geometry(s, s->viewport.width, s->viewport.height, e));
  CHECK(bk_ending_ui_project_target(&s->ui_pick, 4, point, e));
  s->state->control.state_721eec = 1;
  s->state->auxiliary.pending = 0;
  s->state->frame.camera_mode = 0;
  int playing;
  BkEndingAudioCall pause = {.operation = BK_ENDING_AUDIO_PAUSE, .slot = 1};
  CHECK(auxiliary_state1_audio(s, &pause, &playing, e));
  BkFaceState *face = bk_ending_auxiliary_assets_face_state(s->auxiliary_assets);
  CHECK(face);
  BkFaceState old_face = *face;
  CHECK(auxiliary_state4_expression(s, 1, 2, 1, e));
  CHECK(!memcmp(face, &old_face, sizeof(old_face)) &&
      s->state->auxiliary.expression_a == 1 && s->state->auxiliary.expression_b == 2);
  BkInput pointer = {.pointer_active = 1, .pointer_x = s->viewport.x + point[0],
      .pointer_y = s->viewport.y + point[1], .pressed = BK_BUTTON_CONFIRM,
      .held = BK_BUTTON_CONFIRM};
  CHECK(probe_input_tick(scene, s, &out->timeline, &pointer, e));
  CHECK(s->state->control.state_721eec == 3 && s->state->frame.camera_cached == 4);
  CHECK(s->state->auxiliary.expression_a == 5 && s->state->auxiliary.expression_b == 4);
  ++out->auxiliary;
  ++out->entries;
  out->timeline.pcm = selected_hash(out->timeline.pcm, &sink.hash, sizeof(sink.hash));
  ok = 1;
done:
  bk_scene_destroy(scene);
  bk_audio_destroy(audio);
  return ok;
}

int main(int argc, char **argv) {
  if (argc < 2 || argc > 3) return 2;
  char e[256] = {0}, path[1024];
  int rc = 1;
  BkRenderer *renderer = bk_renderer_create(85, 64, stdout, e);
  BkResourceStore *store = bk_resources_create(e);
  if (!renderer || !store) goto done;
  const char *packs[] = {"bk3_00", "bk3_02", "bk3_03", "bk3_04", "bk3_06",
      "bk3_08", "bk3_09", "bk3_10", "bk3_11", "bk3_12", "bk3_13", "bk3_18", "fambom"};
  for (unsigned i = 0; i < sizeof(packs) / sizeof(*packs); ++i)
    if (snprintf(path, sizeof(path), "%s/%s.pp", argv[1], packs[i]) >= (int)sizeof(path) ||
        !bk_resources_mount(store, packs[i], path, e)) goto done;
  BkRenderStats baseline = bk_renderer_stats(renderer);
  AdapterResult out = {.timeline = {.state = UINT64_C(14695981039346656037),
                                    .pcm = UINT64_C(14695981039346656037)}};
  for (unsigned group = 0; group < (argc == 3 ? 1u : 5u); ++group)
    for (unsigned variant = 0; variant < 2; ++variant) {
      if (!adapter_profile(renderer, store, group, variant, &out, e)) goto done;
      BkRenderStats live = bk_renderer_stats(renderer);
      if (live.live_allocations != baseline.live_allocations || live.live_bytes != baseline.live_bytes) {
        snprintf(e, sizeof(e), "selected adapters leaked GPU resources"); goto done;
      }
    }
  printf("PASS selected adapters entries%u manual%u restarts%u flashes%u uv%u cancels%u replay%u completion%u auxiliary%u rejects%u frames%u state=%016llx pcm=%016llx\n",
      out.entries, out.manual, out.restart, out.flash, out.uv, out.cancel, out.replay, out.completion, out.auxiliary,
      out.rejects, out.timeline.frames, (unsigned long long)out.timeline.state,
      (unsigned long long)out.timeline.pcm);
  rc = 0;
done:
  if (rc) fprintf(stderr, "%s\n", e);
  bk_renderer_destroy(renderer);
  bk_resources_destroy(store);
  return rc;
}
