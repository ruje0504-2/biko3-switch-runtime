/* Natural story normal -> secondary -> normal, with actual scene input,
 * clip clocks, consumed PCM, UI and GPU snapshots. The initial story entry
 * is explicit; no progress, phase, pending action or actor source is set by
 * this probe. Picker planning copies mutable destinations before querying. */
#include "../runtime/scene/ending_normal_session.c"
#include "scene/ending_target.h"

typedef struct { uint64_t submitted, consumed, hash; int freeze; } StorySink;
typedef struct {
  unsigned entries, frames, reloads, redraws, gestures, frozen, secondary_states;
  uint64_t pcm, state;
} StoryResult;
static uint64_t story_hash(uint64_t h, const void *data, size_t n) {
  const uint8_t *p = data;
  for (size_t i = 0; i < n; ++i) h = (h ^ p[i]) * UINT64_C(1099511628211);
  return h;
}
static int story_submit(void *context, const int16_t *pcm, size_t frames, char e[256]) {
  (void)e;
  StorySink *s = context;
  s->hash = story_hash(s->hash, pcm, frames * 2 * sizeof(*pcm));
  s->submitted += frames;
  return 1;
}
static int story_poll(void *context, uint64_t *consumed, char e[256]) {
  (void)e;
  StorySink *s = context;
  if (!s->freeze) {
    uint64_t next = s->consumed + 800;
    s->consumed = next < s->submitted ? next : s->submitted;
  }
  *consumed = s->consumed;
  return 1;
}
static int story_schedule(void *context, uint8_t target, uint8_t mode, char e[256]) {
  (void)context;
  snprintf(e, 256, "story stage roundtrip unexpectedly scheduled flow%u mode%u", target, mode);
  return 0;
}
static int story_present(BkScene *scene, EndingNormalScene *s, char e[256]) {
  return bk_renderer_begin(s->services.renderer, e) &&
         bk_scene_draw(scene, &(BkSceneFrame){0}, e) &&
         bk_renderer_end(s->services.renderer, e) &&
         bk_ending_normal_scene_after_present(scene, e);
}
static BkEndingBackgroundAssets *story_background(EndingNormalScene *s) {
  return s->assets ? bk_ending_normal_assets_background(s->assets)
                   : bk_ending_secondary_assets_background(s->secondary_assets);
}
/* Map the read-only record lane by invoking its original mapping on a
 * local value; previous flow24 cannot append a story record. */
