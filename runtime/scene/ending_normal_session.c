#include "scene/ending_normal_session.h"
#include "game/draw_dispatch.h"
#include "model/material_pose.h"
#include "scene/ending_audio.h"
#include "scene/ending_auxiliary.h"
#include "scene/ending_normal_assets.h"
#include "scene/ending_normal_render.h"
#include "scene/ending_stage_ui.h"
#include "scene/ending_ui_batch.h"
#include "scene/ending_ui_cursor.h"
#include "scene/ending_ui_render.h"
#include "scene/common_hud.h"
#include "scene/curtain_render.h"
#include "scene/system_audio.h"
#include "core/camera.h"
#include "core/input.h"
#include "core/matrix.h"
#include "world/actor_pose.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  BkSceneServices services;
  BkEndingState state;
  BkFadeSprite overlay;
  BkMenuCamera camera;
  BkEndingCameraPresets presets;
  BkEndingNormalAssets *assets;
  BkEndingNormalRender *render;
  BkEndingAudio *audio;
  BkMaterialPose *materials;
  int32_t disabled[64];
  size_t disabled_count;
  uint8_t option_a, option_b;
  int32_t selected_group;
  uint32_t random, now_ms;
  unsigned group, variant;
  BkEndingRecords *records;
  BkEndingCameraTransitions camera_transitions;
  BkEndingUi ui;
  BkEndingStageUi stage_ui;
  BkCommonHudState common;
  BkEndingUiNoticeState ui_notices;
  BkEndingUiCompositeFrame ui_frame;
  BkEndingUiRender *ui_render;
  BkEndingUiRender *ui_stage_render;
  BkCurtainRender *ui_curtain;
  BkEndingUiBatch *ui_batch;
  BkEndingUiPickBindings ui_pick;
  float ui_world[BK_ENDING_NORMAL_NODES][16];
  uint8_t ui_present[BK_ENDING_NORMAL_NODES];
  float ui_camera_local[16], ui_view[16], ui_projection[16], ui_viewport[16];
  float ui_camera_position[3];
  BkClipTiming ui_active_timing;
  int32_t ui_active_clip;
  int8_t previous_flow;
  int32_t voice_volume, effect_volume;
  uint8_t ui_flash_wanted, ui_item;
  BkEndingControlRect control_rects[BK_ENDING_CONTROL_RECTS];
  BkSystemAudio *control_audio[3];
  BkInput input;
  float active_seconds;
  int frame_active;
  int pending, valid, drawn, failed;
} EndingNormalScene;

static int fail(char e[256], const char *why) {
  if (e)
    snprintf(e, 256, "ending normal scene: %s", why);
  return 0;
}

static int warp(void *context, float x, float y, char e[256]) {
  EndingNormalScene *s = context;
  if (!s || !isfinite(x) || !isfinite(y) ||
      !bk_menu_camera_dialogue(&s->camera))
    return fail(e, "window warp/camera reset failed");
  return 1;
}

static void release_resources(EndingNormalScene *s) {
  if (!s)
    return;
  char ignored[256];
  bk_ending_ui_batch_destroy(s->ui_batch);
  s->ui_batch = NULL;
  bk_ending_ui_render_destroy(s->ui_stage_render);
  s->ui_stage_render = NULL;
  bk_ending_ui_render_destroy(s->ui_render);
  s->ui_render = NULL;
  bk_curtain_render_destroy(s->ui_curtain);
  s->ui_curtain = NULL;
  if (s->audio)
    bk_ending_audio_stop(s->audio, ignored);
  bk_ending_audio_destroy(s->audio);
  s->audio = NULL;
  bk_ending_normal_render_destroy(s->render);
  s->render = NULL;
  bk_material_pose_destroy(s->materials);
  s->materials = NULL;
  bk_ending_normal_assets_destroy(s->assets);
  s->assets = NULL;
  for (unsigned i = 0; i < 3; ++i) {
    bk_system_audio_destroy(s->control_audio[i]);
    s->control_audio[i] = NULL;
  }
  s->disabled_count = 0;
}

static int control_key(void *context, unsigned code, unsigned mode,
                       uint32_t *result, char e[256]) {
  EndingNormalScene *s = context;
  if (!s || !result || (mode != 1 && mode != 2))
    return fail(e, "invalid ending control key request");
  uint32_t button = code == 0x5a ? BK_BUTTON_BACK : BK_BUTTON_CONFIRM;
  *result = mode == 1 ? !!(s->input.pressed & button)
                      : !!(s->input.held & button);
  return 1;
}

static int control_sound(void *context, unsigned slot, int32_t volume,
                         char e[256]) {
  EndingNormalScene *s = context;
  unsigned index = slot == 0 ? 0 : slot == 5 ? 1 : slot == 3 ? 2 : 3;
  (void)volume;
  if (!s || index >= 3 || !s->control_audio[index])
    return fail(e, "ending control sound is not bound");
  return bk_system_audio_restart(s->control_audio[index], e);
}

