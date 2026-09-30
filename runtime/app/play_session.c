#include "app/play_session.h"
#include "app/front_end.h"
#include "app/capture_output.h"
#include "app/game_preview.h"
#include "app/pause_preview.h"
#include "app/retry_preview.h"
#include "app/save_preview.h"
#include "scene/curtain_render.h"
#include "scene/ending_normal_session.h"
#include "scene/flow_loading_render.h"
#include "scene/outcome_audio.h"
#include "scene/system_audio.h"
#include "platform/platform.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
typedef struct {
  BkSceneServices services;
  BkGameFrameState game_state;
  BkEntryProgress progress;
  BkCommonHudState common;
  BkFlowTransition flow;
  BkPauseState pause_state;
  BkRetryState retry_state;
  BkCheckpointPrompt checkpoint_state;
  BkSavePreviewState save_state;
  BkCheckpointFiles *save_files;
  uint8_t inventory_tail[3];
  BkMenuCursor cursor;
  uint8_t overlay, hover;
  BkFailureHudState failure;
  int failure_active;
  BkFlowLoadingState loading;
  BkCurtainRender *curtain;
  BkFlowLoadingRender *loading_render;
  BkOutcomeAudio *outcome;
  BkSystemAudio *confirm;
  BkFrontEnd *front;
  BkCaptureOutput capture_output;
  BkScreenshot *screenshot;
  BkEndingRecords ending_records;
  BkRecordFile *record_file;
  int require_record_file;
  BkEndingAuxiliaryCycle ending_auxiliary_cycle;
  BkEndingNormalControllerRetained ending_normal_controller;
  BkEndingPresentationRetained ending_presentation;
  BkEndingSecondaryControlState ending_secondary_controller;
  BkEndingSecondaryPresentationState ending_secondary_presentation;
  BkEndingState ending_state;
  BkEndingProcess ending_process;
  BkMenuCamera menu_camera;
  BkEndingCameraTransitions camera_transitions;
  BkSpecialProcess special_process;
  void *special_clock_context;
  int (*special_clock_read)(void *, int, uint32_t *, char[256]);
  BkEndingTertiaryControllerRetained ending_tertiary_controller;
  int32_t ending_duck_transition; /*719c5c: process zero-init, audio owns writes*/
  uint8_t ending_unlock_flags[BK_UNLOCK_FLAGS];
  unsigned ending_unlock_group;
  int ending_unlock_valid;
  int ending_first_present;
  BkScene *game, *pause, *title, *retry, *checkpoint, *save, *ending;
  BkViewport viewport;
  BkCommonHudFrame common_frame;
  double elapsed;
  float pending_seconds;
  uint8_t shown;
  int pending, drawn, retire_game, retire_pause, retire_title, retire_retry,
      retire_checkpoint, retire_save, retire_ending;
} PlaySession;
static int platform_clock(void *p, int timer, uint32_t *out, char e[256]) {
  (void)p; return bk_platform_runtime_clock(timer, out, e);
}
static int special_clock(void *p, int timer, uint32_t *out, char e[256]) {
  PlaySession *s = p;
  return s->special_clock_read(s->special_clock_context, timer, out, e);
}
static int special_release_speech(void *p, char e[256]) {
  PlaySession *s = p;
  int32_t volume, pan;
  int loaded = bk_audio_get_gain(s->services.audio, 61, &volume, &pan);
  if (!bk_audio_clear(s->services.audio, 61, e)) return 0;
  /*50da5f clears the name only when a shared speech buffer existed.*/
  if (loaded) memset(s->ending_state.speech_names[0], 0,
                       sizeof(s->ending_state.speech_names[0]));
  return 1;
}
static void log_line(PlaySession *s, const char *message) {
  if (s->services.log) {
    BkRenderStats r = bk_renderer_stats(s->services.renderer);
    fprintf(s->services.log,
            "%s [flow%02x prev%02x group%u area%u outcome%u; GPU alloc%llu "
            "peak%llu bytes%llu peak%llu]\n",
            message, s->flow.current, s->flow.previous, s->game_state.group,
            s->game_state.area, s->game_state.interaction.outcome,
            (unsigned long long)r.live_allocations,
            (unsigned long long)r.peak_allocations,
            (unsigned long long)r.live_bytes, (unsigned long long)r.peak_bytes);
    fflush(s->services.log);
  }
}
/* Borrow the one process owner; neither save nor game depends on the other. */
static void record_views(PlaySession *s, BkRecordView v[BK_RECORD_GROUPS]) {
  _Static_assert(BK_RECORD_GROUPS == (int)BK_ENDING_RECORD_GROUPS &&
      BK_RECORD_CAPACITY == (int)BK_ENDING_RECORD_CAPACITY, "record dimensions");
  for (unsigned g = 0; g < BK_RECORD_GROUPS; ++g) {
    BkEndingRecord *r = &s->ending_records.groups[g];
    v[g] = (BkRecordView){{r->retained[0], r->retained[1]}, r->actions, &r->count};
  }
}
static int release(void *context, uint8_t flow, char error[256]) {
  PlaySession *s = context;
  log_line(s, "Flow release begin");
  if (s->front && (flow == 1 || flow == 0x38 || flow == 8 || flow == 0x18 || flow == 0x48))
    return bk_front_end_stop(s->front, flow, error);
  if (flow == 0x40 && s->failure_active && s->game) {
    if (!bk_game_preview_release_failure_audio(s->game, error))
      return 0;
    s->failure_active = 0;
    return 1;
  }
  if (flow == 2 && s->game) {
    if (!s->retire_game) {
      /* Logical release immediately silences game-owned voices. Meshes stay
       * alive until the already prepared last snapshot has been presented. */
      for (unsigned voice = 0; voice <= 38; ++voice)
        if (!bk_audio_clear(s->services.audio, voice, error))
          return 0;
      s->retire_game = 1;
    }
    return 1;
  }
  if (flow == 4 && s->pause) {
    s->retire_pause = 1;
    return 1;
  }
  if (flow == 0x20 && s->checkpoint && s->game && !s->retire_checkpoint) {
    if (!bk_game_preview_advance_area(s->game, s->flow.previous, error))
      return 0;
    bk_common_hud_entry_reset(&s->common);
    s->retire_checkpoint = 1;
    log_line(s, "Area advanced with retained actors");
    return 1;
  }
  if (flow == 0x68 && s->retry) {
    s->retire_retry = 1;
    return 1;
  }
  if (flow == 0x28 && s->save) {
    s->retire_save = 1;
    return 1;
  }
  if (flow == 0x10 && s->ending) {
    const BkEndingState *ending_state =
        bk_ending_normal_scene_state(s->ending);
    if (!ending_state || ending_state->frame.group >= BK_UNLOCK_GROUPS) {
      snprintf(error, 256, "play session: ending unlock row is unavailable");
      return 0;
    }
    /*4EB8FF writes Gray before4CEB27 stops/releases the ending owner.
     * Failure preserves the current live scene and the old disk snapshot. */
    if (s->require_record_file && !s->record_file) {
      snprintf(error, 256, "play session: ending record storage unavailable");
      return 0;
    }
    if (s->record_file) {
      BkRecordView v[BK_RECORD_GROUPS]; record_views(s, v);
      if (!bk_record_file_store(s->record_file, v, error)) return 0;
    }
    if (!bk_ending_normal_scene_stop(s->ending, error))
      return 0;
    memcpy(s->ending_unlock_flags,
           ending_state->working[ending_state->frame.group],
           sizeof(s->ending_unlock_flags));
    s->ending_unlock_group = ending_state->frame.group;
    s->ending_unlock_valid = 1;
    s->retire_ending = 1;
    return 1;
  }
  if (flow == 1 && s->title) {
    s->retire_title = 1;
    return 1;
  }
  snprintf(error, 256, "play session: cannot release unowned flow%u", flow);
  return 0;
}
static void collect_retired(PlaySession *s) {
  bk_front_end_collect(s->front);
  int any = s->retire_save || s->retire_game || s->retire_pause ||
            s->retire_checkpoint || s->retire_retry || s->retire_title ||
            s->retire_ending;
  if (any)
    log_line(s, "Retired resources collection begin");
  if (s->retire_save) {
    bk_scene_destroy(s->save);
    s->save = NULL;
    s->retire_save = 0;
  }
  if (s->retire_game) {
    bk_scene_destroy(s->game);
    s->game = NULL;
    bk_outcome_audio_destroy(s->outcome);
    s->outcome = NULL;
    s->retire_game = 0;
  }
  if (s->retire_pause) {
    bk_scene_destroy(s->pause);
    s->pause = NULL;
    s->retire_pause = 0;
  }
  if (s->retire_checkpoint) {
    bk_scene_destroy(s->checkpoint);
    s->checkpoint = NULL;
    s->retire_checkpoint = 0;
  }
  if (s->retire_retry) {
    bk_scene_destroy(s->retry);
    s->retry = NULL;
    s->retire_retry = 0;
  }
  if (s->retire_title) {
    bk_scene_destroy(s->title);
    s->title = NULL;
    s->retire_title = 0;
  }
  if (s->retire_ending) {
    bk_scene_destroy(s->ending);
    s->ending = NULL;
    s->retire_ending = 0;
    s->ending_first_present = 0;
  }
  if (any)
    log_line(s, "Retired resources collection complete");
}
static int schedule(void *context, uint8_t target, uint8_t mode,
                    char error[256]) {
  PlaySession *s = context;
  BkFlowTransitionOps ops = {s, release};
  return bk_flow_transition_schedule(&s->flow, &ops, target, mode, error);
}
static int load_pause(void *context, char error[256]) {
  PlaySession *s = context;
  if (s->pause || !s->game || s->retire_game) {
    snprintf(error, 256, "play session: invalid pause ownership");
    return 0;
  }
  BkPauseBindings bindings = {&s->common, &s->flow, &s->cursor, &s->overlay,
                              &s->hover};
  BkFlowTransitionOps ops = {s, release};
  s->pause = bk_pause_preview_create_shared(&s->services, &s->pause_state,
                                            &bindings, &ops, s->elapsed, error);
  return s->pause != NULL;
}
static int play_outcome(void *context, char error[256]) {
  return bk_outcome_audio_play_outcome(((PlaySession *)context)->outcome,
                                       error);
}
static int play_response(void *context, char error[256]) {
  return bk_outcome_audio_play_response(((PlaySession *)context)->outcome,
                                        error);
}
static int confirm(void *context, char error[256]) {
  return bk_system_audio_restart(((PlaySession *)context)->confirm, error);
}
static int resume_pause(void *context, char error[256]) {
  PlaySession *s = context;
  return bk_game_preview_restore_item_text(s->game, error);
}
static int load_target(void *context, uint8_t target, char error[256]) {
  PlaySession *s = context;
  char message[64];
  snprintf(message, sizeof(message), "Flow load target%02x begin", target);
  log_line(s, message);
  if (target == 0x58 && !s->game && !s->pause && !s->title && !s->retry &&
      !s->checkpoint && !s->save && !s->ending &&
      !bk_front_end_active(s->front)) {
    /* Native4e7671 has no resources for58;466448 exits the outer loop on
     * this byte. Actual process teardown is owned by application.c. */
    return 1;
  }
  if (s->front && (target == 1 || target == 0x38 || target == 8 || target == 0x18 || target == 0x48)) {
    if (!bk_front_end_load(s->front, target, s->flow.previous,
                           s->game_state.interaction.response, s->elapsed,
                           s->pending_seconds, error))
      return 0;
    log_line(s, "Original menu scene loaded");
    return 1;
  }
  if (target == 0x28 && !s->save) {
    BkPauseBindings bindings = {&s->common, &s->flow, &s->cursor, &s->overlay,
                                &s->hover};
    BkSavePreviewGame game = {
        &s->game_state.group, &s->game_state.area, &s->game_state.random,
        s->game_state.pickup.collected, s->inventory_tail};
    BkSavePreviewOps ops = {s, release, resume_pause};
    s->save =
        bk_save_preview_create(&s->services, s->save_files, &s->save_state,
                               &bindings, &game, &ops, s->elapsed, error);
    if (!s->save)
      return 0;
    log_line(s, "Checkpoint menu loaded with writable port storage");
    return 1;
  }
  if (target == 4)
    return load_pause(s, error);
  if (target == 0x10 && !s->ending) {
    BkUnlockTable unlocked;
    if (!s->front || !bk_front_end_unlocks(s->front, &unlocked, error))
      return 0;
    s->ending_unlock_valid = 0;
    BkEndingNormalFlow ending_flow = {.common = &s->common,
        .inventory = s->game_state.pickup.collected,
                                      .context = s,
                                      .schedule = schedule,
                                      .wall_seconds = s->elapsed,
                                      .auxiliary_cycle = &s->ending_auxiliary_cycle,
                                      .random = &s->game_state.random,
                                      .normal_controller = &s->ending_normal_controller,
                                      .presentation = &s->ending_presentation,
                                      .duck_transition = &s->ending_duck_transition,
                                      .secondary_controller = &s->ending_secondary_controller,
                                      .secondary_presentation = &s->ending_secondary_presentation,
                                      .state = &s->ending_state,
                                      .process = &s->ending_process,
                                      .camera = &s->menu_camera,
                                      .camera_transitions = &s->camera_transitions,
                                      .tertiary_controller = &s->ending_tertiary_controller};
    if (s->flow.previous == 0x18) {
      BkGalleryMenuSelection result;
      if (!bk_front_end_gallery_result(s->front, &result, error))
        return 0;
      s->ending = bk_ending_scene_create_gallery(
          &s->services, (unsigned)result.group, (unsigned)result.variant,
          (unsigned)result.selection, &s->ending_records, unlocked.flags,
          &ending_flow, error);
    } else {
      BkDialogueResult result;
      if (!bk_front_end_result(s->front, &result, error))
        return 0;
      if (result.group < 0 || result.group >= BK_ENDING_RECORD_GROUPS ||
          (result.kind != 0 && result.kind != 1)) {
        snprintf(error, 256,
                 "play session: flow16 dialogue result is outside implemented entries");
        return 0;
      }
      s->ending = bk_ending_normal_scene_create_story(
          &s->services, (unsigned)result.group, (unsigned)result.kind,
          &s->ending_records, unlocked.flags, &ending_flow, error);
    }
    if (!s->ending)
      return 0;
    s->ending_first_present = 1;
    log_line(s, s->flow.previous == 0x18 ? "Gallery ending loaded for flow16"
                                       : "Story ending loaded for flow16");
    return 1;
  }
  if (target == 0x40 && s->game && !s->retire_game && !s->failure_active) {
    if (!bk_game_preview_load_failure(s->game, error))
      return 0;
    s->failure_active = 1;
    log_line(s, "Failure scene loaded with retained entry");
    return 1;
  }
  if (target == 0x20 && s->game && !s->retire_game && !s->checkpoint) {
    if (!bk_game_preview_suspend_area(s->game, error))
      return 0;
    BkPauseBindings bindings = {&s->common, &s->flow, &s->cursor, &s->overlay,
                                &s->hover};
    BkFlowTransitionOps ops = {s, release};
    s->checkpoint = bk_checkpoint_preview_create(
        &s->services, &s->checkpoint_state, &bindings, &ops,
        &s->game_state.camera.phase, bk_game_preview_hud_reserve(s->game),
        s->elapsed, error);
    if (!s->checkpoint)
      return 0;
    log_line(s, "Area save prompt loaded with retained actors");
    return 1;
  }
  if (target == 0x68 && !s->game && !s->retry) {
    BkPauseBindings bindings = {&s->common, &s->flow, &s->cursor, &s->overlay,
                                &s->hover};
    BkFlowTransitionOps ops = {s, release};
    s->retry = bk_retry_preview_create(&s->services, &s->retry_state, &bindings,
                                       &ops, s->elapsed, error);
    if (!s->retry)
      return 0;
    log_line(s, "Retry menu loaded after failure");
    return 1;
  }
  if (target == 1 && !s->title) {
    s->title = bk_scene_create(BK_SCENE_TITLE_PREVIEW, &s->services, error);
    if (!s->title)
      return 0;
    log_line(s, "Scene switched: title (retained process state)");
    return 1;
  }
  if (target == 2 && !s->game) {
    /* The explicit development factory still supports its direct entry.
     * The normal application reaches here from the real dialogue flow8. */
    uint8_t previous =
        !s->front && s->flow.previous == 1 ? 8 : s->flow.previous;
    /*4bf268 clears all eight native bytes on a new entry; the five active
     * item bytes are reset by the actor layer, and these three remain here. */
    if (previous == 8 || previous == 0x38)
      memset(s->inventory_tail, 0, sizeof(s->inventory_tail));
    s->game = bk_game_preview_create_entry(
        &s->services, &s->game_state, &s->progress, s->game_state.group,
        s->game_state.area, previous, s->elapsed, s->screenshot, error);
    if (!s->game)
      return 0;
    s->outcome = bk_outcome_audio_create(
        s->services.resources, s->services.audio, 37, 38, -900, error);
    if (!s->outcome)
      return 0;
    bk_common_hud_entry_reset(&s->common);
    return 1;
  }
  snprintf(error, 256, "play session: target flow0x%02x has no resource loader",
           target);
  return 0;
}
static int prepare_common(PlaySession *s, float seconds, char error[256]) {
  /* Pure preview of the first native tail operation. The live step runs
   * once after presentation, when its pause loader can read this frame's
   * capture. No timer, request, audio or ownership side effect is previewed. */
  BkFadeSprite preview = s->common.curtain;
  if (!bk_fade_sprite_advance(&preview, seconds)) {
    snprintf(error, 256, "play session: invalid curtain state");
    return 0;
  }
  s->common_frame.curtain_alpha = preview.alpha;
  return bk_curtain_render_prepare(s->curtain, &s->common_frame,
                                   s->viewport.width, s->viewport.height,
                                   error);
}
static int step(void *context, double seconds, const BkInput *input,
                char error[256]) {
  PlaySession *s = context;
  if (s->pending || !isfinite(seconds) || seconds <= 0 || seconds > 1) {
    snprintf(error, 256, "play session: step requires preceding presentation");
    return 0;
  }
  collect_retired(s);
  s->elapsed += seconds;
  /* Loaders consume this frame's original game clock (not the preceding
   * step), notably the secondary selection-camera initialization. */
  s->pending_seconds = (float)seconds;
  s->shown = s->flow.current;
  /*51917c updates B53954 before every dispatch, including menus/loaders.
   * The process screenshot reads this live latch only when it is written. */
  s->game_state.album_group = s->game_state.group;
  switch (s->shown) {
  case 2:
    bk_game_preview_clock(s->game, s->elapsed - seconds);
    bk_game_preview_block(s->game, s->common.blocked);
    if (!s->game || !bk_scene_step(s->game, seconds, input, error) ||
        !prepare_common(s, (float)seconds, error))
      return 0;
    break;
  case 0x40: {
    BkFailureHudFrame frame;
    BkFailureHudOps ops = {
        .context = s, .release = release, .schedule = schedule};
    if (!s->game || !s->failure_active ||
        !bk_game_preview_failure_step(s->game, &s->failure, &s->common,
                                      &s->overlay, &ops, seconds, s->elapsed,
                                      input, &frame, error))
      return 0;
    s->common_frame.curtain_alpha = frame.curtain_alpha;
    if (!bk_curtain_render_prepare(s->curtain, &s->common_frame,
                                   s->viewport.width, s->viewport.height,
                                   error))
      return 0;
    break;
  }
  case 0x20:
    bk_retry_preview_clock(s->checkpoint, s->elapsed - seconds);
    if (!s->checkpoint || !bk_scene_step(s->checkpoint, seconds, input, error))
      return 0;
    break;
  case 0x28:
    bk_save_preview_clock(s->save, s->elapsed - seconds);
    if (!s->save || !bk_scene_step(s->save, seconds, input, error))
      return 0;
    break;
  case 0x68:
    bk_retry_preview_clock(s->retry, s->elapsed - seconds);
    if (!s->retry || !bk_scene_step(s->retry, seconds, input, error))
      return 0;
    break;
  case 0x10:
    /* The loader prepared the first ending snapshot during flow50. Present
     * it through the normal app draw/after_present boundary before asking
     * the scene for another update. */
    if (!s->ending ||
        (!s->ending_first_present &&
         !bk_ending_normal_scene_step_at(s->ending, seconds, s->elapsed,
                                          input, error)))
      return 0;
    break;
  case 4:
    bk_pause_preview_clock(s->pause, s->elapsed - seconds);
    if (!s->pause || !s->game ||
        !bk_game_preview_background_step(s->game, seconds, s->elapsed, error) ||
        !bk_scene_step(s->pause, seconds, input, error))
      return 0;
    break;
  case 0x50: {
    BkFlowLoadingFrame frame;
    BkFlowLoadingOps ops = {s, load_target, confirm};
    if (!bk_flow_loading_step(&s->loading, &s->common, &s->flow, 0,
                              !!(input->pressed & BK_BUTTON_CONFIRM),
                              (float)seconds, &ops, &frame, error) ||
        !bk_flow_loading_render_prepare(s->loading_render, &frame,
                                        s->viewport.width, s->viewport.height,
                                        error))
      return 0;
    break;
  }
  case 1:
  case 0x38:
  case 8:
  case 0x18:
  case 0x48:
    if (s->front) {
      if (!bk_front_end_step(s->front, seconds, s->elapsed, input, error))
        return 0;
      break;
    }
    if (!s->title || !bk_scene_step(s->title, seconds, input, error))
      return 0;
    if ((input->pressed & BK_BUTTON_CONFIRM) && !schedule(s, 2, 1, error))
      return 0;
    break;
  default:
    snprintf(error, 256, "play session: unbound active flow0x%02x", s->shown);
    return 0;
  }
  s->pending_seconds = (float)seconds;
  s->pending = 1;
  s->drawn = 0;
  return 1;
}
int bk_play_session_step_at(BkScene *scene, double seconds, double wall_seconds,
                            const BkInput *input, char error[256]) {
  PlaySession *s = bk_scene_custom_context(scene);
  if (!s || !isfinite(seconds) || seconds <= 0 || seconds > 1 ||
      !isfinite(wall_seconds) || wall_seconds < 1 || wall_seconds > 1e12 ||
      wall_seconds < s->elapsed || s->pending) {
    snprintf(error, 256, "play session: invalid explicit wall clock");
    return 0;
  }
  /* Existing standalone probes use equal wall/game increments. The actual
   * application supplies the independent clock read by native GetTickCount. */
  s->elapsed = wall_seconds - seconds;
  return bk_scene_step(scene, seconds, input, error);
}
double bk_play_session_wall_seconds(BkScene *scene) {
  PlaySession *s = bk_scene_custom_context(scene);
  return s ? s->elapsed : 0;
}
static int draw(void *context, const BkSceneFrame *frame, char error[256]) {
  PlaySession *s = context;
  if (s->shown == 2 || s->shown == 0x40) {
    if (!bk_scene_draw(s->game, frame, error) ||
        !bk_renderer_viewport(s->services.renderer, &s->viewport, error) ||
        !bk_curtain_render_draw(s->curtain, error) ||
        !bk_renderer_viewport(s->services.renderer, NULL, error))
      return 0;
  } else if (s->shown == 4) {
    if (!bk_scene_draw(s->pause, frame, error))
      return 0;
  } else if (s->shown == 0x20) {
    if (!bk_scene_draw(s->checkpoint, frame, error))
      return 0;
  } else if (s->shown == 0x28) {
    if (!bk_scene_draw(s->save, frame, error))
      return 0;
  } else if (s->shown == 0x68) {
    if (!bk_scene_draw(s->retry, frame, error))
      return 0;
  } else if (s->shown == 0x10) {
    if (!bk_scene_draw(s->ending, frame, error))
      return 0;
  } else if (s->shown == 1 || s->shown == 0x38 || s->shown == 8 || s->shown == 0x18 || s->shown == 0x48) {
    if (!(s->front ? bk_front_end_draw(s->front, error)
                   : bk_scene_draw(s->title, frame, error)))
      return 0;
  } else if (s->shown == 0x50) {
    if (!bk_renderer_viewport(s->services.renderer, &s->viewport, error) ||
        !bk_flow_loading_render_draw(s->loading_render, error) ||
        !bk_renderer_viewport(s->services.renderer, NULL, error))
      return 0;
  } else {
    snprintf(error, 256, "play session: no prepared flow");
    return 0;
  }
  s->drawn = 1;
  return 1;
}
int bk_play_session_after_present(BkScene *scene, char error[256]) {
  PlaySession *s = bk_scene_custom_context(scene);
  if (!s || !s->drawn) {
    snprintf(error, 256, "play session: missing presented snapshot");
    return 0;
  }
  if (!s->pending)
    return 1;
  if (s->front && (s->shown == 1 || s->shown == 0x38 || s->shown == 8 || s->shown == 0x18 || s->shown == 0x48) &&
      !bk_front_end_after_present(s->front, error))
    return 0;
  if (s->shown == 2) {
    BkCommonHudBindings bindings = {&s->game_state.interaction.outcome,
                                    &s->game_state.hotkeys.menu_request,
                                    &s->game_state.interaction.response,
                                    &s->overlay,
                                    &s->game_state.group,
                                    &s->game_state.area,
                                    &s->flow.current,
                                    0};
    BkCommonHudOps ops = {s, play_outcome, play_response, schedule, load_pause};
    BkCommonHudFrame committed;
    if (!bk_common_hud_step(&s->common, &bindings, &ops, s->pending_seconds,
                            (uint32_t)(uint64_t)(s->elapsed * 1000), &committed,
                            error))
      return 0;
    if (committed.curtain_alpha != s->common_frame.curtain_alpha) {
      snprintf(error, 256, "play session: curtain preview/commit mismatch");
      return 0;
    }
    if (s->flow.current == 4)
      log_line(s, "Game paused after HUD capture");
  } else if (s->shown == 4 && s->flow.current != 4) {
    s->retire_pause = 1;
    if (s->flow.current == 2)
      log_line(s, "Game resumed with retained session");
  } else if (s->shown == 0x10) {
    if (!bk_ending_normal_scene_after_present(s->ending, error))
      return 0;
    s->ending_first_present = 0;
  }
  s->pending = 0;
  return 1;
}
static void destroy(void *context) {
  PlaySession *s = context;
  bk_front_end_destroy(s->front);
  bk_scene_destroy(s->pause);
  bk_scene_destroy(s->retry);
  bk_scene_destroy(s->checkpoint);
  bk_scene_destroy(s->save);
  bk_scene_destroy(s->game);
  bk_scene_destroy(s->title);
  bk_scene_destroy(s->ending);
  bk_screenshot_destroy(s->screenshot);
  bk_outcome_audio_destroy(s->outcome);
  bk_system_audio_destroy(s->confirm);
  bk_curtain_render_destroy(s->curtain);
  bk_flow_loading_render_destroy(s->loading_render);
  free(s);
}
static BkScene *create(const BkSceneServices *services,
                       BkCheckpointFiles *files, BkUnlockFile *unlock_file,
                       BkRecordFile *record_file, int require_record_file,
                       int development,
                       char error[256]) {
  if (!services || !services->renderer || !services->resources ||
      !services->audio) {
    snprintf(error, 256, "play session: missing persistent services");
    return NULL;
  }
  PlaySession *s = calloc(1, sizeof(*s));
  if (!s) {
    snprintf(error, 256, "play session: allocation failed");
    return NULL;
  }
  s->services = *services;
  s->record_file = record_file;
  s->require_record_file = require_record_file;
  s->special_clock_read = platform_clock;
  for (unsigned i = 0; i < 16; ++i)
    s->menu_camera.pose.world[i] = s->menu_camera.matrix[i] = i % 5 == 0;
  s->ending_auxiliary_cycle = bk_ending_auxiliary_cycle_initial();
  bk_ending_normal_controller_initialize(&s->ending_normal_controller);
  s->ending_secondary_controller = bk_ending_secondary_control_initial();
  s->ending_secondary_presentation = bk_ending_secondary_presentation_initial();
  bk_ending_state_initialize(&s->ending_state);
  bk_ending_process_initialize(&s->ending_process);
  bk_ending_tertiary_controller_initialize(&s->ending_tertiary_controller);
  s->save_files = files;
  s->save_state.control.tab = 3;
  s->elapsed = 1;
  s->flow = development ? (BkFlowTransition){2, 8, 2, 0}
                        : (BkFlowTransition){1, 0, 1, 0};
  unsigned width, height;
  bk_renderer_extent(services->renderer, &width, &height);
  if (!bk_camera_fit(&s->viewport, width, height, 4, 3) ||
      !bk_scene_game_frame_boot_state(&s->game_state, &s->progress,
                                      (uint32_t)time(NULL)) ||
      !bk_menu_cursor_initialize(&s->cursor, s->viewport.width,
                                 s->viewport.height))
    goto bad;
  bk_common_hud_initialize(&s->common);
  if (services->capture_files) {
    s->capture_output = (BkCaptureOutput){services->capture_files,
                                          bk_capture_output_platform_clock()};
    BkScreenshotOutput output = bk_capture_output_service(&s->capture_output);
    s->screenshot = bk_screenshot_create(services->renderer,
                                         services->resources, &output, error);
    if (!s->screenshot || !bk_screenshot_bind_album_group(
            s->screenshot, &s->game_state.album_group, error))
      goto bad;
  }
  bk_flow_loading_initialize(&s->loading, 0);
  s->curtain =
      bk_curtain_render_create(services->renderer, services->resources, error);
  s->loading_render = bk_flow_loading_render_create(
      services->renderer, services->resources, 0, error);
  s->confirm = bk_system_audio_create_slot(services->resources, services->audio,
                                           56, 1, -600, error);
  if (!s->curtain || !s->loading_render || !s->confirm)
    goto bad;
  if (!development) {
    BkFrontEndConfig c = {.services = s->services,
                          .viewport = s->viewport,
                          .common = &s->common,
                          .curtain = s->curtain,
                          .flow = &s->flow,
                          .cursor = &s->cursor,
                          .hover = &s->hover,
                          .group = &s->game_state.group,
                          .area = &s->game_state.area,
                          .random = &s->game_state.random,
                          .photos = s->game_state.hotkeys.photos,
                          .photo_count = &s->game_state.hotkeys.photo_count,
                          .context = s,
                          .schedule = schedule,
                          .unlock_file = unlock_file,
                          .ending_flags = s->ending_unlock_flags,
                          .ending_flags_valid = &s->ending_unlock_valid,
                          .ending_flags_group = &s->ending_unlock_group};
    c.camera = &s->menu_camera;
    c.envelope = &s->game_state.voice;
    c.special = (BkSpecialSessionConfig){
        .process = &s->special_process, .transitions = &s->camera_transitions,
        .camera_clip = &s->ending_state.frame.camera_clip,
        .camera_mode = &s->ending_state.frame.camera_mode,
        .album_group = &s->game_state.album_group,
        .latches = s->game_state.shared_latches,
        .latch_count = sizeof(s->game_state.shared_latches),
        .visibility = s->ending_state.control.toggles,
        .paused = (const int8_t *)&s->ending_state.frame.finish_blocked,
        .screenshot = s->screenshot, .context = s, .clock = special_clock,
        .release_speech = special_release_speech};
    s->front = bk_front_end_create(&c, error);
    if (!s->front)
      goto bad;
    /*4E6F33 saved flags, then4E6F38 records; before any title/ending load.*/
    if (record_file) {
      BkRecordView v[BK_RECORD_GROUPS]; record_views(s, v);
      if (bk_record_file_read(record_file, v, error) == BK_RESOURCE_ERROR)
        goto bad;
    }
  }
  if (!load_target(s, development ? 2 : 1, error) ||
      !step(s, 1.0 / 60, &(BkInput){0}, error))
    goto bad;
  BkScene *scene =
      bk_scene_custom_create(s, (BkSceneCustomOps){step, draw, destroy}, error);
  if (!scene)
    goto bad;
  return scene;
bad:
  destroy(s);
  return NULL;
}
BkScene *bk_play_session_create_with_saves(const BkSceneServices *services,
                                           BkCheckpointFiles *files,
                                           char error[256]) {
  return create(services, files, NULL, NULL, 0, 0, error);
}
BkScene *bk_play_session_create_with_storage(const BkSceneServices *services,
                                             BkCheckpointFiles *files,
                                             BkUnlockFile *unlock_file,
                                             char error[256]) {
  return create(services, files, unlock_file, NULL, 0, 0, error);
}
BkScene *bk_play_session_create_with_progress(const BkSceneServices *services,
    BkCheckpointFiles *files, BkUnlockFile *unlock_file, BkRecordFile *record_file,
    char error[256]) {
  return create(services, files, unlock_file, record_file, 1, 0, error);
}
BkScene *bk_play_session_create_development(const BkSceneServices *services,
                                            BkCheckpointFiles *files,
                                            char error[256]) {
  return create(services, files, NULL, NULL, 0, 1, error);
}
BkScene *bk_play_session_create(const BkSceneServices *services,
                                char error[256]) {
  return bk_play_session_create_with_saves(services, NULL, error);
}
const BkGameFrameState *bk_play_session_state(BkScene *scene) {
  PlaySession *s = bk_scene_custom_context(scene);
  return s ? &s->game_state : NULL;
}
const BkCommonHudState *bk_play_session_common(BkScene *scene) {
  PlaySession *s = bk_scene_custom_context(scene);
  return s ? &s->common : NULL;
}
const BkFlowTransition *bk_play_session_flow(BkScene *scene) {
  PlaySession *s = bk_scene_custom_context(scene);
  return s ? &s->flow : NULL;
}

int bk_play_session_finished(BkScene *scene) {
  PlaySession *s = bk_scene_custom_context(scene);
  return s && !s->pending && s->flow.current == 0x58;
}
