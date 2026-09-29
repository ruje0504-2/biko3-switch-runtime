/*Actual third story entry, controller, UI, PCM and Vulkan presentation.
 * Gestures only modify input; copied picker destinations are used to plan
 * them. Stop at the native request for the next loader: that loader and a
 * complete story/ending are not claimed. Cross-entry retirement below is an
 * explicit lifecycle fixture, not a natural third-to-normal transition. */
#include "../runtime/scene/ending_normal_session.c"
#include "scene/ending_target.h"

typedef struct { uint64_t submitted, consumed, hash; int freeze; } ThirdSink;
typedef struct {
  unsigned entries, frames, menus, drags, zooms, frozen, states, redraws, retired;
  uint64_t pcm, state;
} ThirdResult;
static uint64_t third_hash(uint64_t h, const void *data, size_t size) {
  const uint8_t *p = data;
  for (size_t i = 0; i < size; ++i) h = (h ^ p[i]) * UINT64_C(1099511628211);
  return h;
}
static int third_submit(void *context, const int16_t *pcm, size_t frames, char e[256]) {
  (void)e;
  ThirdSink *s = context;
  s->hash = third_hash(s->hash, pcm, frames * 2 * sizeof(*pcm));
  s->submitted += frames;
  return 1;
}
static int third_poll(void *context, uint64_t *consumed, char e[256]) {
  (void)e;
  ThirdSink *s = context;
  if (!s->freeze) {
    uint64_t next = s->consumed + 800;
    s->consumed = next < s->submitted ? next : s->submitted;
  }
  *consumed = s->consumed;
  return 1;
}
static int third_schedule(void *context, uint8_t target, uint8_t mode, char e[256]) {
  (void)context;
  snprintf(e, 256, "third probe unexpectedly scheduled flow%u mode%u", target, mode);
  return 0;
}
static int third_present(BkScene *scene, BkRenderer *renderer, char e[256]) {
  return bk_renderer_begin(renderer, e) && bk_scene_draw(scene, &(BkSceneFrame){0}, e) &&
         bk_renderer_end(renderer, e) && bk_ending_normal_scene_after_present(scene, e);
}
static uint64_t third_snapshot(EndingNormalScene *s) {
  uint64_t h = third_hash(UINT64_C(14695981039346656037), s->state, sizeof(*s->state));
  h = third_hash(h, &s->camera, sizeof(s->camera));
  h = third_hash(h, &s->ui, sizeof(s->ui));
  h = third_hash(h, s->common, sizeof(*s->common));
  h = third_hash(h, s->tertiary_controller, sizeof(*s->tertiary_controller));
  h = third_hash(h, s->normal_controller, sizeof(*s->normal_controller));
  h = third_hash(h, s->presentation, sizeof(*s->presentation));
  h = third_hash(h, s->random, sizeof(*s->random));
  return third_hash(h, &s->wall_seconds, sizeof(s->wall_seconds));
}
static int third_hit(EndingNormalScene *s, int wanted, int x, int y, char e[256]) {
  BkEndingFrameState frame = s->state->frame;
  BkEndingControlState control = s->state->control;
  BkEndingAuxiliaryState auxiliary = s->state->auxiliary;
  BkClipState clip;
  if (!bk_actor_pose_state(scene_primary(s), &clip)) return -1;
  int32_t targets[39][2], kind = s->normal_controller->control.action_kind;
  int32_t column = s->normal_controller->control.action_column;
  memcpy(targets, s->state->targets, sizeof(targets));
  BkEndingUiSprite ring = s->ui.sprites[50];
  BkEndingTargetBindings b = {&frame, &control, &auxiliary, &clip.slot,
      bk_ending_tertiary_assets_config(s->tertiary_assets)->actions, targets,
      &kind, &column, &ring, s->ui_camera_local, &s->ui_pick};
  int hit;
  if (!bk_ending_target_step(&b, (float[2]){(float)x, (float)y}, &hit, e)) return -1;
  return hit == 1 && frame.camera_cached == wanted;
}
static int third_gesture(EndingNormalScene *s, int wanted, BkInput *input, char e[256]) {
  int32_t center[2];
  if (wanted < 0 || wanted >= 39 ||
      !bk_ending_ui_project_target(&s->ui_pick, (unsigned)wanted, center, e)) return -1;
  if (center[0] < 0 || center[1] < 0 || (uint32_t)center[0] > s->viewport.width ||
      (uint32_t)center[1] > s->viewport.height) return 0;
  for (int radius = 0; radius <= 32; ++radius)
    for (int dy = -radius; dy <= radius; ++dy)
      for (int dx = -radius; dx <= radius; ++dx) {
        if (abs(dx) != radius && abs(dy) != radius) continue;
        int x = center[0] + dx, y = center[1] + dy;
        if (x < 0 || y < 0 || (uint32_t)x >= s->viewport.width ||
            (uint32_t)y >= s->viewport.height) continue;
        int hit = third_hit(s, wanted, x, y, e);
        if (hit < 0) return -1;
        if (hit) {
          input->pointer_active = 1;
          input->pointer_x = (float)s->viewport.x + x;
          input->pointer_y = (float)s->viewport.y + y;
          return 1;
        }
      }
  return 0;
}
#define VERIFY(x) do { if (!(x)) { \
  fprintf(stderr, "third session group%u line%d (%s): %s\n", group, __LINE__, #x, e); \
  goto done; } } while (0)