static int frame_key(void *context, unsigned code, int *pressed,
                     char e[256]) {
  EndingNormalScene *s = context;
  if (!s || !pressed)
    return fail(e, "invalid ending frame key request");
  uint32_t button = code == 0x70 ? BK_BUTTON_BACK : BK_BUTTON_CONFIRM;
  *pressed = !!(s->input.pressed & button);
  return 1;
}

static int frame_clock(void *context, uint32_t *milliseconds, char e[256]) {
  EndingNormalScene *s = context;
  if (!s || !milliseconds)
    return fail(e, "invalid ending frame clock request");
  *milliseconds = s->now_ms;
  return 1;
}

static int ui_position(void *context, float out[2], char e[256]) {
  EndingNormalScene *s = context;
  if (!s || !out || !isfinite(s->input.pointer_x) ||
      !isfinite(s->input.pointer_y))
    return fail(e, "invalid ending UI pointer");
  out[0] = s->input.pointer_x;
  out[1] = s->input.pointer_y;
  return 1;
}

static int ui_motion(void *context, float out[2], char e[256]) {
  EndingNormalScene *s = context;
  if (!s || !out || !isfinite(s->input.pointer_motion_x) ||
      !isfinite(s->input.pointer_motion_y))
    return fail(e, "invalid ending UI pointer motion");
  out[0] = s->input.pointer_motion_x;
  out[1] = s->input.pointer_motion_y;
  return 1;
}

static uint32_t ui_button(unsigned code) {
  if (code == 0)
    return BK_BUTTON_CONFIRM;
  if (code == 1 || code == 0x5a || code == 0x70)
    return BK_BUTTON_BACK;
  return BK_BUTTON_CONFIRM;
}

static int ui_key(void *context, unsigned code, unsigned mode,
                  uint32_t *result, char e[256]) {
  EndingNormalScene *s = context;
  if (!s || !result || (mode != 1 && mode != 2))
    return fail(e, "invalid ending UI key request");
  uint32_t button = ui_button(code);
  *result = mode == 1 ? !!(s->input.pressed & button)
                      : !!(s->input.held & button);
  return 1;
}

static int ui_voice_playing(void *context, int *playing, char e[256]) {
  EndingNormalScene *s = context;
  BkEndingAudioCall call = {.operation = BK_ENDING_AUDIO_STATUS, .slot = 1};
  if (!s || !s->audio || !playing)
    return fail(e, "ending UI voice owner is missing");
  return bk_ending_audio_call(s->audio, s->state.frame.group,
                              s->state.auxiliary.variant,
                              s->state.auxiliary.selection, &call, playing, e);
}

typedef struct {
  EndingNormalScene *scene;
} EndingUiTailContext;

static int ui_tail_active(void *context, int32_t *slot, char e[256]) {
  EndingUiTailContext *c = context;
  BkClipState state;
  if (!c || !c->scene || !slot ||
      !bk_actor_pose_state(bk_ending_normal_assets_pose(c->scene->assets, 0),
                           &state))
    return fail(e, "ending UI active clip is unavailable");
  *slot = (int32_t)state.slot;
  return 1;
}

static int ui_tail_write(void *context, unsigned slot, BkEndingClipWrite kind,
                         int32_t value, char e[256]) {
  EndingUiTailContext *c = context;
  BkActorPose *primary;
  BkClipEdit edit = {.slot = slot};
  if (!c || !c->scene || !(primary = bk_ending_normal_assets_pose(
                                 c->scene->assets, 0)))
    return fail(e, "ending UI clip owner is missing");
  switch (kind) {
  case BK_ENDING_CLIP_CHAIN:
    edit.fields = BK_CLIP_EDIT_CHAIN;
    edit.chain = value;
    break;
  case BK_ENDING_CLIP_NEXT:
    edit.fields = BK_CLIP_EDIT_NEXT;
    edit.next = value;
    break;
  case BK_ENDING_CLIP_REWIND: {
    BkClipTiming timing;
    if (!bk_actor_pose_timing(primary, slot, &timing))
      return fail(e, "ending UI rewind clip is missing");
    edit.fields = BK_CLIP_EDIT_SOURCE;
    edit.source = timing.start;
    break;
  }
  default:
    return fail(e, "unknown ending UI clip write");
  }
  return bk_actor_pose_edit_clips(primary, &edit, 1, e);
}

static int ui_tail_request(void *context, unsigned slot, char e[256]) {
  EndingUiTailContext *c = context;
  BkActorPose *primary =
      c ? bk_ending_normal_assets_pose(c->scene->assets, 0) : NULL;
  if (!primary)
    return fail(e, "ending UI clip request owner is missing");
  return bk_actor_pose_request_mode(primary, slot, BK_CLIP_REQUEST_CONFIGURED,
                                    e);
}

