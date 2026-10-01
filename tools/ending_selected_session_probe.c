/*Actual Japanese resources through the production selected session. Direct
 *entries run their real opening. Cross-loader requests are explicit boundary
 *fixtures, not evidence of natural story input or an implemented phase8 loop.*/
#include "../runtime/scene/ending_normal_session.c"

typedef struct { uint64_t submitted, consumed, hash; } SelectedSink;
typedef struct {
  unsigned entries, transitions, openings, frames, redraws, replay_boundaries, menus, changes;
  uint64_t state, pcm;
} SelectedResult;
static uint64_t selected_hash(uint64_t h, const void *data, size_t n) {
  const unsigned char *p = data;
  for (size_t i = 0; i < n; ++i) h = (h ^ p[i]) * UINT64_C(1099511628211);
  return h;
}
static int probe_submit(void *p, const int16_t *samples, size_t frames, char e[256]) {
  (void)e;
  SelectedSink *s = p;
  s->hash = selected_hash(s->hash, samples, frames * 2 * sizeof(*samples));
  s->submitted += frames;
  return 1;
}
static int probe_poll(void *p, uint64_t *consumed, char e[256]) {
  (void)e;
  SelectedSink *s = p;
  uint64_t next = s->consumed + 800;
  s->consumed = next < s->submitted ? next : s->submitted;
  *consumed = s->consumed;
  return 1;
}
static int probe_present(BkScene *scene, BkRenderer *r, char e[256]) {
  return bk_renderer_begin(r, e) && bk_scene_draw(scene, &(BkSceneFrame){0}, e) &&
         bk_renderer_end(r, e) && bk_ending_normal_scene_after_present(scene, e);
}
static int probe_input_tick(BkScene *scene, EndingNormalScene *s, SelectedResult *out,
                             const BkInput *input, char e[256]) {
  if (!bk_audio_poll(s->services.audio, e) ||
      !bk_scene_step(scene, 1.0 / 60.0, input, e) ||
      !bk_audio_fill(s->services.audio, e) ||
      !probe_present(scene, s->services.renderer, e)) return 0;
  ++out->frames;
  /*Only scalar state: no pointer identities or struct padding.*/
  int32_t values[] = {s->state->frame.phase, s->state->auxiliary.gate,
      s->state->selected, s->state->control.variant,
      s->state->retained.auxiliary.byte_6ea358, s->state->frame.camera_mode};
  out->state = selected_hash(out->state, values, sizeof(values));
  return 1;
}
static int probe_tick(BkScene *scene, EndingNormalScene *s, SelectedResult *out,
                       char e[256]) {
  return probe_input_tick(scene, s, out, &(BkInput){0}, e);
}
static int probe_load(void *p, BkEndingLoader loader, int32_t arg, char e[256]) {
  EndingNormalScene *s = p;
  if (loader != BK_ENDING_LOAD_4D1025) return 0;
  int32_t before[] = {s->state->frame.group, s->state->auxiliary.variant, arg,
      s->state->selected, s->state->auxiliary.gate, s->state->frame.state_721ee0,
      s->state->frame.state_721ee4, s->state->frame.camera_event,
      s->state->control.target_choice, s->state->control.variant,
      s->inventory[1], s->inventory[2]};
  if (!ui_reload_load(s, loader, arg, e)) return 0;
  /*Consumed by the fixed-EXE oracle, which executes loader scalar regions.
   *It checks untouched parent states as well as the actual initialized gate.*/
  printf("SELECTED_LOAD");
  for (unsigned i = 0; i < sizeof(before) / sizeof(*before); ++i) printf(" %d", before[i]);
  int32_t after[] = {s->state->frame.phase, s->state->selected,
      s->state->auxiliary.gate, s->state->frame.state_721ee0,
      s->state->frame.state_721ee4, s->state->frame.camera_event,
      s->state->control.target_choice, s->state->control.variant,
      s->state->auxiliary.selection, s->state->auxiliary.pending,
      s->state->frame.camera_clip, s->state->control.toggles[3],
      s->state->control.toggles[4], s->state->control.toggles[5],
      s->state->control.toggles[6],
      bk_ending_selected_assets_replaced_background(s->selected_assets)};
  for (unsigned i = 0; i < sizeof(after) / sizeof(*after); ++i) printf(" %d", after[i]);
  putchar('\n');
  return 1;
}
#define VERIFY(x) do { if (!(x)) { \
  fprintf(stderr, "selected session group%u variant%u mode%u arg%u line%d (%s): %s\n", \
          group, variant, mode, arg, __LINE__, #x, e); goto done; } } while (0)
static int profile(BkRenderer *renderer, BkResourceStore *store, unsigned group,
                     unsigned variant, unsigned mode, unsigned arg,
                     SelectedResult *out, char e[256]) {
  int ok = 0;
  BkScene *scene = NULL;
  BkAudio *audio = NULL;
  BkEndingRecords *records = NULL;
  SelectedSink sink = {.hash = UINT64_C(14695981039346656037)};
  BkAudioSink output = {&sink, 48000, 480, 1920, probe_submit, probe_poll};
  VERIFY(audio = bk_audio_create(&output, e));
  BkSceneServices services = {store, renderer, stdout, audio, NULL, NULL};
  uint8_t unlocked[5][8] = {{0}};
  if (!mode) {
    VERIFY(scene = bk_ending_selected_scene_create_gallery(&services, group,
        variant, variant ? 5 : 2, unlocked, NULL, e));
  } else {
    VERIFY(records = calloc(1, sizeof(*records)));
    VERIFY(scene = bk_ending_normal_scene_create_story(&services, group,
        variant, records, unlocked, NULL, e));
  }
  EndingNormalScene *s = bk_scene_custom_context(scene);
  VERIFY(s && s->frame_active);
  VERIFY(bk_audio_fill(audio, e) && probe_present(scene, renderer, e));
  uint8_t pixels[85 * 64 * 4], redraw[sizeof(pixels)];
  if (mode) {
    VERIFY(probe_tick(scene, s, out, e));
    BkEndingBackgroundAssets *old_background = variant
        ? bk_ending_tertiary_assets_background(s->tertiary_assets)
        : bk_ending_normal_assets_background(s->assets);
    BkEndingNormalRender *old_render = s->render;
    BkEndingAudio *audio_owner = s->audio;
    VERIFY(bk_renderer_readback(renderer, pixels, sizeof(pixels), e));
    /*Only the boundary request is supplied. For mode2 the parent phase8
     *dispatch is an explicit fixture; no unimplemented child is invoked.*/
    if (mode == 2) {
      s->previous_flow = 0x18;
      s->state->frame.phase = 8;
      s->state->frame.state_721ee0 = variant ? 7 : 4;
      s->state->frame.state_721ee4 = variant ? 1500 : 701;
      s->state->selected = (int32_t)arg + 2;
    }
    s->state->frame.camera_event = 83;
    s->state->control.target_choice = 2;
    s->state->auxiliary.gate = 71;
    s->state->auxiliary.selection = (int32_t)arg;
    s->common->action = 7;
    s->common->blocked = 1;
    int32_t preserved_ee4 = s->state->frame.state_721ee4;
    BkEndingReloadBindings bindings;
    VERIFY(bk_ending_state_reload_bindings(s->state, s->common,
                                           &s->previous_flow, &bindings));
    BkEndingReloadOps ops = {s, probe_load, ui_reload_release,
        ui_reload_final_image, ui_reload_leave, ui_reload_lighting,
        ui_reload_schedule, ui_reload_present, ui_reload_status, ui_reload_pause};
    int early = -1;
    VERIFY(bk_ending_reload_transition(&bindings, 3, &ops, &early, e));
    VERIFY(!early && !s->common->action && !s->common->blocked);
    VERIFY(s->state->frame.phase == (mode == 2 ? 8 : (int32_t)(5 + variant)));
    VERIFY(s->selected_assets && s->state->auxiliary.gate == 4 &&
        s->state->frame.state_721ee4 == preserved_ee4 &&
        s->state->frame.camera_event == 0 && s->state->control.target_choice == 0 &&
        s->state->control.variant == 2 + arg + 5 * variant);
    int replaced = group == 1 && !variant && mode != 2;
    int32_t expected_selected = mode == 2 ? (int32_t)arg + 2 : variant ? 6 : replaced ? 1 : 0;
    VERIFY(s->state->selected == expected_selected);
    VERIFY(bk_ending_selected_assets_replaced_background(s->selected_assets) == replaced);
    VERIFY((bk_ending_selected_assets_background(s->selected_assets) == old_background) == !replaced);
    VERIFY(s->audio == audio_owner && s->render != old_render &&
        s->retired.render == old_render && s->snapshot_render == old_render);
    BkEndingState post = *s->state;
    for (unsigned i = 0; i < 3; ++i) {
      VERIFY(probe_present(scene, renderer, e));
      VERIFY(bk_renderer_readback(renderer, redraw, sizeof(redraw), e));
      VERIFY(!memcmp(pixels, redraw, sizeof(pixels)) && !memcmp(&post, s->state, sizeof(post)));
      VERIFY(s->retired.render == old_render && s->snapshot_render == old_render);
      ++out->redraws;
    }
    ++out->transitions;
    if (mode == 2) {
      VERIFY(s->state->frame.state_721ee0 == 6);
      ++out->replay_boundaries;
      goto success; /*Do not pretend the missing phase8 parent is implemented.*/
    }
    VERIFY(probe_tick(scene, s, out, e));
    VERIFY(!s->retired.render && s->snapshot_render == s->render);
  } else {
    VERIFY(s->selected_assets && s->state->auxiliary.gate == 4);
    VERIFY(s->state->selected == (group == 1 && !variant ? 1 : -1));
    VERIFY(!strcmp(bk_ending_background_assets_name(
        bk_ending_selected_assets_background(s->selected_assets)),
        group == 1 && !variant ? "m02_90.xan" : bk_ending_normal_background(group, variant)));
  }
  {
    unsigned opening = 0;
    while (s->state->auxiliary.gate == 4 && opening++ < 1800)
      VERIFY(probe_tick(scene, s, out, e));
    VERIFY(s->state->auxiliary.gate == 1 && opening > 200 && opening < 1800);
    VERIFY(s->state->frame.camera_mode == 0 && s->camera.fov == .2f);
    VERIFY(s->state->retained.auxiliary.byte_6ea358 == 0);
    VERIFY(s->state->auxiliary.index == (variant ? 90 : 36) + s->state->auxiliary.selection * 6);
    int present;
    VERIFY(bk_ending_audio_present(s->audio, 0, &present) && present);
    for (unsigned i = 0; i < 90; ++i) VERIFY(probe_tick(scene, s, out, e));
    VERIFY(s->state->auxiliary.gate == 1 && s->state->frame.phase == (int32_t)(5 + variant));
    /*Aim at the real projected node4 and click through the public input
     *path. A fixed screen-radius approximation cannot open this menu.*/
    int32_t point[2];
    VERIFY(bk_ending_ui_project_target(&s->ui_pick, 4, point, e));
    BkInput pointer = {.pointer_active = 1,
        .pointer_x = (float)s->viewport.x + (float)point[0],
        .pointer_y = (float)s->viewport.y + (float)point[1]};
    VERIFY(probe_input_tick(scene, s, out, &pointer, e));
    pointer.held = pointer.pressed = BK_BUTTON_CONFIRM;
    VERIFY(probe_input_tick(scene, s, out, &pointer, e));
    if (s->state->auxiliary.gate != 3)
      snprintf(e, 256, "node4=(%d,%d) selected%d event%d gate%d open%d", point[0], point[1],
          s->state->frame.camera_cached, s->state->frame.camera_event,
          s->state->auxiliary.gate, s->state->open);
    VERIFY(s->state->auxiliary.gate == 3 && s->state->ui_controller.auxiliary.mode == 3);
    VERIFY(s->state->choices[0] != s->state->auxiliary.selection &&
        s->state->choices[1] != s->state->auxiliary.selection && s->state->choices[2] == -1);
    VERIFY(s->state->auxiliary.expression_a == 6 && s->state->auxiliary.expression_b == 6);
    ++out->menus;
    int32_t previous = s->state->auxiliary.selection;
    int32_t next = s->state->choices[0];
    BkEndingNormalRender *old_render = s->render;
    int32_t count = records ? records->groups[group].count : 0;
    pointer.pointer_x = (float)s->viewport.x + (float)s->state->points[0][0];
    pointer.pointer_y = (float)s->viewport.y + (float)s->state->points[0][1];
    pointer.held = pointer.pressed = 0;
    pointer.released = BK_BUTTON_CONFIRM;
    VERIFY(probe_input_tick(scene, s, out, &pointer, e));
    VERIFY(s->state->auxiliary.selection == next && s->state->selected == previous + 2);
    VERIFY(s->common->action == 63 && s->common->blocked == 1);
    if (records) {
      VERIFY(records->groups[group].count == count + 1);
      VERIFY(records->groups[group].actions[count] == next + 12);
    }
    unsigned wait = 0;
    while (s->render == old_render && wait++ < 2400)
      VERIFY(probe_tick(scene, s, out, e));
    VERIFY(s->render != old_render && s->retired.render == old_render &&
        s->snapshot_render == old_render && s->selected_assets && wait < 2400);
    VERIFY(s->state->auxiliary.gate == 4 && s->state->frame.phase == (int32_t)(5 + variant));
    VERIFY(s->state->control.variant == 2 + next + 5 * variant);
    VERIFY(!s->common->action && !s->common->blocked);
    VERIFY(bk_ending_selected_assets_background_first(s->selected_assets));
    VERIFY(bk_renderer_readback(renderer, pixels, sizeof(pixels), e));
    VERIFY(probe_present(scene, renderer, e));
    VERIFY(bk_renderer_readback(renderer, redraw, sizeof(redraw), e));
    VERIFY(!memcmp(pixels, redraw, sizeof(pixels)));
    ++out->redraws;
    wait = 0;
    while ((s->state->auxiliary.gate == 4 || s->retired.render) && wait++ < 1800)
      VERIFY(probe_tick(scene, s, out, e));
    VERIFY(s->state->auxiliary.gate == 1 && !s->retired.render && wait < 1800);
    ++out->changes;
    ++out->openings;
  }
success:
  ++out->entries;
  out->pcm = selected_hash(out->pcm, &sink.hash, sizeof(sink.hash));
  ok = 1;
done:
  bk_scene_destroy(scene);
  bk_audio_destroy(audio);
  free(records);
  return ok;
}
int main(int argc, char **argv) {
  if (argc < 2 || argc > 3) return 2;
  int mode_filter = argc == 3 ? atoi(argv[2]) : -1;
  char e[256] = {0}, path[1024];
  int rc = 1;
  BkRenderer *renderer = bk_renderer_create(85, 64, stdout, e);
  BkResourceStore *store = bk_resources_create(e);
  if (!renderer || !store) goto done;
  const char *packs[] = {"bk3_00", "bk3_02", "bk3_03", "bk3_04", "bk3_06",
      "bk3_08", "bk3_09", "bk3_10", "bk3_11", "bk3_13", "bk3_18", "fambom"};
  for (unsigned i = 0; i < sizeof(packs) / sizeof(*packs); ++i)
    if (snprintf(path, sizeof(path), "%s/%s.pp", argv[1], packs[i]) >= (int)sizeof(path) ||
        !bk_resources_mount(store, packs[i], path, e)) goto done;
  BkRenderStats baseline = bk_renderer_stats(renderer);
  SelectedResult out = {.state = UINT64_C(14695981039346656037),
                         .pcm = UINT64_C(14695981039346656037)};
  for (unsigned mode = 0; mode < 3; ++mode) {
    if (mode_filter >= 0 && mode != (unsigned)mode_filter) continue;
    for (unsigned group = 0; group < 5; ++group)
      for (unsigned variant = 0; variant < 2; ++variant)
        for (unsigned arg = 0; arg < (mode ? 3u : 1u); ++arg) {
          if (!profile(renderer, store, group, variant, mode, arg, &out, e)) goto done;
          BkRenderStats live = bk_renderer_stats(renderer);
          if (live.live_allocations != baseline.live_allocations || live.live_bytes != baseline.live_bytes) {
            snprintf(e, sizeof(e), "selected session leaked GPU resources");
            goto done;
          }
        }
  }
  printf("PASS selected session entries%u transitions%u openings%u frames%u redraws%u replay-boundaries%u menus%u changes%u state=%016llx pcm=%016llx\n",
      out.entries, out.transitions, out.openings, out.frames, out.redraws, out.replay_boundaries, out.menus, out.changes,
      (unsigned long long)out.state, (unsigned long long)out.pcm);
  rc = 0;
done:
  if (rc) fprintf(stderr, "%s\n", e);
  bk_renderer_destroy(renderer);
  bk_resources_destroy(store);
  return rc;
}