static int story_lane(int node, char e[256]) {
  int32_t lane = -1;
  return bk_ending_record_normal_choice(NULL, 0x18, node, &lane, e) ? lane : -1;
}
static int story_normal_gesture(EndingNormalScene *s, BkInput *in, char e[256]) {
  static const unsigned nodes[] = {11, 12, 9, 10, 26, 27, 1, 5};
  BkClipState clip;
  if (!bk_actor_pose_state(scene_primary(s), &clip)) return -1;
  for (unsigned i = 0; i < sizeof(nodes) / sizeof(*nodes); ++i) {
    unsigned node = nodes[i];
    int lane = story_lane((int)node, e);
    if (lane < 0 || !s->ui_present[node] ||
        (s->state->normal_processed[lane * 2] && s->state->normal_processed[lane * 2 + 1]))
      continue;
    int32_t point[2];
    if (!bk_ending_ui_project_target(&s->ui_pick, node, point, e)) return -1;
    for (int dy = -2; dy <= 2; ++dy)
      for (int dx = -2; dx <= 2; ++dx) {
        float x = (float)point[0] + dx, y = (float)point[1] + dy;
        if (x < 2 || y < 2 || x >= s->viewport.width * .9f || y >= s->viewport.height - 2)
          continue;
        BkEndingFrameState frame = s->state->frame;
        BkEndingUiSprite ring = s->ui.sprites[50];
        int32_t targets[39][2], kind = s->normal_controller->control.action_kind;
        int32_t column = s->normal_controller->control.action_column;
        memcpy(targets, s->state->targets, sizeof(targets));
        BkEndingTargetBindings b = {&frame, &s->state->control, &s->state->auxiliary,
            &clip.slot, bk_ending_normal_assets_config(s->assets)->actions, targets,
            &kind, &column, &ring, s->ui_camera_local, &s->ui_pick};
        int hit;
        if (!bk_ending_target_step(&b, (float[2]){x, y}, &hit, e)) return -1;
        int selected = story_lane(frame.camera_cached, e);
        if (hit && kind >= 0 && kind < 3 && selected >= 0 &&
            (!s->state->normal_processed[selected * 2] ||
             !s->state->normal_processed[selected * 2 + 1])) {
          *in = (BkInput){.pointer_active = 1, .pointer_x = s->viewport.x + x,
              .pointer_y = s->viewport.y + y};
          return 1;
        }
      }
  }
  return 0;
}
static int story_secondary_hit(EndingNormalScene *s, float x, float y, int wanted, char e[256]) {
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
static int story_secondary_gesture(EndingNormalScene *s, BkInput *in, char e[256]) {
  BkEndingUiPickBindings geometry = s->ui_pick;
  uint8_t present = 1;
  geometry.world = (const float (*)[16])bk_actor_forest_world(scene_forest(s),
      (uint32_t)s->state->retained.normal.word_719b40);
  geometry.present = &present;
  geometry.count = 1;
  int32_t center[2];
  if (!bk_ending_ui_project_target(&geometry, 0, center, e)) return -1;
  for (int radius = 0; radius <= 64; ++radius)
    for (int dy = -radius; dy <= radius; ++dy)
      for (int dx = -radius; dx <= radius; ++dx) {
        if (abs(dx) != radius && abs(dy) != radius) continue;
        int64_t x = (int64_t)center[0] + dx, y = (int64_t)center[1] + dy;
        if (x < 0 || y < 0 || x >= s->viewport.width || y >= s->viewport.height) continue;
        int hit = story_secondary_hit(s, (float)x, (float)y, 2, e);
        if (hit < 0) return -1;
        if (hit) {
          *in = (BkInput){.pointer_active = 1,
              .pointer_x = s->viewport.x + (float)x, .pointer_y = s->viewport.y + (float)y};
          return 1;
        }
      }
  return 0;
}
static int story_toolbar_hover(EndingNormalScene *s, BkInput *in, char e[256]) {
  float scale = (float)((double)s->viewport.width / 1280.0);
  const float *r = s->ui.sprites[49].rect;
  for (float y = fmaxf(r[1] + 2, 2); y < fminf(r[1] + r[3] - 2, s->viewport.height - 2); y += 4)
    for (float x = s->viewport.width - 2; x > 1096 * scale; x -= 4) {
      int miss = story_secondary_hit(s, x, y, 0, e);
      if (miss < 0) return 0;
      if (miss) {
        *in = (BkInput){.pointer_active = 1, .pointer_x = s->viewport.x + x,
                        .pointer_y = s->viewport.y + y};
        return 1;
      }
    }
  return fail(e, "no unoccupied sidebar hover point in the current camera");
}
#define VERIFY(x) do { if (!(x)) { \
  fprintf(stderr, "story reload group%u line%d (%s): %s\n", group, __LINE__, #x, e); \
  goto done; } } while (0)