static int third_profile(BkRenderer *renderer, BkResourceStore *store, unsigned group,
                         ThirdResult *out, char e[256]) {
  int ok = 0;
  BkScene *scene = NULL, *next = NULL;
  BkAudio *audio = NULL;
  BkEndingState *state = calloc(1, sizeof(*state));
  BkEndingRecords *records = calloc(1, sizeof(*records));
  uint8_t *pixels = NULL, *again = NULL;
  ThirdSink sink = {.hash = UINT64_C(14695981039346656037)};
  BkAudioSink output = {&sink, 48000, 480, 1920, third_submit, third_poll};
  VERIFY(state && records && (audio = bk_audio_create(&output, e)));
  BkSceneServices services = {store, renderer, stdout, audio, NULL};
  BkCommonHudState common = {0};
  bk_common_hud_initialize(&common);
  BkEndingAuxiliaryCycle cycle = bk_ending_auxiliary_cycle_initial();
  BkEndingNormalControllerRetained normal;
  bk_ending_normal_controller_initialize(&normal);
  BkEndingPresentationRetained presentation = {0};
  BkEndingSecondaryControlState secondary = bk_ending_secondary_control_initial();
  BkEndingSecondaryPresentationState secondary_display = bk_ending_secondary_presentation_initial();
  BkEndingTertiaryControllerRetained tertiary;
  bk_ending_tertiary_controller_initialize(&tertiary);
  bk_ending_state_initialize(state);
  uint32_t random = 123 + group;
  int32_t duck = 0;
  BkEndingNormalFlow flow = {.common = &common, .schedule = third_schedule,
      .wall_seconds = 1, .auxiliary_cycle = &cycle, .random = &random,
      .normal_controller = &normal, .presentation = &presentation, .duck_transition = &duck,
      .secondary_controller = &secondary, .secondary_presentation = &secondary_display,
      .state = state, .tertiary_controller = &tertiary};
  uint8_t unlocked[5][8] = {{0}};
  VERIFY(scene = bk_ending_normal_scene_create_story(&services, group, 1, records,
                                                      unlocked, &flow, e));
  EndingNormalScene *s = bk_scene_custom_context(scene);
  VERIFY(s->state == state && s->tertiary_assets && !s->assets && !s->secondary_assets &&
         state->frame.phase == 3 && state->control.variant == 5 &&
         s->disabled_count == (group == 2 ? 5u : 0u));
  VERIFY(third_present(scene, renderer, e));
  unsigned width, height;
  bk_renderer_extent(renderer, &width, &height);
  size_t pixel_bytes = (size_t)width * height * 4;
  VERIFY((pixels = malloc(pixel_bytes)) && (again = malloc(pixel_bytes)));
  unsigned frame = 0, operations = 0, drag_frames = 0, zooms = 0, frozen = 0;
  for (; frame < 20000 && !state->frame.curtain_wanted; ++frame) {
    BkClipState clip;
    VERIFY(bk_actor_pose_state(scene_primary(s), &clip));
    int playing;
    VERIFY(bk_audio_playing(audio, 1, &playing));
    BkEndingRetainedStage2 *retained = &state->retained.stage2;
    sink.freeze = state->stage3_state == 4 && retained->byte_6afd18 == 2 && frozen < 8;
    if (sink.freeze) { ++frozen; ++out->frozen; }
    BkInput input = {.pointer_active = 1,
        .pointer_x = (float)s->viewport.x + 3, .pointer_y = (float)s->viewport.y + 3};
    if (frame == 1) input.pressed = input.held = BK_BUTTON_CONFIRM;
    if (state->stage3_state == 1 && !playing && (clip.slot == 1 || clip.slot == 4)) {
      const BkEndingTertiaryConfig *config = bk_ending_tertiary_assets_config(s->tertiary_assets);
      int wanted = clip.slot == 1 ? bk_ending_tertiary_initial_targets()[group]
          : operations < 2 ? config->actions[5] : !state->unavailable[0] ? config->actions[10]
          : !state->unavailable[1] ? config->actions[15] : 6;
      int found = third_gesture(s, wanted, &input, e);
      VERIFY(found >= 0);
      if (found) input.pressed = input.held = BK_BUTTON_CONFIRM;
      else {
        VERIFY(state->frame.camera_mode == 0 && s->camera.radius < 120 && zooms < 120);
        input.look_x = 60;
        input.held = BK_BUTTON_BACK;
        ++zooms; ++out->zooms;
      }
    } else if (state->stage3_state == 3) {
      input.pointer_x = (float)s->viewport.x + state->points[0][0];
      input.pointer_y = (float)s->viewport.y + state->points[0][1];
      if (clip.slot == 7 || clip.slot == 9) {
        /* Alternate real cursor drags, keeping both endpoints on-screen. */
        float sign = drag_frames & 1 ? -1.f : 1.f;
        input.pointer_x += sign * 12;
        input.pointer_y -= sign * 8;
        ++drag_frames; ++out->drags;
        input.held = BK_BUTTON_CONFIRM;
        if (state->ui_controller.hints.movement_ready) {
          input.held = 0; input.released = BK_BUTTON_CONFIRM;
          ++operations; ++out->menus;
        }
      } else {
        input.released = BK_BUTTON_CONFIRM;
        ++operations; ++out->menus;
      }
    }
    if (state->stage3_state >= 0 && state->stage3_state < 6)
      out->states |= 1u << (unsigned)state->stage3_state;
    VERIFY(bk_audio_poll(audio, e) && bk_scene_step(scene, 1.0 / 60.0, &input, e) &&
           bk_audio_fill(audio, e) && third_present(scene, renderer, e));
    out->state = third_hash(out->state, state, sizeof(*state));
    ++out->frames;
    if (frame % 512 == 0) {
      uint64_t held = third_snapshot(s);
      BkAudioStats queued = bk_audio_stats(audio);
      VERIFY(bk_renderer_readback(renderer, pixels, pixel_bytes, e));
      VERIFY(third_present(scene, renderer, e));
      VERIFY(bk_renderer_readback(renderer, again, pixel_bytes, e));
      VERIFY(!memcmp(pixels, again, pixel_bytes) && third_snapshot(s) == held);
      VERIFY(bk_audio_stats(audio).submitted == queued.submitted &&
             bk_audio_stats(audio).consumed == queued.consumed);
      ++out->redraws;
    }
    if (frame && frame % 2000 == 0) {
      printf("third session progress group%u frame%u state%d clip%d progress%g menus%u zoom%u\n",
          group, frame, state->stage3_state, clip.slot, state->auxiliary.progress, operations, zooms);
      fflush(stdout);
    }
  }
  VERIFY(frame < 20000 && state->frame.transition_action == 7 &&
         state->auxiliary.progress >= .39f && operations >= 4 && drag_frames && frozen == 8);
  VERIFY(records->groups[group].count >= 4);
  /*Explicit outer-entry boundary: logically stop the third scene, prepare
   * the next entry, then redraw/destroy the old immutable snapshot. */
  VERIFY(bk_renderer_readback(renderer, pixels, pixel_bytes, e));
  VERIFY(bk_ending_normal_scene_stop(scene, e));
  BkEndingTertiaryControllerRetained held_controller = tertiary;
  bk_common_hud_entry_reset(&common);
  flow.wall_seconds = s->wall_seconds;
  VERIFY(next = bk_ending_normal_scene_create_story(&services, group, 0, records,
                                                     unlocked, &flow, e));
  EndingNormalScene *next_owner = bk_scene_custom_context(next);
  uint64_t held = third_snapshot(next_owner);
  VERIFY(!memcmp(&tertiary, &held_controller, sizeof(tertiary)));
  VERIFY(third_present(scene, renderer, e));
  VERIFY(bk_renderer_readback(renderer, again, pixel_bytes, e));
  VERIFY(!memcmp(pixels, again, pixel_bytes) && third_snapshot(next_owner) == held);
  bk_scene_destroy(scene); scene = NULL;
  VERIFY(third_snapshot(next_owner) == held && state->retained.normal.follow_target != 0);
  VERIFY(third_present(next, renderer, e));
  VERIFY(bk_scene_step(next, 1.0 / 60.0, &(BkInput){0}, e) && third_present(next, renderer, e));
  ++out->retired; ++out->entries;
  out->pcm = third_hash(out->pcm, &sink.hash, sizeof(sink.hash));
  printf("third session group%u frames%u menus%u drag%u zoom%u request7, old-snapshot retirement PASS\n",
      group, frame, operations, drag_frames, zooms);
  fflush(stdout);
  ok = 1;
done:
  bk_scene_destroy(scene); bk_scene_destroy(next);
  bk_audio_destroy(audio); free(records); free(state); free(pixels); free(again);
  return ok;
}
int main(int argc, char **argv) {
  if (argc < 2 || argc > 3 || (argc == 3 &&
      (strlen(argv[2]) != 1 || argv[2][0] < '0' || argv[2][0] > '4'))) return 2;
  char e[256] = {0}, path[1024];
  int rc = 1;
  BkRenderer *renderer = bk_renderer_create(427, 240, stdout, e);
  BkResourceStore *store = bk_resources_create(e);
  if (!renderer || !store) goto done;
  const char *packs[] = {"bk3_00", "bk3_02", "bk3_03", "bk3_04", "bk3_06",
      "bk3_08", "bk3_09", "bk3_11", "bk3_18", "fambom"};
  for (unsigned i = 0; i < sizeof(packs) / sizeof(*packs); ++i)
    if (snprintf(path, sizeof(path), "%s/%s.pp", argv[1], packs[i]) >= (int)sizeof(path) ||
        !bk_resources_mount(store, packs[i], path, e)) goto done;
  BkRenderStats baseline = bk_renderer_stats(renderer);
  ThirdResult result = {.state = UINT64_C(14695981039346656037), .pcm = UINT64_C(14695981039346656037)};
  unsigned first = argc == 3 ? (unsigned)(argv[2][0] - '0') : 0;
  unsigned last = argc == 3 ? first + 1 : 5;
  for (unsigned group = first; group < last; ++group) {
    if (!third_profile(renderer, store, group, &result, e)) goto done;
    BkRenderStats live = bk_renderer_stats(renderer);
    if (live.live_allocations != baseline.live_allocations || live.live_bytes != baseline.live_bytes) {
      snprintf(e, sizeof(e), "third scene retirement left GPU allocations"); goto done;
    }
  }
  if ((result.states & 59u) != 59u) { snprintf(e, sizeof(e), "missing third states: %u", result.states); goto done; }
  printf("PASS third session entries%u frames%u menus%u drags%u zooms%u frozen%u states%u redraws%u retired%u state=%016llx pcm=%016llx\n",
      result.entries, result.frames, result.menus, result.drags, result.zooms, result.frozen,
      result.states, result.redraws, result.retired, (unsigned long long)result.state, (unsigned long long)result.pcm);
  rc = 0;
done:
  if (rc) fprintf(stderr, "%s\n", e);
  bk_resources_destroy(store); bk_renderer_destroy(renderer);
  return rc;
}