static int ui_tail_audio(void *context, const BkEndingAudioCall *call,
                         int *playing, char e[256]) {
  EndingUiTailContext *c = context;
  EndingNormalScene *s = c ? c->scene : NULL;
  if (!s || !s->audio || !call || !playing)
    return fail(e, "ending UI audio owner is missing");
  return bk_ending_audio_call(s->audio, s->state.frame.group,
                              s->state.auxiliary.variant,
                              s->state.auxiliary.selection, call, playing, e);
}

static int ui_tail_eyes(void *context, unsigned slot, char e[256]) {
  EndingUiTailContext *c = context;
  BkEyeAssets *eyes = c ? bk_ending_normal_assets_eyes(c->scene->assets) : NULL;
  if (!eyes)
    return fail(e, "ending UI eye owner is missing");
  return bk_eye_assets_select(eyes, slot, e);
}

static int ui_tail_speech(void *context, unsigned slot, const char *name,
                          int32_t volume, char e[256]) {
  EndingUiTailContext *c = context;
  EndingNormalScene *s = c ? c->scene : NULL;
  if (!s || !s->audio)
    return fail(e, "ending UI speech owner is missing");
  return bk_ending_audio_speech(s->audio, slot, name, volume, e);
}

static int ui_reload_load(void *context, BkEndingLoader loader, int32_t argument,
                          char e[256]) {
  (void)context;
  (void)loader;
  (void)argument;
  return fail(e, "ending resource reload is not implemented");
}

static int ui_reload_release(void *context, BkEndingLoader loader, char e[256]) {
  (void)context;
  (void)loader;
  return fail(e, "ending resource release is not implemented");
}

static int ui_reload_final_image(void *context, int create, unsigned group,
                                 char e[256]) {
  (void)context;
  (void)create;
  (void)group;
  return fail(e, "ending final image ownership is not implemented");
}

static int ui_reload_leave(void *context, BkEndingLeave leave, char e[256]) {
  (void)context;
  (void)leave;
  return fail(e, "ending scene leave is not implemented");
}

static int ui_reload_lighting(void *context, BkEndingReloadLight operation,
                              char e[256]) {
  (void)context;
  (void)operation;
  return fail(e, "ending lighting reload is not implemented");
}

static int ui_reload_schedule(void *context, uint8_t target, uint8_t mode,
                              char e[256]) {
  (void)context;
  (void)target;
  (void)mode;
  return fail(e, "ending flow scheduling is not implemented");
}

static int ui_reload_present(void *context, unsigned slot, int *present,
                             char e[256]) {
  EndingNormalScene *s = context;
  if (!s || !present)
    return fail(e, "ending reload audio owner is missing");
  if (slot >= 6) {
    *present = 0;
    return 1;
  }
  return bk_ending_audio_present(s->audio, slot, present);
}

static int ui_reload_status(void *context, unsigned slot, int *playing,
                            char e[256]) {
  EndingNormalScene *s = context;
  BkEndingAudioCall call = {.operation = BK_ENDING_AUDIO_STATUS, .slot = slot};
  if (!s || !s->audio || !playing || slot >= 6)
    return fail(e, "ending reload audio slot is unavailable");
  return bk_ending_audio_call(s->audio, s->state.frame.group,
                              s->state.auxiliary.variant,
                              s->state.auxiliary.selection, &call, playing, e);
}

static int ui_reload_pause(void *context, unsigned slot, char e[256]) {
  EndingNormalScene *s = context;
  BkEndingAudioCall call = {.operation = BK_ENDING_AUDIO_PAUSE, .slot = slot};
  int playing = 0;
  if (!s || !s->audio || slot >= 6)
    return fail(e, "ending reload pause slot is unavailable");
  return bk_ending_audio_call(s->audio, s->state.frame.group,
                              s->state.auxiliary.variant,
                              s->state.auxiliary.selection, &call, &playing, e);
}

static int ui_pose_root_local(BkActorPose *pose, float out[16], char e[256]) {
  const BkModel *model;
  if (!pose || !out || !(model = bk_actor_pose_model(pose)))
    return fail(e, "ending camera track model is missing");
  for (uint32_t frame = 0; frame < model->frame_count; ++frame)
    if (model->frames[frame].parent_index == BK_MODEL_NONE) {
      memcpy(out, bk_actor_pose_local(pose, frame), sizeof(float) * 16);
      return 1;
    }
  return fail(e, "ending camera track root is missing");
}