static int story_profile(BkRenderer *renderer, BkResourceStore *store,
                         unsigned group, StoryResult *out, char e[256]) {
  int ok = 0;
  BkScene *scene = NULL;
  EndingNormalScene *s = NULL;
  BkAudio *audio = NULL;
  BkEndingRecords *records = NULL;
  uint8_t *pixels = NULL, *again = NULL;
  StorySink sink = {.hash = UINT64_C(14695981039346656037)};
  BkAudioSink output = {&sink, 48000, 480, 1920, story_submit, story_poll};
  VERIFY(audio = bk_audio_create(&output, e));
  BkSceneServices services = {store, renderer, stdout, audio, NULL, NULL};
  /*4e6dee retains transition/timer words; emulate the application's zeroed
   * process allocation before running that partial initializer.*/
  BkCommonHudState common = {0};
  bk_common_hud_initialize(&common);
  BkEndingAuxiliaryCycle cycle = bk_ending_auxiliary_cycle_initial();
  BkEndingNormalControllerRetained controller;
  bk_ending_normal_controller_initialize(&controller);
  BkEndingPresentationRetained presentation = {0};
  BkEndingSecondaryControlState secondary = bk_ending_secondary_control_initial();
  BkEndingSecondaryPresentationState secondary_display = bk_ending_secondary_presentation_initial();
  BkEndingState state;
  bk_ending_state_initialize(&state);
  BkEndingTertiaryControllerRetained tertiary;
  bk_ending_tertiary_controller_initialize(&tertiary);
  uint32_t random = 123;
  int32_t duck = 0;
  BkEndingProcess process;
  bk_ending_process_initialize(&process);
  uint8_t inventory[5] = {0};
  BkEndingNormalFlow flow = {.inventory = inventory, .process = &process, .common = &common, .schedule = story_schedule,
      .wall_seconds = 1, .auxiliary_cycle = &cycle, .random = &random,
      .normal_controller = &controller, .presentation = &presentation,
      .duck_transition = &duck, .secondary_controller = &secondary,
      .secondary_presentation = &secondary_display,
      .state = &state, .tertiary_controller = &tertiary};
  const uint8_t unlocked[5][8] = {{0}};
  VERIFY(records = calloc(1, sizeof(*records)));
  VERIFY(scene = bk_ending_normal_scene_create_story(
      &services, group, 0, records, unlocked, &flow, e));
  s = bk_scene_custom_context(scene);
  VERIFY(s && s->assets && s->state->frame.phase == 1);
  VERIFY(bk_audio_fill(audio, e) && story_present(scene, s, e));
  unsigned width, height;
  bk_renderer_extent(renderer, &width, &height);
  size_t bytes = (size_t)width * height * 4;
  VERIFY(pixels = malloc(bytes));
  VERIFY(again = malloc(bytes));
  BkEndingAudio *audio_owner = s->audio;
  unsigned reloads = 0, idle_wait = 0, preset_wait = 0, held = 0;
  unsigned settle = 0, frozen = 0, menus = 0, local_frames = 0, focus = 0, focus_wait = 0;
  unsigned local_gestures = 0;
  uint32_t previous_keys = 0;
  for (; local_frames < 50000 && settle < 120; ++local_frames) {
    BkInput in = {.pointer_active = 1, .pointer_x = s->viewport.x + 2,
                   .pointer_y = s->viewport.y + 2};
    int playing0, playing1;
    VERIFY(bk_audio_playing(audio, 0, &playing0) && bk_audio_playing(audio, 1, &playing1));
    sink.freeze = !reloads && s->state->frame.state_721ee0 == 5 &&
                  s->state->auxiliary.pending == 12 && playing0 && frozen < 24;
    if (sink.freeze) { ++frozen; ++out->frozen; }
    if (reloads == 2) {
      if (!common.blocked && !common.curtain.stage && s->state->frame.state_721ee0 == 1)
        ++settle;
    } else if (!common.blocked && s->assets) {
      int state = s->state->frame.state_721ee0;
      if (state == 3) {
        in = (BkInput){.held = held ? BK_BUTTON_CONFIRM : 0};
        if (held && (s->state->normal_ready == 1 || s->state->normal_ready == 5)) {
          int lane = s->state->normal_target;
          VERIFY(lane >= 0 && lane < 7);
          unsigned side = s->state->normal_processed[lane * 2] ? 1 : 0;
          if (s->state->normal_processed[lane * 2 + side]) {
            in.held = 0;
            in.released = BK_BUTTON_CONFIRM;
            held = 0;
          } else in.pointer_motion_x = (local_frames & 1 ? 1.f : -1.f) * (side ? 10.f : 3.f);
        }
      } else if (state == 1 && s->state->auxiliary.progress <= .49f && !playing1 && !s->state->open) {
        if (preset_wait) {
          --preset_wait;
        } else {
          int gesture = story_normal_gesture(s, &in, e);
          VERIFY(gesture >= 0);
          if (gesture) {
            in.pressed = in.held = BK_BUTTON_CONFIRM;
            held = 1;
            idle_wait = 0;
            ++out->gestures; ++local_gestures;
          } else if (++idle_wait >= 90) {
            float scale = (float)((double)s->viewport.width / 1280.0);
            in.pointer_x = s->viewport.x + 1200 * scale;
            in.pointer_y = s->viewport.y + 730 * scale;
            BkEndingControlRect rects[BK_ENDING_CONTROL_RECTS];
            VERIFY(bk_ending_ui_control_rects(&s->ui, rects));
            const BkEndingControlRect *r = &rects[0];
            if (r->x >= 0 && r->y >= 0 && r->x + r->width <= s->viewport.width &&
                r->y + r->height <= s->viewport.height) {
              in.pointer_x = s->viewport.x + r->x + r->width * .5f;
              in.pointer_y = s->viewport.y + r->y + r->height * .5f;
              in.pressed = in.held = BK_BUTTON_CONFIRM;
              preset_wait = 90;
              idle_wait = 0;
            }
          }
        }
      }
    } else if (!common.blocked && s->secondary_assets) {
      int state = s->state->frame.state_721ee4;
      if (state == 1 && !menus) {
        if (focus < 184) {
          /*The initial head focus can put the alternate below the content
           * window. Reveal the real toolbar and select its other targets.*/
          if (focus < 4) {
            VERIFY(story_toolbar_hover(s, &in, e));
            VERIFY(++focus_wait < 1200);
            if (!(focus & 1)) {
              BkEndingControlRect rects[BK_ENDING_CONTROL_RECTS];
              VERIFY(bk_ending_ui_control_rects(&s->ui, rects));
              const BkEndingControlRect *r = &rects[3];
              if (s->state->open && r->x >= 0 && r->y >= 0 &&
                  r->x + r->width <= s->viewport.width && r->y + r->height <= s->viewport.height) {
                in.pointer_x = s->viewport.x + r->x + r->width * .5f;
                in.pointer_y = s->viewport.y + r->y + r->height * .5f;
                in.pressed = in.held = BK_BUTTON_CONFIRM;
                ++focus;
              }
            } else {
              in.released = BK_BUTTON_CONFIRM;
              ++focus;
            }
          } else ++focus;
        } else if (!playing1 && !s->state->open) {
          int gesture = story_secondary_gesture(s, &in, e);
          VERIFY(gesture >= 0);
          if (gesture) in.pressed = in.held = BK_BUTTON_CONFIRM;
        }
      } else if (state == 3) {
        unsigned choice = 0;
        while (choice < 3 && s->state->choices[choice] != 4) ++choice;
        VERIFY(choice < 3);
        in.pointer_x = s->viewport.x + (float)s->state->points[choice][0];
        in.pointer_y = s->viewport.y + (float)s->state->points[choice][1];
        in.released = BK_BUTTON_CONFIRM;
        ++menus;
      }
    }
    /* A planner may retry a click that the live controller did not accept.
     * Release before another press and derive both edges from the physical
     * held state. No synthetic repeated key-down or unheld release events. */
    in.held &= ~(in.pressed & previous_keys);
    in.pressed = in.held & ~previous_keys;
    in.released = previous_keys & ~in.held;
    previous_keys = in.held;
    BkEndingNormalRender *old_render = s->render;
    BkEndingBackgroundAssets *old_background = story_background(s);
    int old_phase = s->state->frame.phase;
    VERIFY(bk_audio_poll(audio, e));
    VERIFY(bk_scene_step(scene, 1.0 / 60.0, &in, e));
    if (getenv("BK_ENDING_STORY_TRACE") && in.pressed) {
      printf("input group%u frame%u xy%g,%g state%d cached%d kind%d col%d open%d request%d\n",
          group, local_frames, s->pointer.position[0], s->pointer.position[1],
          s->state->frame.state_721ee0, s->state->frame.camera_cached,
          controller.control.action_kind, controller.control.action_column,
          s->state->open, s->state->frame.camera_request);
      fflush(stdout);
    }
    VERIFY(bk_audio_fill(audio, e) && story_present(scene, s, e));
    if (sink.freeze)
      VERIFY(s->assets && s->state->frame.state_721ee0 == 5 && s->state->auxiliary.pending == 12);
    ++out->frames;
    out->state = story_hash(out->state, &s->state->frame, sizeof(s->state->frame));
    if (s->secondary_assets && s->state->frame.state_721ee4 >= 0 && s->state->frame.state_721ee4 < 8)
      out->secondary_states |= 1u << (unsigned)s->state->frame.state_721ee4;
    if (s->render != old_render) {
      ++reloads; ++out->reloads;
      VERIFY(reloads <= 2 && s->state->frame.phase == (old_phase == 1 ? 2 : 1));
      VERIFY(s->retired.render == old_render && s->snapshot_render == old_render);
      VERIFY(s->audio == audio_owner && (story_background(s) == old_background) == (group != 1));
      VERIFY(bk_renderer_readback(renderer, pixels, bytes, e));
      BkEndingState snapshot = (*s->state);
      uint32_t saved_rng = random;
      BkAudioStats audio_stats = bk_audio_stats(audio);
      for (unsigned redraw = 0; redraw < 3; ++redraw) {
        VERIFY(story_present(scene, s, e));
        VERIFY(bk_renderer_readback(renderer, again, bytes, e));
        VERIFY(!memcmp(pixels, again, bytes) && !memcmp(&snapshot, s->state, sizeof(snapshot)));
        VERIFY(saved_rng == random && bk_audio_stats(audio).submitted == audio_stats.submitted);
        VERIFY(s->retired.render == old_render && s->snapshot_render == old_render);
        ++out->redraws;
      }
      printf("story group%u transition%u frame%u phase%d progress%g\n",
          group, reloads, local_frames, s->state->frame.phase, s->state->auxiliary.progress);
      fflush(stdout);
    }
    if (local_frames && local_frames % 2000 == 0) {
      printf("story group%u frames%u phase%d normal%d ready%d secondary%d progress%g menus%u focus%u open%d camera%d gestures%u wait%u preset%u speech%d/%d request%d\n",
          group, local_frames, s->state->frame.phase, s->state->frame.state_721ee0,
          s->state->normal_ready, s->state->frame.state_721ee4, s->state->auxiliary.progress,
          menus, focus, s->state->open, s->state->frame.camera_mode,
          local_gestures, idle_wait, preset_wait, playing0, playing1, s->state->frame.camera_request);
      fflush(stdout);
    }
  }
  VERIFY(reloads == 2 && settle == 120 && menus == 1 && frozen == 24);
  VERIFY(s->state->next_mode == 1 && s->assets && !s->secondary_assets && !s->retired.render);
  VERIFY(s->state->working[group][1] == 1);
  unsigned transitions_recorded = 0;
  const BkEndingRecord *record = &records->groups[group];
  for (int32_t i = 0; i < record->count; ++i) transitions_recorded += record->actions[i] == 10;
  VERIFY(transitions_recorded == 1);
  out->pcm = story_hash(out->pcm, &sink.hash, sizeof(sink.hash));
  ++out->entries;
  printf("story group%u PASS frames%u record%d pcm=%016llx\n", group, local_frames,
      record->count, (unsigned long long)sink.hash);
  fflush(stdout);
  ok = 1;
done:
  if (!ok && s) fprintf(stderr, "phase%d normal%d ready%d secondary%d pending%d progress%g next%d\n",
      s->state->frame.phase, s->state->frame.state_721ee0, s->state->normal_ready,
      s->state->frame.state_721ee4, s->state->auxiliary.pending, s->state->auxiliary.progress, s->state->next_mode);
  free(pixels); free(again);
  bk_scene_destroy(scene);
  bk_audio_destroy(audio);
  free(records);
  return ok;
}
int main(int argc, char **argv) {
  if (argc < 2 || argc > 3 || (argc == 3 && (strlen(argv[2]) != 1 || argv[2][0] < '0' || argv[2][0] > '4')))
    return 2;
  char e[256] = {0}, path[1024];
  int rc = 1;
  BkRenderer *renderer = bk_renderer_create(427, 240, stdout, e);
  BkResourceStore *store = bk_resources_create(e);
  if (!renderer || !store) goto done;
  const char *packs[] = {"bk3_00", "bk3_02", "bk3_03", "bk3_04", "bk3_06",
                         "bk3_08", "bk3_09", "bk3_18", "fambom"};
  for (unsigned i = 0; i < sizeof(packs) / sizeof(*packs); ++i)
    if (snprintf(path, sizeof(path), "%s/%s.pp", argv[1], packs[i]) >= (int)sizeof(path) ||
        !bk_resources_mount(store, packs[i], path, e)) goto done;
  BkRenderStats baseline = bk_renderer_stats(renderer);
  StoryResult result = {.pcm = UINT64_C(14695981039346656037),
                         .state = UINT64_C(14695981039346656037)};
  unsigned first = argc == 3 ? (unsigned)(argv[2][0] - '0') : 0;
  unsigned last = argc == 3 ? first + 1 : 5;
  for (unsigned group = first; group < last; ++group) {
    if (!story_profile(renderer, store, group, &result, e)) goto done;
    BkRenderStats live = bk_renderer_stats(renderer);
    if (live.live_allocations != baseline.live_allocations || live.live_bytes != baseline.live_bytes) {
      snprintf(e, sizeof(e), "story roundtrip retained GPU allocations after destruction");
      goto done;
    }
  }
  printf("PASS natural story reload entries%u frames%u reloads%u redraws%u gestures%u frozen%u states%u state=%016llx pcm=%016llx\n",
      result.entries, result.frames, result.reloads, result.redraws, result.gestures, result.frozen,
      result.secondary_states, (unsigned long long)result.state, (unsigned long long)result.pcm);
  rc = 0;
done:
  if (rc) fprintf(stderr, "%s\n", e);
  bk_resources_destroy(store);
  bk_renderer_destroy(renderer);
  return rc;
}
