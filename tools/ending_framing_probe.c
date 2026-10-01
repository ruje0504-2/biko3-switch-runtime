/* Real scene camera/input integration. Private state is inspected through
 * the implementation, with no production test-only setters or asset edits.
 * Normal story entries use4cf318. Other loader topologies are not asserted. */
#include "../runtime/scene/ending_normal_session.c"

typedef struct { uint64_t submitted, consumed, hash; int hold; } FramingSink;
typedef struct {
  uint64_t camera_hash;
  unsigned frames, clicks, camera_calls, absent_nodes;
  unsigned opening_frames, frozen_voice_frames;
  unsigned action_frames, release_checks;
  unsigned display_switches, dual_view_frames, redraw_checks;
  int32_t action_target;
  uint64_t pcm_hash;
  float head_ndc[3];
} FramingResult;

static int framing_submit(void *p, const int16_t *pcm, size_t n, char e[256]) {
  (void)e;
  FramingSink *sink = p;
  const uint8_t *bytes = (const uint8_t *)pcm;
  for (size_t i = 0; i < n * 2 * sizeof(*pcm); ++i)
    sink->hash = (sink->hash ^ bytes[i]) * UINT64_C(1099511628211);
  sink->submitted += n;
  return 1;
}
static int framing_poll(void *p, uint64_t *n, char e[256]) {
  (void)e;
  FramingSink *sink = p;
  if (!sink->hold) {
    uint64_t next = sink->consumed + 800; /*48000Hz /60 actual game ticks*/
    sink->consumed = next < sink->submitted ? next : sink->submitted;
  }
  *n = sink->consumed;
  return 1;
}
static uint64_t framing_hash(uint64_t h, const void *v, size_t size) {
  const uint8_t *p = v;
  for (size_t i = 0; i < size; ++i)
    h = (h ^ p[i]) * UINT64_C(1099511628211);
  return h;
}
static uint64_t framing_snapshot_state(const EndingNormalScene *s) {
  uint64_t h = UINT64_C(14695981039346656037);
  h = framing_hash(h, s->state, sizeof((*s->state)));
  h = framing_hash(h, &s->camera, sizeof(s->camera));
  h = framing_hash(h, s->normal_controller, sizeof(*s->normal_controller));
  h = framing_hash(h, s->presentation, sizeof(*s->presentation));
  h = framing_hash(h, s->random, sizeof(*s->random));
  h = framing_hash(h, s->duck_transition, sizeof(*s->duck_transition));
  uint32_t movie = bk_ending_normal_render_movie_frame(s->render);
  return framing_hash(h, &movie, sizeof(movie));
}
static int framing_present(BkScene *scene, EndingNormalScene *s, char e[256]) {
  BkSceneFrame frame = {0};
  return bk_renderer_begin(s->services.renderer, e) &&
         bk_scene_draw(scene, &frame, e) &&
         bk_renderer_end(s->services.renderer, e) &&
         bk_ending_normal_scene_after_present(scene, e);
}
static int framing_tick(BkScene *scene, EndingNormalScene *s, BkInput in,
                         FramingResult *out, char e[256]) {
  if (!bk_audio_poll(s->services.audio, e) ||
      !bk_scene_step(scene, 1.0 / 60.0, &in, e) ||
      !bk_audio_fill(s->services.audio, e) || !framing_present(scene, s, e))
    return 0;
  BkNodeReference anchor;
  if (!bk_actor_forest_anchor_reference(
          bk_ending_normal_assets_forest(s->assets), 1, &anchor, e) ||
      memcmp(anchor.local, s->ui_camera_local, sizeof(anchor.local)))
    return fail(e, "UI camera sector does not use the displayed camera");
  out->camera_hash = framing_hash(out->camera_hash, &s->camera,
                                   sizeof(s->camera));
  out->camera_hash = framing_hash(out->camera_hash, s->ui_camera_local,
                                   sizeof(s->ui_camera_local));
  ++out->frames;
  return 1;
}
#define VERIFY(x) do { if (!(x)) { \
  fprintf(stderr, "framing group%u %ux%u line%d (%s): %s\n", group, width, height, \
            __LINE__, #x, e); goto done; } } while (0)

static int framing_run(const char *data, unsigned group, unsigned width,
                         unsigned height, FramingResult *out) {
  char e[256] = {0}, path[1024];
  BkRenderer *renderer = NULL;
  BkResourceStore *store = NULL;
  BkAudio *audio = NULL;
  BkScene *scene = NULL;
  BkEndingRecords *records = NULL;
  uint8_t *pixels = NULL, *clear = NULL, *redraw = NULL;
  int ok = 0;
  FramingSink sink = {.hash = UINT64_C(14695981039346656037)};
  *out = (FramingResult){.camera_hash = UINT64_C(14695981039346656037)};
  VERIFY(renderer = bk_renderer_create(width, height, stdout, e));
  VERIFY(store = bk_resources_create(e));
  const char *packs[] = {"bk3_00", "bk3_02", "bk3_03", "bk3_04",
                         "bk3_06", "bk3_08", "bk3_18", "fambom"};
  for (unsigned i = 0; i < sizeof(packs) / sizeof(*packs); ++i) {
    snprintf(path, sizeof(path), "%s/%s.pp", data, packs[i]);
    VERIFY(bk_resources_mount(store, packs[i], path, e));
  }
  BkAudioSink output = {&sink, 48000, 240, 960, framing_submit, framing_poll};
  VERIFY(audio = bk_audio_create(&output, e));
  BkSceneServices services = {store, renderer, NULL, audio, NULL, NULL};
  BkRenderStats baseline = bk_renderer_stats(renderer);
  size_t size = (size_t)width * height * 4;
  VERIFY(clear = malloc(size));
  VERIFY(pixels = malloc(size));
  VERIFY(redraw = malloc(size));
  VERIFY(bk_renderer_begin(renderer, e) && bk_renderer_end(renderer, e) &&
          bk_renderer_readback(renderer, clear, size, e));
  VERIFY(records = calloc(1, sizeof(*records)));
  const uint8_t unlocked[5][8] = {{0}};
  VERIFY(scene = bk_ending_normal_scene_create_story(
      &services, group, 0, records, unlocked, NULL, e));
  EndingNormalScene *s = bk_scene_custom_context(scene);
  BkActorForest *forest = bk_ending_normal_assets_forest(s->assets);
  float loaded[3][3];
  for (unsigned i = 0; i < 3; ++i) {
    const float *p = bk_ending_normal_assets_target(s->assets, i);
    VERIFY(p);
    memcpy(loaded[i], p, sizeof(loaded[i]));
  }
  VERIFY(!memcmp(loaded, s->state->control.targets, sizeof(loaded)));
  VERIFY(!memcmp(loaded[0], s->state->frame.camera_values, sizeof(loaded[0])));
  VERIFY(!memcmp(s->state->frame.camera_table,
                 bk_ending_normal_assets_config(s->assets)->camera_table,
                 sizeof(s->state->frame.camera_table)));
  for (unsigned i = 0; i < BK_ENDING_NORMAL_NODES; ++i)
    out->absent_nodes += !s->ui_present[i];
  float vp[16], clip[4];
  bk_matrix_multiply(vp, s->ui_view, s->ui_projection);
  bk_matrix_point(clip, s->ui_world[0] + 12, vp);
  VERIFY(isfinite(clip[3]) && clip[3] > 0);
  for (unsigned i = 0; i < 3; ++i)
    out->head_ndc[i] = clip[i] / clip[3];
  /* The reported group0 failure had head Y=-4.4927, outside clip[-1,1].
   * This is a framing regression, not a claim that every authored camera
   * must always show the complete body. */
  if (!group)
    VERIFY(fabsf(out->head_ndc[0]) < 1 && fabsf(out->head_ndc[1]) < 1 &&
            out->head_ndc[2] > 0 && out->head_ndc[2] < 1);
  VERIFY(framing_present(scene, s, e));
  float scale = (float)((double)s->viewport.width / 1280.0);
  BkInput hover = {.pointer_active = 1,
                    .pointer_x = s->viewport.x + 1200 * scale,
                    .pointer_y = s->viewport.y + 730 * scale};
  for (unsigned i = 0; i < 129; ++i)
    VERIFY(framing_tick(scene, s, hover, out, e));
  /* The recovered opening owns camera/FOV after common toolbar input. Wait
   * for its actual speech/camera handoff instead of assuming that129 ticks
   * means idle. Freeze consumption, not the game clock, to expose fabricated
   * speech-duration gates. Neither phase nor actor state is overwritten. */
  while (s->state->frame.state_721ee0 != 1 && out->opening_frames < 6000) {
    int playing = 0;
    BkEndingAudioCall status = {.operation = BK_ENDING_AUDIO_STATUS, .slot = 0};
    VERIFY(bk_ending_audio_call(s->audio, group, 0, 0, &status, &playing, e));
    sink.hold = s->state->frame.state_721ee0 == 4 &&
                s->state->retained.normal.word_719b50 == 3 &&
                s->state->retained.normal.word_719b24 && playing &&
                out->frozen_voice_frames < 48;
    VERIFY(framing_tick(scene, s, hover, out, e));
    if (sink.hold) {
      VERIFY(s->state->frame.state_721ee0 == 4 &&
              s->state->retained.normal.word_719b50 == 3 &&
              s->state->retained.normal.word_719b24);
      ++out->frozen_voice_frames;
    }
    ++out->opening_frames;
  }
  sink.hold = 0;
  VERIFY(s->state->frame.state_721ee0 == 1 && out->frozen_voice_frames == 48);
  /* Actual pointer input hits the current animated toolbar bounds. Check
   * captured OLD worlds, not the post-update nodes or the load snapshots. */
  const unsigned buttons[] = {3, 3, 3, 0, 5};
  unsigned different_from_load = 0;
  for (unsigned click = 0; click < sizeof(buttons) / sizeof(*buttons); ++click) {
    float before[3][3];
    const unsigned indices[] = {5, 13, 0};
    for (unsigned i = 0; i < 3; ++i) {
      const float *p = ending_cached_target(s, indices[i]);
      VERIFY(p);
      memcpy(before[i], p, sizeof(before[i]));
    }
    different_from_load += memcmp(before, loaded, sizeof(before)) != 0;
    BkEndingControlRect rects[BK_ENDING_CONTROL_RECTS];
    VERIFY(bk_ending_ui_control_rects(&s->ui, rects));
    const BkEndingControlRect *rect = rects + buttons[click];
    BkInput input = {.pointer_active = 1,
                      .pointer_x = s->viewport.x + rect->x + rect->width * .5f,
                      .pointer_y = s->viewport.y + rect->y + rect->height * .5f,
                      .pressed = BK_BUTTON_CONFIRM,
                      .held = BK_BUTTON_CONFIRM};
    VERIFY(framing_tick(scene, s, input, out, e));
    VERIFY(!memcmp(before, s->state->control.targets, sizeof(before)));
    unsigned selected = click < 3 ? (click + 1) % 3 : 0;
    VERIFY(s->state->control.target_choice == (int32_t)selected);
    VERIFY(!memcmp(before[selected], s->state->frame.camera_values,
                    sizeof(before[selected])));
    ++out->clicks;
    input.held = 0;
    input.pressed = 0;
    input.released = BK_BUTTON_CONFIRM;
    VERIFY(framing_tick(scene, s, input, out, e));
  }
  VERIFY(different_from_load);
  /* Real idle -> held-miss -> released-edge -> idle transition. Native
   * mode0 is idle-up, so neither a held nor just-released button completes
   * state2. No controller state or target is assigned by this fixture. */
  BkInput missed = {.pointer_active = 1, .pointer_x = s->viewport.x + 1,
                     .pointer_y = s->viewport.y + 1};
  for (unsigned i = 0; i < 90; ++i)
    VERIFY(framing_tick(scene, s, missed, out, e));
  missed.pressed = missed.held = BK_BUTTON_CONFIRM;
  VERIFY(framing_tick(scene, s, missed, out, e));
  VERIFY(s->state->frame.state_721ee0 == 2);
  missed.pressed = 0;
  VERIFY(framing_tick(scene, s, missed, out, e));
  VERIFY(s->state->frame.state_721ee0 == 2);
  missed.held = 0;
  missed.released = BK_BUTTON_CONFIRM;
  VERIFY(framing_tick(scene, s, missed, out, e));
  VERIFY(s->state->frame.state_721ee0 == 2);
  missed.released = 0;
  VERIFY(framing_tick(scene, s, missed, out, e));
  VERIFY(s->state->frame.state_721ee0 == 1);
  out->release_checks = 4;

  /* Aim at an actual projected, eligible resource node and select it
   * through the pointer. This covers the production parent/action/manual
   * presentation chain instead of injecting a ready state or clip. */
  const unsigned candidates[] = {11, 12, 9, 10, 26, 27, 1, 5};
  for (unsigned attempt = 0; attempt < 64 &&
                             s->state->frame.state_721ee0 != 3; ++attempt) {
    /* Some authored presets face away from the available action region.
     * Cycle the real camera toolbar button, then wait for its transition;
     * never bypass the original front/back selection gate. */
    if (attempt && attempt % 8 == 0) {
      for (unsigned i = 0; i < 48; ++i)
        VERIFY(framing_tick(scene, s, hover, out, e));
      BkEndingControlRect rects[BK_ENDING_CONTROL_RECTS];
      VERIFY(bk_ending_ui_control_rects(&s->ui, rects));
      BkInput preset = {.pointer_active = 1,
          .pointer_x = s->viewport.x + rects[0].x + rects[0].width * .5f,
          .pointer_y = s->viewport.y + rects[0].y + rects[0].height * .5f,
          .pressed = BK_BUTTON_CONFIRM, .held = BK_BUTTON_CONFIRM};
      VERIFY(framing_tick(scene, s, preset, out, e));
      ++out->clicks;
      for (unsigned i = 0; i < 65; ++i)
        VERIFY(framing_tick(scene, s, missed, out, e));
      VERIFY(s->state->frame.state_721ee0 == 1);
    }
    unsigned node = candidates[attempt % 8];
    if (!s->ui_present[node])
      continue;
    int32_t point[2];
    VERIFY(bk_ending_ui_project_target(&s->ui_pick, node, point, e));
    if (point[0] < 2 || point[1] < 2 ||
        point[0] >= (int32_t)s->viewport.width * 9 / 10 ||
        point[1] >= (int32_t)s->viewport.height - 2)
      continue;
    BkInput aim = {.pointer_active = 1,
                    .pointer_x = s->viewport.x + point[0],
                    .pointer_y = s->viewport.y + point[1]};
    VERIFY(framing_tick(scene, s, aim, out, e));
    if (attempt < 8)
      fprintf(stderr, "aim group%u node%u selected=%d kind=%d state=%d\n", group,
          node, s->state->frame.camera_cached,
          s->normal_controller->control.action_kind,
          s->state->frame.state_721ee0);
    if (s->state->frame.state_721ee0 != 1 ||
        s->state->frame.camera_cached < 0 ||
        s->normal_controller->control.action_kind < 0 ||
        s->normal_controller->control.action_kind >= 3)
      continue;
    aim.pressed = aim.held = BK_BUTTON_CONFIRM;
    VERIFY(framing_tick(scene, s, aim, out, e));
  }
  if (s->state->frame.state_721ee0 != 3) {
    fprintf(stderr, "action not entered: state=%d camera=%d cached=%d pending=%d "
                    "wait=%d kind=%d column=%d\n",
        s->state->frame.state_721ee0, s->state->frame.camera_mode,
        s->state->frame.camera_cached, s->state->auxiliary.pending,
        s->state->retained.normal.word_719b24,
        s->normal_controller->control.action_kind,
        s->normal_controller->control.action_column);
    for (unsigned i = 0; i < 8; ++i) {
      unsigned node = candidates[i];
      int32_t point[2];
      if (s->ui_present[node] &&
          bk_ending_ui_project_target(&s->ui_pick, node, point, e))
        fprintf(stderr, "node%u point=(%d,%d)\n", node, point[0], point[1]);
    }
  }
  VERIFY(s->state->frame.state_721ee0 == 3);
  out->action_target = s->state->frame.camera_cached;
  while (!s->state->normal_ready && out->action_frames < 900) {
    VERIFY(framing_tick(scene, s, (BkInput){.held = BK_BUTTON_CONFIRM}, out, e));
    ++out->action_frames;
  }
  VERIFY(s->state->normal_ready == 1 || s->state->normal_ready == 5);
  for (unsigned i = 0; i < 24; ++i) {
    BkInput motion = {.held = BK_BUTTON_CONFIRM,
                       .move_x = i % 2 ? .8f : -.8f};
    VERIFY(framing_tick(scene, s, motion, out, e));
    VERIFY(s->state->frame.state_721ee0 == 3);
    ++out->action_frames;
  }
  VERIFY(framing_tick(scene, s, (BkInput){.released = BK_BUTTON_CONFIRM}, out, e));
  while (s->state->frame.state_721ee0 != 1 && out->action_frames < 2400) {
    VERIFY(framing_tick(scene, s, (BkInput){0}, out, e));
    ++out->action_frames;
  }
  VERIFY(s->state->frame.state_721ee0 == 1 && !s->state->normal_ready);
  out->pcm_hash = sink.hash;
  /* Exercise the actual scene adapter at each recovered camera boundary.
   * The existing math/original oracle independently validates the formulas;
   * this checks which old published actor/track the scene passes to them. */
  for (unsigned kind = 0; kind < 2; ++kind) {
    BkMenuCamera expected = s->camera;
    const float *target = ending_cached_target(s, kind ? 0 : 1);
    VERIFY(target);
    BkEndingCall call = {.operation = kind ? BK_ENDING_CAMERA_4E1D16
                                           : BK_ENDING_CAMERA_4E1711,
                          .count = kind ? 4 : 1,
                          .args = {BK_ENDING_PLAYER}};
    if (!kind) {
      BkEndingCameraAssets *a = bk_ending_normal_assets_cameras(s->assets);
      const float *track = bk_actor_pose_frame(
          bk_ending_camera_assets_pose(a, 0), bk_ending_camera_assets_node(a, 0));
      VERIFY(track && bk_ending_camera_track_pose(
          &expected, track + 12, target, s->active_seconds, e));
    } else {
      VERIFY(bk_ending_camera_manual(&expected, (float[2]){0}, 0,
                                      (float[3]){0}, target,
                                      s->active_seconds, e));
    }
    s->input = (BkInput){0};
    uint32_t result = 0;
    VERIFY(frame_invoke(s, &call, &result, e));
    VERIFY(!memcmp(&expected, &s->camera, sizeof(expected)));
    VERIFY(prepare_ui_geometry(s, s->viewport.width, s->viewport.height, e));
    BkNodeReference anchor;
    VERIFY(bk_actor_forest_anchor_reference(forest, 1, &anchor, e));
    VERIFY(!memcmp(anchor.local, s->ui_camera_local, sizeof(anchor.local)));
    out->camera_hash = framing_hash(out->camera_hash, &s->camera,
                                     sizeof(s->camera));
    ++out->camera_calls;
  }
  /* Three real toolbar clicks select forced second view, audio-only, then
   * conditional view. No production mode/camera/voice setters are used. */
  VERIFY(s->state->frame.phase == 1 && s->state->control.mode_721ec4 == 0);
  VERIFY(s->special_viewport.x == s->viewport.x &&
          s->special_viewport.y == s->viewport.y + 270 &&
          s->special_viewport.width == 600 && s->special_viewport.height == 450);
  /* The action may have retracted the animated side toolbar. A real pointer
   * cannot click its offscreen rectangle; hover in its activation area and
   * wait for the actual rendered/control rectangle to return inside. */
  unsigned toolbar_wait = 0;
  for (;;) {
    BkEndingControlRect rects[BK_ENDING_CONTROL_RECTS];
    VERIFY(bk_ending_ui_control_rects(&s->ui, rects));
    const BkEndingControlRect *rect = rects + 1;
    if (rect->x >= 0 && rect->y >= 0 &&
        rect->x + rect->width <= s->viewport.width &&
        rect->y + rect->height <= s->viewport.height)
      break;
    VERIFY(toolbar_wait++ < 180);
    VERIFY(framing_tick(scene, s, hover, out, e));
  }
  for (unsigned selection = 0; selection < 3; selection++) {
    BkEndingControlRect rects[BK_ENDING_CONTROL_RECTS];
    VERIFY(bk_ending_ui_control_rects(&s->ui, rects));
    const BkEndingControlRect *rect = rects + 1;
    BkInput input = {.pointer_active = 1,
                      .pointer_x = s->viewport.x + rect->x + rect->width * .5f,
                      .pointer_y = s->viewport.y + rect->y + rect->height * .5f,
                      .pressed = BK_BUTTON_CONFIRM,
                      .held = BK_BUTTON_CONFIRM};
    VERIFY(framing_tick(scene, s, input, out, e));
    int32_t mode = (int32_t)(selection + 1) % 3;
    if (s->state->control.mode_721ec4 != mode || s->state->frame.phase != 1)
      snprintf(e, sizeof(e),
               "display mode expected%d got%d phase%d hover%d pointer(%.9g,%.9g) "
               "rect(%.9g,%.9g,%.9g,%.9g) state%d",
               mode, s->state->control.mode_721ec4, s->state->frame.phase,
               s->state->control.hover, s->pointer.position[0], s->pointer.position[1],
               rect->x, rect->y, rect->width, rect->height,
               s->state->frame.state_721ee0);
    VERIFY(s->state->control.mode_721ec4 == mode && s->state->frame.phase == 1);
    unsigned passes = bk_ending_normal_render_pass_count(s->render);
    if (mode == 1)
      VERIFY(passes == 5);
    else if (mode == 2)
      VERIFY(passes == 3);
    else
      VERIFY(passes == 3 || passes == 5);
    for (unsigned pass = 0; pass < passes; pass++)
      VERIFY(bk_ending_normal_render_pass_view(s->render, pass) == (pass >= 3));
    out->dual_view_frames += passes == 5;
    ++out->display_switches;
    ++out->clicks;
    VERIFY(bk_renderer_readback(renderer, pixels, size, e));
    uint64_t snapshot = framing_snapshot_state(s);
    uint64_t pcm = sink.hash;
    VERIFY(framing_present(scene, s, e));
    VERIFY(bk_renderer_readback(renderer, redraw, size, e));
    VERIFY(!memcmp(pixels, redraw, size) && framing_snapshot_state(s) == snapshot &&
            sink.hash == pcm);
    ++out->redraw_checks;
    input.pressed = input.held = 0;
    input.released = BK_BUTTON_CONFIRM;
    VERIFY(framing_tick(scene, s, input, out, e));
  }
  /* Both viewports and the UI must stay inside the fitted content after
   * mode changes; the side bars retain the original clear. */
  VERIFY(bk_renderer_readback(renderer, pixels, size, e));
  for (unsigned y = 0; y < height; ++y)
    for (unsigned x = 0; x < width; ++x)
      if (x < s->viewport.x || x >= s->viewport.x + s->viewport.width ||
          y < s->viewport.y || y >= s->viewport.y + s->viewport.height) {
        size_t i = ((size_t)y * width + x) * 4;
        VERIFY(!memcmp(pixels + i, clear + i, 4));
      }
  bk_scene_destroy(scene);
  scene = NULL;
  BkRenderStats final = bk_renderer_stats(renderer);
  VERIFY(final.live_allocations == baseline.live_allocations &&
          final.live_bytes == baseline.live_bytes);
  ok = 1;
done:
  bk_scene_destroy(scene);
  free(records); free(clear); free(pixels); free(redraw);
  bk_audio_destroy(audio);
  bk_resources_destroy(store);
  bk_renderer_destroy(renderer);
  return ok;
}

int main(int argc, char **argv) {
  if (argc != 2 && argc != 3)
    return 2;
  unsigned first = argc == 3 ? (unsigned)strtoul(argv[2], NULL, 10) : 0;
  unsigned end = argc == 3 ? first + 1 : 5;
  if (first >= 5)
    return 2;
  unsigned frames = 0, clicks = 0, calls = 0, cases = 0;
  unsigned display_switches = 0, dual_views = 0, redraw_checks = 0;
  for (unsigned group = first; group < end; ++group) {
    FramingResult a, b;
    if (!framing_run(argv[1], group, 960, 720, &a) ||
        !framing_run(argv[1], group, 1280, 720, &b))
      return 1;
    if (a.camera_hash != b.camera_hash || a.frames != b.frames ||
        a.clicks != b.clicks || a.camera_calls != b.camera_calls ||
        a.opening_frames != b.opening_frames ||
        a.frozen_voice_frames != b.frozen_voice_frames || a.pcm_hash != b.pcm_hash ||
        a.action_frames != b.action_frames || a.action_target != b.action_target ||
        a.release_checks != b.release_checks ||
        a.display_switches != b.display_switches ||
        a.dual_view_frames != b.dual_view_frames || a.redraw_checks != b.redraw_checks ||
        a.absent_nodes != b.absent_nodes ||
        memcmp(a.head_ndc, b.head_ndc, sizeof(a.head_ndc))) {
      fprintf(stderr, "framing group%u: content/frame state differs\n", group);
      return 1;
    }
    printf("framing group%u PASS head=(%.9g,%.9g,%.9g) absent=%u "
           "camera=%016llx opening=%u frozen_voice=%u pcm=%016llx "
           "action_target=%d action_frames=%u release_checks=%u "
           "display_switches=%u dual_views=%u redraws=%u\n", group,
           a.head_ndc[0], a.head_ndc[1], a.head_ndc[2], a.absent_nodes,
           (unsigned long long)a.camera_hash, a.opening_frames,
           a.frozen_voice_frames, (unsigned long long)a.pcm_hash,
           a.action_target, a.action_frames, a.release_checks,
           a.display_switches, a.dual_view_frames, a.redraw_checks);
    fflush(stdout);
    frames += a.frames + b.frames;
    clicks += a.clicks + b.clicks;
    calls += a.camera_calls + b.camera_calls;
    cases += 2;
    display_switches += a.display_switches + b.display_switches;
    dual_views += a.dual_view_frames + b.dual_view_frames;
    redraw_checks += a.redraw_checks + b.redraw_checks;
  }
  printf("ending framing PASS cases=%u frames=%u toolbar_clicks=%u "
         "camera_calls=%u display_switches=%u dual_views=%u redraws=%u\n",
         cases, frames, clicks, calls, display_switches, dual_views, redraw_checks);
  return 0;
}