static int prepare_ui_geometry(EndingNormalScene *s, unsigned width,
                               unsigned height, char e[256]) {
  BkActorForest *forest;
  BkActorPose *primary, *track;
  BkClipState clip;
  const float *view;
  if (!s || !s->assets || !(forest = bk_ending_normal_assets_forest(s->assets)) ||
      !(primary = bk_ending_normal_assets_pose(s->assets, 0)) ||
      !(track = bk_ending_normal_assets_pose(s->assets, 2)) || !width ||
      !height || !bk_actor_pose_state(primary, &clip) ||
      !bk_actor_pose_timing(primary, clip.slot, &s->ui_active_timing) ||
      !ui_pose_root_local(track, s->ui_camera_local, e) ||
      !(view = bk_actor_forest_view(forest)))
    return fail(e, "ending UI geometry owner is unavailable");
  s->ui_active_clip = (int32_t)clip.slot;
  memcpy(s->ui_view, view, sizeof(s->ui_view));
  if (!bk_camera_projection(
          s->ui_projection,
          &(BkCameraLens){s->camera.fov, .75f, .5f, 126384}))
    return fail(e, "ending UI projection is invalid");
  memset(s->ui_viewport, 0, sizeof(s->ui_viewport));
  s->ui_viewport[0] = (float)width * .5f;
  s->ui_viewport[5] = (float)height * -.5f;
  s->ui_viewport[10] = s->ui_viewport[15] = 1;
  s->ui_viewport[12] = (float)width * .5f;
  s->ui_viewport[13] = (float)height * .5f;
  memcpy(s->ui_camera_position, s->camera.pose.position,
         sizeof(s->ui_camera_position));
  memset(s->ui_present, 0, sizeof(s->ui_present));
  for (unsigned i = 0; i < BK_ENDING_NORMAL_NODES; ++i) {
    uint32_t frame = bk_ending_normal_assets_node(s->assets, i);
    if (frame == BK_MODEL_NONE)
      continue;
    uint32_t node = bk_actor_forest_node(forest, 0, frame);
    const float *world = bk_actor_forest_world(forest, node);
    if (node == BK_FRAME_NONE || !world)
      return fail(e, "ending UI target node is not published");
    memcpy(s->ui_world[i], world, sizeof(s->ui_world[i]));
    s->ui_present[i] = 1;
  }
  s->ui_pick = (BkEndingUiPickBindings){
      s->ui_world,       s->ui_present, BK_ENDING_NORMAL_NODES,
      s->ui_camera_position, s->ui_view, s->ui_projection, s->ui_viewport,
      s->ui.sprites[50].rect[2]};
  return 1;
}

static int prepare_ui_frame(EndingNormalScene *s, float seconds, unsigned width,
                            unsigned height, char e[256]) {
  BkEndingStateUiViews views;
  BkEndingUiFrameBindings bindings;
  EndingUiTailContext tail_context = {s};
  BkEndingUiTailOps tail = {{&tail_context, ui_tail_active, ui_tail_write,
                             ui_tail_request, ui_tail_audio, ui_tail_eyes},
                            ui_tail_speech};
  BkEndingReloadOps reload = {
      s,          ui_reload_load,       ui_reload_release,
      ui_reload_final_image, ui_reload_leave, ui_reload_lighting,
      ui_reload_schedule, ui_reload_present, ui_reload_status, ui_reload_pause};
  BkEndingUiFrameOps ops = {s, ui_position, ui_motion, ui_key,
                            ui_voice_playing, reload, tail};
  if (!s || !s->ui_render || !s->ui_batch || !s->ui_curtain ||
      !prepare_ui_geometry(s, width, height, e) ||
      !bk_ending_state_import_frame_aliases(&s->state, &s->common,
                                            &s->stage_ui))
    return fail(e, "ending UI frame preparation failed");
  views = (BkEndingStateUiViews){
      &s->ui_active_clip,
      &s->ui_active_timing,
      bk_ending_normal_assets_config(s->assets)->actions,
      s->ui_camera_local,
      &s->ui_pick,
      &s->ui_notices,
      &s->ui_flash_wanted,
      &s->ui_item,
      &s->previous_flow,
      &s->voice_volume,
      &s->random};
  if (!bk_ending_state_ui_bindings(&s->state, &s->common, &views, &bindings))
    return fail(e, "ending UI live bindings are unavailable");
  float scale = (float)((double)width / 1280.0);
  if (!bk_ending_ui_frame(&s->ui, &s->stage_ui, &s->state.ui_controller,
                          &bindings, &ops, scale, seconds, &s->ui_frame, e) ||
      !bk_ending_state_export_frame_requests(&s->state, &s->common) ||
      !bk_ending_ui_batch_prepare(s->ui_batch, &s->ui_frame, s->ui_render,
                                  s->ui_stage_render, width, height, e))
    return 0;
  return 1;
}

static int advance_story_primary(EndingNormalScene *s, float seconds,
                                 char e[256]) {
  BkActorPose *primary = bk_ending_normal_assets_pose(s->assets, 0);
  if (!primary || !bk_actor_pose_advance(primary, -1, seconds, e) ||
      !bk_bom_assets_advance(bk_ending_normal_assets_bom(s->assets), seconds,
                             e) ||
      !bk_bom_assets_follow_references(bk_ending_normal_assets_bom(s->assets),
                                       e) ||
      !bk_face_assets_step(
          bk_ending_normal_assets_face(s->assets),
          bk_ending_normal_assets_face_state(s->assets),
          bk_ending_normal_assets_config(s->assets)->expression_a, 0,
          s->now_ms, s->now_ms, s->now_ms, s->now_ms, &s->random, e))
    return 0;
  return 1;
}

