/*Production scene entry/frame/UI/input/render lifecycle. Private inspection
 * supplies gestures without test setters: all game-state changes use scene
 * input. The outer scheduler is an observed boundary, not a gallery menu. */
#include "../runtime/scene/ending_normal_session.c"

typedef struct {
  uint64_t submitted, consumed, hash;
  int freeze;
} SecondarySink;
typedef struct {
  BkScene *scene;
  unsigned calls;
  uint8_t target, mode;
  uint8_t request_action, request_curtain;
} SecondarySchedule;
typedef struct {
  unsigned entries, frames, states, menus, frozen, redraws, schedules;
  unsigned hidden_background, dual_views, focused;
  unsigned display_switches;
  uint64_t state_hash, pcm_hash;
} SecondaryResult;
static uint64_t secondary_hash(uint64_t h, const void *v, size_t n) {
  const uint8_t *p = v;
  for (size_t i = 0; i < n; ++i) h = (h ^ p[i]) * UINT64_C(1099511628211);
  return h;
}
static int secondary_submit(void *p, const int16_t *pcm, size_t n, char e[256]) {
  (void)e;
  SecondarySink *sink = p;
  sink->hash = secondary_hash(sink->hash, pcm, n * 2 * sizeof(*pcm));
  sink->submitted += n;
  return 1;
}
static int secondary_poll(void *p, uint64_t *n, char e[256]) {
  (void)e;
  SecondarySink *sink = p;
  if (!sink->freeze) {
    uint64_t next = sink->consumed + 800;
    sink->consumed = next < sink->submitted ? next : sink->submitted;
  }
  *n = sink->consumed;
  return 1;
}
static int secondary_schedule(void *p, uint8_t target, uint8_t mode, char e[256]) {
  SecondarySchedule *s = p;
  if (!s || !s->scene || s->calls)
    return fail(e, "duplicate or construction-time schedule request");
  s->target = target;
  s->mode = mode;
  const BkEndingState *state = bk_ending_normal_scene_state(s->scene);
  if (!state)
    return fail(e, "missing ending state at scheduler boundary");
  s->request_action = state->frame.transition_action;
  s->request_curtain = state->frame.curtain_wanted;
  ++s->calls;
  return bk_ending_normal_scene_stop(s->scene, e);
}
static uint64_t secondary_snapshot(EndingNormalScene *s) {
  uint64_t h = UINT64_C(14695981039346656037);
  h = secondary_hash(h, s->state, sizeof((*s->state)));
  h = secondary_hash(h, &s->camera, sizeof(s->camera));
  h = secondary_hash(h, s->common, sizeof(*s->common));
  h = secondary_hash(h, &s->ui, sizeof(s->ui));
  h = secondary_hash(h, &s->stage_ui, sizeof(s->stage_ui));
  h = secondary_hash(h, &s->pointer, sizeof(s->pointer));
  h = secondary_hash(h, s->secondary_controller, sizeof(*s->secondary_controller));
  h = secondary_hash(h, s->secondary_presentation, sizeof(*s->secondary_presentation));
  h = secondary_hash(h, s->presentation, sizeof(*s->presentation));
  h = secondary_hash(h, s->normal_controller, sizeof(*s->normal_controller));
  h = secondary_hash(h, s->random, sizeof(*s->random));
  h = secondary_hash(h, &s->wall_seconds, sizeof(s->wall_seconds));
  uint32_t movie = bk_ending_normal_render_movie_frame(s->render);
  return secondary_hash(h, &movie, sizeof(movie));
}
static int secondary_present(BkScene *scene, EndingNormalScene *s, char e[256]) {
  return bk_renderer_begin(s->services.renderer, e) &&
         bk_scene_draw(scene, &(BkSceneFrame){0}, e) &&
         bk_renderer_end(s->services.renderer, e) &&
         bk_ending_normal_scene_after_present(scene, e);
}
static int secondary_tick(BkScene *scene, EndingNormalScene *s, BkInput in,
                          SecondaryResult *out, char e[256]) {
  if (!bk_audio_poll(s->services.audio, e) ||
      !bk_scene_step(scene, 1.0 / 60.0, &in, e) ||
      !bk_audio_fill(s->services.audio, e) || !secondary_present(scene, s, e))
    return 0;
  BkNodeReference camera;
  if (!bk_actor_forest_anchor_reference(scene_forest(s), 1, &camera, e) ||
      memcmp(camera.local, s->ui_camera_local, sizeof(camera.local)))
    return fail(e, "UI picking does not use the published display camera");
  int state = s->state->frame.state_721ee4;
  if (state >= 0 && state < 8) out->states |= 1u << (unsigned)state;
  unsigned passes = bk_ending_normal_render_pass_count(s->render);
  out->dual_views += passes == 3;
  out->hidden_background += passes && !bk_ending_normal_render_queue_count(s->render, 0);
  out->state_hash = secondary_hash(out->state_hash, &s->state->frame,
                                    sizeof(s->state->frame));
  ++out->frames;
  return 1;
}
/*Copy every mutable picker destination. Planning cannot publish a world,
 * alter a ring, or change the controller's preferred node or event. */