static int advance_story_secondary(EndingNormalScene *s, float seconds,
                                   char e[256]) {
  BkActorPose *auxiliary = bk_ending_normal_assets_pose(s->assets, 1);
  BkActorPose *track = bk_ending_normal_assets_pose(s->assets, 2);
  BkActorPose *locator = bk_ending_normal_assets_pose(s->assets, 3);
  if (!auxiliary || !track || !locator ||
      !bk_actor_pose_advance(auxiliary, -1, seconds, e) ||
      !bk_actor_pose_advance(track, -1, seconds, e) ||
      !bk_actor_pose_advance(locator, -1, seconds, e) ||
      !bk_actor_forest_refresh(bk_ending_normal_assets_forest(s->assets), e) ||
      !bk_ending_normal_render_movie_step(s->render, (int32_t)s->now_ms,
                                          (int32_t)s->now_ms + 1, e))
    return 0;
  return 1;
}

static int ending_target_node(EndingNormalScene *s, uint32_t *node,
                              char e[256]) {
  uint32_t frame = bk_ending_normal_assets_node(s->assets, 0);
  if (frame == BK_MODEL_NONE)
    return fail(e, "ending camera target is missing");
  *node = bk_actor_forest_node(bk_ending_normal_assets_forest(s->assets), 0,
                               frame);
  return *node != BK_FRAME_NONE
             ? 1
             : fail(e, "ending camera target is not attached");
}

static int frame_invoke(void *context, const BkEndingCall *call,
                        uint32_t *result, char e[256]) {
  EndingNormalScene *s = context;
  if (!s || !call || !result || !s->assets || !s->frame_active)
    return fail(e, "invalid live ending frame binding");
  *result = 0;
  switch (call->operation) {
  case BK_ENDING_COMMON_4D7AC4: {
    BkEndingControlBindings bindings = {
        &s->state.frame,
        &s->camera,
        &s->presets,
        {bk_ending_normal_assets_target(s->assets, 0),
         bk_ending_normal_assets_target(s->assets, 1),
         bk_ending_normal_assets_target(s->assets, 2), NULL},
        s->control_rects,
        &(float){.5f},
        &(int32_t){0}};
    BkEndingControlOps ops = {s, control_key, control_sound, NULL, warp};
    return bk_ending_control_step(&s->state.control, &bindings,
                                  &call->input, &ops, e);
  }
  case BK_ENDING_STAGE_4DB608:
  case BK_ENDING_STAGE_47A5D0:
  case BK_ENDING_STAGE_476720:
  case BK_ENDING_STAGE_47DC79:
  case BK_ENDING_STAGE_48D8E9:
    return advance_story_primary(s, s->active_seconds, e);
  case BK_ENDING_STAGE_4DF6C0:
  case BK_ENDING_STAGE_47D3CB:
  case BK_ENDING_STAGE_479137:
  case BK_ENDING_STAGE_48181F:
  case BK_ENDING_STAGE_494015:
    return advance_story_secondary(s, s->active_seconds, e);
  case BK_ENDING_AUXILIARY_4965B9: {
    BkEndingAuxiliaryServices services = {
        bk_ending_normal_assets_pose(s->assets, 0),
        bk_ending_normal_assets_eyes(s->assets), s->audio};
    return bk_ending_auxiliary_apply(
        &services, &s->state.auxiliary, &s->state.frame,
        s->state.frame.auxiliary_mode, 0, 0, (int32_t *)result, e);
  }
  case BK_ENDING_CAMERA_4BB0A4:
  case BK_ENDING_CAMERA_4DF411:
  case BK_ENDING_CAMERA_4E1711:
  case BK_ENDING_CAMERA_4E1D16:
  case BK_ENDING_CAMERA_4E0ECB:
  case BK_ENDING_CAMERA_4BC444: {
    float offset[3] = {0, 0, 0};
    if (call->count >= 4)
      memcpy(offset, call->args + (call->operation == BK_ENDING_CAMERA_4E0ECB ||
                                   call->operation == BK_ENDING_CAMERA_4BC444
                                       ? 2
                                       : 1),
             sizeof(offset));
    const uint32_t tracks[2] = {2, 3};
    unsigned buttons = (s->input.held & BK_BUTTON_CONFIRM ? 1u : 0u) |
                       (s->input.held & BK_BUTTON_BACK ? 2u : 0u);
    float motion[2] = {s->input.look_x, s->input.look_y};
    uint32_t target;
    if (call->operation == BK_ENDING_CAMERA_4BB0A4)
      return bk_ending_camera_assets_step(
          bk_ending_normal_assets_cameras(s->assets),
          bk_ending_normal_assets_forest(s->assets), tracks, &s->camera,
          BK_ENDING_CAMERA_ORBIT, offset, motion, buttons, BK_FRAME_NONE,
          s->active_seconds, e);
    if (call->operation == BK_ENDING_CAMERA_4DF411)
      return bk_ending_camera_assets_step(
          bk_ending_normal_assets_cameras(s->assets),
          bk_ending_normal_assets_forest(s->assets), tracks, &s->camera,
          BK_ENDING_CAMERA_FIXED, offset, NULL, 0, BK_FRAME_NONE, 0, e);
    if (!ending_target_node(s, &target, e))
      return 0;
    if (call->operation == BK_ENDING_CAMERA_4E1711)
      return bk_ending_camera_assets_step(
          bk_ending_normal_assets_cameras(s->assets),
          bk_ending_normal_assets_forest(s->assets), tracks, &s->camera,
          BK_ENDING_CAMERA_AUTO, offset, NULL, 0, target, s->active_seconds, e);
    if (call->operation == BK_ENDING_CAMERA_4E1D16)
      return bk_ending_camera_assets_step(
          bk_ending_normal_assets_cameras(s->assets),
          bk_ending_normal_assets_forest(s->assets), tracks, &s->camera,
          BK_ENDING_CAMERA_MANUAL, offset, motion, buttons, target,
          s->active_seconds, e);
    if (call->count < 5)
      return fail(e, "ending preset call is truncated");
    BkEndingCameraPresetGate gate = {8,
                                    s->state.frame.phase,
                                    s->state.selected,
                                    s->state.frame.state_721ee0,
                                    s->state.frame.state_721ee4,
                                    s->state.control.state_721eec,
                                    0,
                                    s->state.next_mode};
    int done = 0;
    int ok = bk_ending_camera_assets_preset(
        bk_ending_normal_assets_cameras(s->assets),
        bk_ending_normal_assets_forest(s->assets), tracks, &s->camera,
        &s->camera_transitions, &s->presets,
        call->operation == BK_ENDING_CAMERA_4BC444 ? BK_ENDING_PRESET_ZOOM
                                                   : BK_ENDING_PRESET,
        call->args[1], offset, &gate, 0x10, s->active_seconds, &done, e);
    *result = (uint32_t)done;
    return ok;
  }
  case BK_ENDING_STAGE_48302B:
  case BK_ENDING_STAGE_48BCBB:
  case BK_ENDING_STAGE_4E2223:
    return fail(e, "ending phase controller is not implemented");
  default:
    return fail(e, "unknown ending frame operation");
  }
}

static int prepare_story_frame(EndingNormalScene *s, char e[256]) {
  BkEndingFrameInput input = {0};
  memcpy(&input.words[9], &(int32_t){(int32_t)s->input.pointer_x}, 4);
  memcpy(&input.words[10], &(int32_t){(int32_t)s->input.pointer_y}, 4);
  BkEndingFrameOps ops = {s, frame_invoke, frame_clock, frame_key};
  return bk_ending_frame_step(&s->state.frame, s->state.working, &input, &ops,
                              e);
}

static int normal_load(void *context, BkEndingLoader loader, int32_t argument,
                       char e[256]) {
  EndingNormalScene *s = context;
  unsigned variant;
  if (!s || argument != -1)
    return fail(e, "invalid normal loader argument");
  /* 4d00fa is the second normal gallery profile. The remaining loaders have
   * different scene topologies and must reach their own owners later. */
  if (loader == BK_ENDING_LOAD_4CF318)
    variant = 0;
  else if (loader == BK_ENDING_LOAD_4D00FA)
    variant = 1;
  else
    return fail(e, "normal scene does not own this ending loader");
  release_resources(s);
  uint32_t clocks[4] = {s->now_ms, s->now_ms, s->now_ms, s->now_ms};
  s->assets = bk_ending_normal_assets_create(
      s->services.resources, s->state.frame.group, variant, clocks, &s->random,
      &s->camera, &s->presets, e);
  if (!s->assets ||
      !bk_ending_normal_assets_load_background(s->assets, s->services.resources,
                                               e))
    goto bad;
  s->render = bk_ending_normal_render_create(
      s->services.renderer, s->services.resources, s->assets, (int32_t)s->now_ms,
      e);
  if (!s->render)
    goto bad;
  s->materials = bk_material_pose_create(
      bk_actor_pose_model(bk_ending_normal_assets_pose(s->assets, 0)), e);
  if (!s->materials)
    goto bad;
  s->disabled_count = bk_bom_assets_count(bk_ending_normal_assets_bom(s->assets));
  if (s->disabled_count > sizeof(s->disabled) / sizeof(s->disabled[0])) {
    fail(e, "normal disabled-model table is too large");
    goto bad;
  }
  memset(s->disabled, 0, sizeof(s->disabled));
  s->audio = bk_ending_audio_create_entry(
      s->services.resources, s->services.audio, 0, s->state.frame.group, variant,
      -900, e);
  if (!s->audio)
    goto bad;
  unsigned width, height;
  bk_renderer_extent(s->services.renderer, &width, &height);
  if (!bk_ending_stage_ui_initialize(&s->ui, &s->stage_ui,
                                     BK_ENDING_UI_NORMAL,
                                     s->state.frame.group, variant, width, e) ||
      !(s->ui_render = bk_ending_ui_render_create(
            s->services.renderer, s->services.resources, e)) ||
      !(s->ui_curtain = bk_curtain_render_create(
            s->services.renderer, s->services.resources, e)) ||
      !(s->ui_batch = bk_ending_ui_batch_create(s->ui_curtain, e)))
    goto bad;
  s->voice_volume = -1000;
  s->effect_volume = -600;
  static const unsigned control_slots[3] = {0, 5, 3};
  for (unsigned i = 0; i < 3; ++i) {
    s->control_audio[i] = bk_system_audio_create_slot(
        s->services.resources, s->services.audio, 57 + i, control_slots[i], -600,
        e);
    if (!s->control_audio[i])
      goto bad;
  }
  s->variant = variant;
  return 1;
bad:
  release_resources(s);
  return 0;
}