static int secondary_hit(EndingNormalScene *s, float x, float y, int wanted,
                         char e[256]) {
  BkEndingFrameState frame = s->state->frame;
  int32_t kind = s->normal_controller->control.action_kind;
  int32_t column = s->normal_controller->control.action_column;
  int32_t targets[39][2], alternate[2], result;
  memcpy(targets, s->state->targets, sizeof(targets));
  memcpy(alternate, s->state->alternate, sizeof(alternate));
  BkEndingUiSprite ring = s->ui.sprites[50];
  const float *world = bk_actor_forest_world(scene_forest(s),
      (uint32_t)s->state->retained.normal.word_719b40);
  BkEndingSecondaryPickBindings b = {&frame, &kind, &column, targets, alternate,
                                     &ring, &s->ui_pick, world};
  if (!bk_ending_secondary_pick(&b, (float[2]){x, y}, 0, &result, e)) return -1;
  return result == wanted;
}
static int secondary_gesture(EndingNormalScene *s, int wanted, BkInput *in,
                             char e[256]) {
  int32_t center[2] = {0};
  if (wanted) {
    BkEndingUiPickBindings geometry = s->ui_pick;
    uint8_t present = 1;
    if (wanted == 2) {
      geometry.world = (const float (*)[16])bk_actor_forest_world(scene_forest(s),
          (uint32_t)s->state->retained.normal.word_719b40);
      geometry.present = &present;
      geometry.count = 1;
    }
    if (!bk_ending_ui_project_target(&geometry, 0, center, e)) return 0;
    for (int radius = 0; radius <= 64; ++radius)
      for (int dy = -radius; dy <= radius; ++dy)
        for (int dx = -radius; dx <= radius; ++dx) {
          if (abs(dx) != radius && abs(dy) != radius) continue;
          int64_t x = (int64_t)center[0] + dx, y = (int64_t)center[1] + dy;
          if (x < 0 || y < 0 || x >= s->viewport.width || y >= s->viewport.height) continue;
          int hit = secondary_hit(s, (float)x, (float)y, wanted, e);
          if (hit < 0) return 0;
          if (hit) {
            in->pointer_active = 1;
            in->pointer_x = s->viewport.x + (float)x;
            in->pointer_y = s->viewport.y + (float)y;
            return 1;
          }
        }
  }
  for (unsigned y = 4; y < s->viewport.height; y += 8)
    for (unsigned x = 4; x < s->viewport.width; x += 8) {
      int hit = secondary_hit(s, (float)x, (float)y, wanted, e);
      if (hit < 0) return 0;
      if (hit) {
        in->pointer_active = 1;
        in->pointer_x = (float)(s->viewport.x + x);
        in->pointer_y = (float)(s->viewport.y + y);
        return 1;
      }
    }
  snprintf(e, 256, "no visible event%d gesture: center%d,%d, camera%g,%g,%g fov%g",
      wanted, center[0], center[1], s->camera.pose.position[0],
      s->camera.pose.position[1], s->camera.pose.position[2], s->camera.fov);
  return 0;
}
#define VERIFY(x) do { if (!(x)) { \
  fprintf(stderr, "secondary session group%u variant%u line%d (%s): %s\n", \
      group, variant, __LINE__, #x, e); goto done; } } while (0)
static int secondary_profile(BkRenderer *renderer, BkResourceStore *store,
    unsigned group, unsigned variant, SecondaryResult *out, char e[256]) {
  int ok = 0;
  BkAudio *mixer = NULL;
  BkScene *scene = NULL;
  EndingNormalScene *s = NULL;
  uint8_t *pixels = NULL, *again = NULL;
  SecondarySink sink = {.hash = UINT64_C(14695981039346656037)};
  SecondarySchedule scheduled = {0};
  BkAudioSink output = {&sink, 48000, 480, 1920, secondary_submit, secondary_poll};
  VERIFY(mixer = bk_audio_create(&output, e));
  BkSceneServices services = {store, renderer, stdout, mixer, NULL};
  /*The constructor retains transition/timer words. The real application's
   * process allocation clears them before this first initialization.*/
  BkCommonHudState common = {0};
  bk_common_hud_initialize(&common);
  BkEndingAuxiliaryCycle cycle = bk_ending_auxiliary_cycle_initial();
  BkEndingNormalControllerRetained normal;
  bk_ending_normal_controller_initialize(&normal);
  BkEndingPresentationRetained presentation = {0};
  BkEndingSecondaryControlState controller = bk_ending_secondary_control_initial();
  BkEndingSecondaryPresentationState display = bk_ending_secondary_presentation_initial();
  BkEndingState state;
  bk_ending_state_initialize(&state);
  BkEndingTertiaryControllerRetained tertiary;
  bk_ending_tertiary_controller_initialize(&tertiary);
  uint32_t random = 123;
  int32_t duck = 0;
  BkEndingProcess process;
  bk_ending_process_initialize(&process);
  BkEndingNormalFlow flow = {.process = &process, .common = &common, .context = &scheduled,
      .schedule = secondary_schedule, .wall_seconds = 1,
      .auxiliary_cycle = &cycle, .random = &random, .normal_controller = &normal,
      .presentation = &presentation, .duck_transition = &duck,
      .secondary_controller = &controller, .secondary_presentation = &display,
      .state = &state, .tertiary_controller = &tertiary};
  uint8_t unlocked[5][8];
  for (unsigned g = 0; g < 5; ++g)
    for (unsigned i = 0; i < 8; ++i) unlocked[g][i] = (uint8_t)(7 + g * 13 + i * 19);
  BkRenderStats baseline = bk_renderer_stats(renderer);
  BkEndingNormalFlow missing = flow;
  missing.secondary_controller = NULL;
  VERIFY(!bk_ending_secondary_scene_create_gallery(&services, group, variant, unlocked, &missing, e));
  VERIFY(bk_renderer_stats(renderer).live_allocations == baseline.live_allocations);
  scene = bk_ending_secondary_scene_create_gallery(&services, group, variant, unlocked, &flow, e);
  VERIFY(scene);
  scheduled.scene = scene;
  s = bk_scene_custom_context(scene);
  VERIFY(s && s->secondary_assets && !s->assets && s->frame_active);
  VERIFY(s->secondary_controller == &controller && s->secondary_presentation == &display);
  VERIFY(s->presentation == &presentation && s->random == &random && s->common == &common);
  VERIFY(s->state->control.variant == 1 && s->state->auxiliary.variant == (int32_t)variant);
  VERIFY(s->state->frame.phase == 2);
  for (unsigned slot = 63; slot <= 69; ++slot)
    VERIFY(bk_ending_stage_ui_image(&s->stage_ui, slot));
  VERIFY(!bk_ending_stage_ui_image(&s->stage_ui, 70));
  VERIFY(!strcmp(bk_ending_stage_ui_image(&s->stage_ui, 64),
                 group == 1 ? "hs_67.tga" : "hs_59.tga"));
  VERIFY(!strcmp(bk_ending_stage_ui_image(&s->stage_ui, 65), "hs_73.tga"));
  VERIFY(s->materials == bk_ending_secondary_assets_materials(s->secondary_assets, 0));
  VERIFY(bk_audio_fill(mixer, e) && secondary_present(scene, s, e));
  unsigned width, height;
  bk_renderer_extent(renderer, &width, &height);
  size_t bytes = (size_t)width * height * 4;
  pixels = malloc(bytes); again = malloc(bytes);
  VERIFY(pixels && again && s->viewport.x > 0 && s->viewport.width == 320 && s->viewport.height == 240);
  unsigned operation = 0, local_frames = 0, held = 0, focus = 0;
  unsigned displays = 0, display_settle = 0, toolbar_wait = 0;
  int display_release = 0;
  int idle_clicked = 0, menu_held = 0;
  for (; local_frames < 18000 && !scheduled.calls; ++local_frames) {
    int playing0, playing1;
    VERIFY(bk_audio_playing(mixer, 0, &playing0) && bk_audio_playing(mixer, 1, &playing1));
    sink.freeze = playing0 && !idle_clicked && held < 12;
    if (sink.freeze) { ++held; ++out->frozen; }
    BkInput in = {.pointer_active = 1,
        .pointer_x = s->viewport.x + 4, .pointer_y = s->viewport.y + 4};
    int before = s->state->frame.state_721ee4;
    int clicked_display = -1;
    if (display_release) {
      in.released = BK_BUTTON_CONFIRM;
      display_release = 0;
    } else if (!common.blocked && before == 1 && !playing1) {
      if (displays < 3) {
        BkEndingControlRect rects[BK_ENDING_CONTROL_RECTS];
        VERIFY(bk_ending_ui_control_rects(&s->ui, rects));
        const BkEndingControlRect *rect = rects + 1;
        float scale = (float)((double)s->viewport.width / 1280.0);
        in.pointer_x = s->viewport.x + 1200 * scale;
        in.pointer_y = s->viewport.y + 730 * scale;
        if (rect->x >= 0 && rect->y >= 0 &&
            rect->x + rect->width <= s->viewport.width &&
            rect->y + rect->height <= s->viewport.height) {
          in.pointer_x = s->viewport.x + rect->x + rect->width * .5f;
          in.pointer_y = s->viewport.y + rect->y + rect->height * .5f;
          in.pressed = in.held = BK_BUTTON_CONFIRM;
          ++displays; ++out->display_switches;
          clicked_display = (int)(displays % 3);
          display_release = 1;
        } else VERIFY(++toolbar_wait < 240);
      } else if (display_settle < 60) {
        ++display_settle;
      } else if (!idle_clicked) {
        VERIFY(secondary_gesture(s, 0, &in, e));
        in.pressed = in.held = BK_BUTTON_CONFIRM;
        idle_clicked = 1;
      } else if (operation == 4 && focus < 232) {
        float scale = (float)((double)s->viewport.width / 1280.0);
        if (focus < 52) {
          in.pointer_x = s->viewport.x + 1200 * scale;
          in.pointer_y = s->viewport.y + 730 * scale;
          if (focus == 48 || focus == 50) {
            BkEndingControlRect rects[BK_ENDING_CONTROL_RECTS];
            VERIFY(bk_ending_ui_control_rects(&s->ui, rects));
            in.pointer_x = s->viewport.x + rects[3].x + rects[3].width * .5f;
            in.pointer_y = s->viewport.y + rects[3].y + rects[3].height * .5f;
            in.pressed = in.held = BK_BUTTON_CONFIRM;
            ++out->focused;
          } else if (focus == 49 || focus == 51) in.released = BK_BUTTON_CONFIRM;
        }
        ++focus;
      } else if (operation < 5 && !s->state->open) {
        VERIFY(secondary_gesture(s, operation == 4 ? 2 : 1, &in, e));
        in.pressed = in.held = BK_BUTTON_CONFIRM;
      }
    } else if (before == 3 && !common.blocked) {
      int wanted = operation == 4 ? 4 : (int)operation;
      unsigned choice = 0;
      while (choice < 3 && s->state->choices[choice] != wanted) ++choice;
      VERIFY(choice < 3);
      in.pointer_x = s->viewport.x + (float)s->state->points[choice][0];
      in.pointer_y = s->viewport.y + (float)s->state->points[choice][1];
      if (!menu_held) {
        in.held = BK_BUTTON_CONFIRM;
        menu_held = 1;
      } else {
        in.released = BK_BUTTON_CONFIRM;
        ++operation; ++out->menus;
        menu_held = 0;
      }
    }
    VERIFY(secondary_tick(scene, s, in, out, e));
    if (clicked_display >= 0) {
      VERIFY(s->state->control.mode_721ec4 == clicked_display && s->state->frame.phase == 2);
      unsigned passes = bk_ending_normal_render_pass_count(s->render);
      VERIFY(clicked_display == 1 ? passes == 3 :
             clicked_display == 2 ? passes == 2 : (passes == 2 || passes == 3));
      for (unsigned pass = 0; pass < passes; ++pass)
        VERIFY(bk_ending_normal_render_pass_view(s->render, pass) == (pass >= 2));
    }
    for (unsigned g = 0; g < 5; ++g)
      for (unsigned flag = 0; flag < 8; ++flag)
        if (g != group || flag != 1)
          VERIFY(s->state->working[g][flag] == unlocked[g][flag]);
    if (local_frames % 512 == 0 || scheduled.calls || clicked_display >= 0) {
      VERIFY(bk_renderer_readback(renderer, pixels, bytes, e));
      uint64_t saved = secondary_snapshot(s);
      BkRenderStats stats = bk_renderer_stats(renderer);
      VERIFY(secondary_present(scene, s, e));
      VERIFY(bk_renderer_readback(renderer, again, bytes, e));
      VERIFY(!memcmp(pixels, again, bytes) && secondary_snapshot(s) == saved);
      VERIFY(bk_renderer_stats(renderer).skin_dispatches == stats.skin_dispatches);
      ++out->redraws;
    }
    if (local_frames && local_frames % 2000 == 0) {
      printf("secondary session group%u variant%u frames%u state%d menus%u focus%u\n",
          group, variant, local_frames, s->state->frame.state_721ee4, operation, focus);
      fflush(stdout);
    }
  }
  VERIFY(local_frames < 18000 && scheduled.calls == 1 && operation == 5 && held == 12);
  /*The parent publishes49/1 before the UI transition dispatch. After the
   * scheduler returns, prepare_ui_frame imports the live common fields;
   * asserting the OLD curtain request there would reject a consumed request. */
  VERIFY(s->stopped && scheduled.request_action == 49 && scheduled.request_curtain == 1);
  VERIFY(scheduled.target == 0x18 && scheduled.mode <= 3);
  VERIFY(!common.blocked && s->state->control.variant == 255 && s->state->selected == -1);
  VERIFY(s->state->frame.curtain_wanted == common.blocked &&
         s->state->frame.transition_action == common.action);
  VERIFY(s->state->working[group][1] == 1 && controller.remaining == 15 && controller.alternate == 1);
  VERIFY(!bk_ending_normal_scene_step_at(scene, 1.0 / 60.0,
      s->wall_seconds + 1, &(BkInput){0}, e));
  out->pcm_hash = secondary_hash(out->pcm_hash, &sink.hash, sizeof(sink.hash));
  ++out->entries; ++out->schedules;
  printf("secondary session group%u variant%u complete frames%u target%u mode%u\n",
      group, variant, local_frames, scheduled.target, scheduled.mode);
  fflush(stdout);
  ok = 1;
done:
  if (!ok && s) fprintf(stderr, "phase%d state%d event%d open%d blocked%u\n",
      s->state->frame.phase, s->state->frame.state_721ee4, s->state->frame.camera_event,
      s->state->open, s->common->blocked);
  free(pixels); free(again);
  bk_scene_destroy(scene);
  bk_audio_destroy(mixer);
  return ok;
}
int main(int argc, char **argv) {
  if (argc != 2) return 2;
  char e[256] = {0}, path[1024];
  int rc = 1;
  BkRenderer *renderer = bk_renderer_create(427, 240, stdout, e);
  BkResourceStore *store = bk_resources_create(e);
  if (!renderer || !store) goto done;
  const char *packs[] = {"bk3_00", "bk3_02", "bk3_03", "bk3_04",
                         "bk3_06", "bk3_09", "bk3_18", "fambom"};
  for (unsigned i = 0; i < sizeof(packs) / sizeof(*packs); ++i) {
    if (snprintf(path, sizeof(path), "%s/%s.pp", argv[1], packs[i]) >= (int)sizeof(path) ||
        !bk_resources_mount(store, packs[i], path, e)) goto done;
  }
  BkRenderStats baseline = bk_renderer_stats(renderer);
  SecondaryResult result = {.state_hash = UINT64_C(14695981039346656037),
                             .pcm_hash = UINT64_C(14695981039346656037)};
  for (unsigned group = 0; group < 5; ++group)
    for (unsigned variant = 0; variant < 2; ++variant) {
      if (!secondary_profile(renderer, store, group, variant, &result, e)) goto done;
      BkRenderStats released = bk_renderer_stats(renderer);
      if (released.live_allocations != baseline.live_allocations ||
          released.live_bytes != baseline.live_bytes) {
        snprintf(e, sizeof(e), "secondary scene retained GPU allocations after destruction");
        goto done;
      }
    }
  if (result.entries != 10 || result.menus != 50 || result.states != 255 ||
      result.frozen != 120 || result.schedules != 10 || result.focused != 20 ||
      result.display_switches != 30 || result.dual_views < 10) {
    snprintf(e, sizeof(e), "incomplete secondary scene input coverage");
    goto done;
  }
  printf("PASS secondary sessions entries%u frames%u states%u menus%u frozen%u redraws%u "
      "schedules%u focus%u display_switches%u hidden_background%u dual_views%u state=%016llx pcm=%016llx\n",
      result.entries, result.frames, result.states, result.menus, result.frozen,
      result.redraws, result.schedules, result.focused, result.display_switches,
      result.hidden_background,
      result.dual_views, (unsigned long long)result.state_hash,
      (unsigned long long)result.pcm_hash);
  rc = 0;
done:
  if (rc) fprintf(stderr, "%s\n", e);
  bk_resources_destroy(store);
  bk_renderer_destroy(renderer);
  return rc;
}