static int clear_record(void *context, unsigned group, char e[256]) {
  EndingNormalScene *s = context;
  if (!s || !s->records)
    return fail(e, "story record owner is missing");
  return bk_ending_record_clear(s->records, group, e);
}

static int prepare_frame(EndingNormalScene *s, float seconds, char e[256]) {
  BkActorPose *primary, *camera_track;
  BkActorForest *forest;
  BkDrawDispatchInput in;
  BkDrawDispatch dispatch;
  BkFog fog = {0};
  uint32_t roots[3] = {BK_MODEL_NONE, BK_MODEL_NONE, BK_MODEL_NONE};
  unsigned width, height;
  if (!s || !s->assets || !s->render || !isfinite(seconds) || seconds <= 0 ||
      seconds > 1)
    return fail(e, "invalid live normal scene");
  s->active_seconds = seconds;
  uint32_t step_ms = (uint32_t)((double)seconds * 1000.0);
  s->now_ms += step_ms ? step_ms : 1;
  bk_renderer_extent(s->services.renderer, &width, &height);
  if (!bk_ending_ui_batch_begin(s->ui_batch, s->ui_render, s->ui_stage_render,
                                e))
    return 0;
  if (s->frame_active) {
    if (!prepare_story_frame(s, e))
      return 0;
  } else {
    primary = bk_ending_normal_assets_pose(s->assets, 0);
    camera_track = bk_ending_normal_assets_pose(s->assets, 2);
    if (!primary || !camera_track ||
        !bk_actor_pose_advance(primary, -1, seconds, e) ||
        !bk_actor_pose_advance(camera_track, -1, seconds, e) ||
        !bk_bom_assets_advance(bk_ending_normal_assets_bom(s->assets), seconds,
                               e) ||
        !bk_bom_assets_follow_references(
            bk_ending_normal_assets_bom(s->assets), e) ||
        !bk_face_assets_step(
            bk_ending_normal_assets_face(s->assets),
            bk_ending_normal_assets_face_state(s->assets),
            bk_ending_normal_assets_config(s->assets)->expression_a, 0,
            s->now_ms, s->now_ms, s->now_ms, s->now_ms, &s->random, e))
      return 0;
  }
  forest = bk_ending_normal_assets_forest(s->assets);
  if (!s->frame_active && !bk_actor_forest_refresh(forest, e))
    return 0;
  for (unsigned actor = 0; actor < 3; ++actor) {
    unsigned source = actor == 2 ? 4 : actor;
    BkActorPose *pose = bk_ending_normal_assets_pose(s->assets, source);
    const BkModel *model;
    if (!pose)
      return fail(e, "normal actor disappeared");
    model = bk_actor_pose_model(pose);
    for (uint32_t frame = 0; frame < model->frame_count; ++frame)
      if (model->frames[frame].parent_index == BK_MODEL_NONE) {
        roots[actor] = bk_actor_forest_node(forest, source, frame);
        break;
      }
    if (roots[actor] == BK_MODEL_NONE)
      return fail(e, "normal actor root is missing");
  }
  if (!s->frame_active &&
      !bk_ending_normal_render_movie_step(s->render, (int32_t)s->now_ms,
                                          (int32_t)s->now_ms + 1, e))
    return 0;
  in = (BkDrawDispatchInput){.flow = 16,
                             .group = (int32_t)s->state.frame.group,
                             .event_state = 1,
                             .root_721b28 = roots[0],
                             .root_721b2c = bk_actor_forest_node(forest, 1, 0),
                             .root_721b34 = roots[2]};
  /* Resolve the auxiliary model root from its live forest binding rather than
   * guessing frame zero; roots[1] is the only source used by slot 5. */
  roots[1] = BK_MODEL_NONE;
  {
    const BkModel *model =
        bk_actor_pose_model(bk_ending_normal_assets_pose(s->assets, 1));
    for (uint32_t frame = 0; frame < model->frame_count; ++frame)
      if (model->frames[frame].parent_index == BK_MODEL_NONE) {
        roots[1] = bk_actor_forest_node(forest, 1, frame);
        break;
      }
  }
  if (roots[1] == BK_MODEL_NONE)
    return fail(e, "normal auxiliary root is missing");
  in.root_721b2c = roots[1];
  if (!bk_draw_dispatch_select(&in, &dispatch) ||
      !bk_ending_normal_render_prepare_regular(
          s->render, &dispatch, &s->camera, &fog, s->materials, s->disabled,
          s->disabled_count, e))
    return 0;
  if (!prepare_ui_frame(s, seconds, width, height, e))
    return 0;
  s->pending = 1;
  s->valid = 1;
  s->drawn = 0;
  return 1;
}

static int step(void *context, double seconds, const BkInput *input,
                char e[256]) {
  EndingNormalScene *s = context;
  if (!s || !input || s->pending || s->failed)
    return fail(e, "previous ending snapshot not presented");
  s->input = *input;
  s->failed = 1;
  if (!prepare_frame(s, (float)seconds, e))
    return 0;
  s->failed = 0;
  return 1;
}

static int draw(void *context, const BkSceneFrame *frame, char e[256]) {
  EndingNormalScene *s = context;
  (void)frame;
  if (!s || (!s->pending && !s->valid) || !s->render)
    return fail(e, "no ending snapshot");
  if (!bk_ending_normal_render_draw(s->render, e))
    return 0;
  if (!bk_ending_ui_batch_draw(s->ui_batch, e))
    return 0;
  s->drawn = 1;
  return 1;
}

static void destroy(void *context) {
  EndingNormalScene *s = context;
  if (!s)
    return;
  release_resources(s);
  free(s);
}

static BkScene *create_entry(const BkSceneServices *services, unsigned group,
                             unsigned variant, int8_t previous,
                             uint32_t selected, BkEndingRecords *records,
                             char e[256]) {
  if (!services || !services->renderer || !services->resources ||
      !services->audio || group >= 5 || variant > 1)
    return NULL;
  EndingNormalScene *s = calloc(1, sizeof(*s));
  if (!s)
    return NULL;
  s->services = *services;
  s->group = group;
  s->variant = variant;
  s->records = records;
  s->frame_active = records != NULL;
  s->previous_flow = records ? 8 : 0x18;
  s->random = UINT32_C(0x4cc582) ^ (group * UINT32_C(0x9e3779b9));
  s->overlay = (BkFadeSprite){0, 2, 0};
  bk_common_hud_initialize(&s->common);
  BkEndingStateOps state_ops = {s, warp};
  if (!bk_ending_state_begin(&s->state, &s->overlay, group, variant, .5f,
                             (int32_t[2]){0, 0}, &state_ops, e))
    goto bad;
  unsigned width, height;
  bk_renderer_extent(services->renderer, &width, &height);
  if (!bk_ending_ui_initialize(&s->ui, width, s->state.control.pause_flags,
                               &s->state.gauge_y, e) ||
      !bk_ending_ui_control_rects(&s->ui, s->control_rects))
    goto bad;
  BkEndingEntryBindings bindings;
  if (!bk_ending_state_entry_bindings(&s->state, &s->option_a, &s->option_b,
                                      &s->selected_group, &bindings))
    goto bad;
  BkEndingEntryOps ops = {s, normal_load, records ? clear_record : NULL, NULL};
  /* Gallery selection 0/1 enters the two normal loader profiles without
   * pretending that story record persistence or other loaders exist. */
  if (!bk_ending_entry_dispatch(&bindings, previous, selected, .5f, &ops, e))
    goto bad;
  if (!s->assets || !s->render || !s->audio)
    goto bad;
  /* The application may present a first frame before its fixed-step clock
   * emits a tick. Build that snapshot here so the scene follows the same
   * load->present contract as the other scene owners. */
  if (!prepare_frame(s, 1.0f / 60.0f, e))
    goto bad;
  BkScene *scene = bk_scene_custom_create(
      s, (BkSceneCustomOps){step, draw, destroy}, e);
  if (!scene)
    goto bad;
  return scene;
bad:
  destroy(s);
  return NULL;
}

BkScene *bk_ending_normal_scene_create(const BkSceneServices *services,
                                       unsigned group, unsigned variant,
                                       char e[256]) {
  return create_entry(services, group, variant, 0x18, variant, NULL, e);
}

BkScene *bk_ending_normal_scene_create_story(const BkSceneServices *services,
                                             unsigned group, unsigned variant,
                                             BkEndingRecords *records,
                                             char e[256]) {
  return create_entry(services, group, variant, 8, variant, records, e);
}

int bk_ending_normal_scene_after_present(BkScene *scene, char e[256]) {
  EndingNormalScene *s = bk_scene_custom_context(scene);
  if (!s || !s->valid || !s->drawn)
    return fail(e, "ending snapshot was not presented");
  s->pending = 0;
  s->drawn = 0;
  return 1;
}

const BkEndingState *bk_ending_normal_scene_state(const BkScene *scene) {
  EndingNormalScene *s =
      bk_scene_custom_context((BkScene *)(uintptr_t)scene);
  return s ? &s->state : NULL;
}
