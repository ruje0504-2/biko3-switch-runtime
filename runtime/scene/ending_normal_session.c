#include "scene/ending_normal_session.h"
#include "game/draw_dispatch.h"
#include "model/material_pose.h"
#include "scene/ending_audio.h"
#include "scene/ending_auxiliary.h"
#include "scene/ending_normal_assets.h"
#include "scene/ending_normal_render.h"
#include "scene/ending_secondary_controller.h"
#include "scene/ending_secondary_presentation.h"
#include "scene/ending_selected_assets.h"
#include "scene/ending_auxiliary_assets.h"
#include "game/ending_auxiliary_presentation.h"
#include "game/ending_auxiliary_controller.h"
#include "game/ending_auxiliary_state1.h"
#include "game/ending_auxiliary_state3.h"
#include "game/ending_auxiliary_state4.h"
#include "scene/ending_selected_presentation.h"
#include "game/ending_selected_action.h"
#include "scene/ending_secondary_ui.h"
#include "scene/ending_tertiary_presentation.h"
#include "scene/ending_stage_ui.h"
#include "scene/ending_ui_batch.h"
#include "scene/ending_ui_cursor.h"
#include "scene/ending_ui_geometry.h"
#include "scene/ending_ui_render.h"
#include "scene/common_hud.h"
#include "scene/curtain_render.h"
#include "scene/system_audio.h"
#include "core/camera.h"
#include "core/input.h"
#include "core/matrix.h"
#include "core/random.h"
#include "world/actor_pose.h"
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*A resource release happens in the UI tail, after the 3D draw was captured.
 * Keep that exact stage alive through presentation and zero-tick redraws;
 * retire it only when the next actual tick replaces the snapshot. */
typedef struct {
  BkEndingNormalAssets *normal;
  BkEndingSecondaryAssets *secondary;
  BkEndingTertiaryAssets *tertiary;
  BkEndingSelectedAssets *selected;
  BkEndingAuxiliaryAssets *auxiliary;
  BkEndingNormalRender *render;
  BkMaterialPose *materials;
  BkEndingSpecialMaterial *special_materials;
} EndingRetiredStage;

typedef struct {
  BkSceneServices services;
  BkEndingState diagnostic_state;
  BkEndingState *state;
  BkFadeSprite overlay;
  BkMenuCamera camera;
  BkEndingCameraPresets presets;
  BkEndingNormalAssets *assets;
  BkEndingSecondaryAssets *secondary_assets;
  BkEndingTertiaryAssets *tertiary_assets;
  BkEndingSelectedAssets *selected_assets;
  BkEndingAuxiliaryAssets *auxiliary_assets;
  BkEndingNormalRender *render;
  BkEndingNormalRender *snapshot_render;
  EndingRetiredStage retired;
  BkEndingBackgroundAssets *retained_background;
  BkRetainedLightState retained_lights;
  int has_retained_lights, final_image, snapshot_ui_only;
  BkEndingAudio *audio;
  BkMaterialPose *materials;
  BkEndingSpecialMaterial *special_materials;
  size_t special_material_count;
  int32_t disabled[64];
  size_t disabled_count;
  uint8_t option_a, option_b;
  int32_t selected_group;
  uint32_t diagnostic_random, now_ms;
  uint32_t *random;
  double wall_seconds;
  int clock_supplied;
  unsigned group, variant;
  uint32_t gallery_selection;
  BkEndingRecords *records;
  BkEndingCameraTransitions camera_transitions;
  BkEndingUi ui;
  BkEndingStageUi stage_ui;
  BkCommonHudState diagnostic_common;
  BkCommonHudState *common;
  BkEndingNormalFlow flow;
  BkEndingAuxiliaryCycle diagnostic_cycle;
  BkEndingAuxiliaryCycle *auxiliary_cycle;
  BkEndingNormalControllerRetained diagnostic_controller;
  BkEndingNormalControllerRetained *normal_controller;
  BkEndingPresentationRetained diagnostic_presentation;
  BkEndingPresentationRetained *presentation;
  BkEndingSecondaryControlState diagnostic_secondary_controller;
  BkEndingSecondaryControlState *secondary_controller;
  BkEndingSecondaryPresentationState diagnostic_secondary_presentation;
  BkEndingSecondaryPresentationState *secondary_presentation;
  BkEndingTertiaryControllerRetained diagnostic_tertiary_controller;
  BkEndingTertiaryControllerRetained *tertiary_controller;
  BkEndingSelectedControlState diagnostic_selected_controller;
  BkEndingSelectedControlState *selected_controller;
  BkEndingSelectedActionState selected_action;
  BkEndingSelectedCycle diagnostic_selected_cycle;
  BkEndingSelectedCycle *selected_cycle;
  int32_t selected_plain_scheduled;
  uint8_t selected_ui_bytes[80][3];
  float selected_ui_alpha[80];
  int32_t diagnostic_duck_transition;
  int32_t *duck_transition;
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
  BkSystemAudio *control_audio[4];
  BkVirtualPointer pointer;
  BkViewport viewport;
  BkViewport special_viewport;
  BkInput input;
  float active_seconds;
  int frame_active;
  int pending, valid, drawn, failed, stopped;
} EndingNormalScene;

static int normal_load(void *, BkEndingLoader, int32_t, char[256]);
static void release_retired(EndingRetiredStage *stage) {
  bk_ending_normal_render_destroy(stage->render);
  free(stage->special_materials);
  if (stage->normal)
    bk_material_pose_destroy(stage->materials);
  bk_ending_normal_assets_destroy(stage->normal);
  bk_ending_secondary_assets_destroy(stage->secondary);
  bk_ending_tertiary_assets_destroy(stage->tertiary);
  bk_ending_selected_assets_destroy(stage->selected);
  bk_ending_auxiliary_assets_destroy(stage->auxiliary);
  memset(stage, 0, sizeof(*stage));
}
static BkEndingBackgroundAssets *scene_retired_background(const EndingNormalScene *s) {
  return s->retired.tertiary
      ? bk_ending_tertiary_assets_background(s->retired.tertiary)
      : s->retired.auxiliary
      ? bk_ending_auxiliary_assets_background(s->retired.auxiliary)
      : s->retired.selected
      ? bk_ending_selected_assets_background(s->retired.selected)
      : s->retired.normal
      ? bk_ending_normal_assets_background(s->retired.normal)
      : bk_ending_secondary_assets_background(s->retired.secondary);
}

/*Common frame/UI services borrow the active loader's explicit topology.
 * Physical registry IDs are never inferred from logical actor roles. */
static BkActorForest *scene_forest(EndingNormalScene *s) {
  return s ? (s->tertiary_assets
      ? bk_ending_tertiary_assets_forest(s->tertiary_assets)
      : s->auxiliary_assets
      ? bk_ending_auxiliary_assets_forest(s->auxiliary_assets)
      : s->secondary_assets
      ? bk_ending_secondary_assets_forest(s->secondary_assets)
      : s->selected_assets
      ? bk_ending_selected_assets_forest(s->selected_assets)
      : bk_ending_normal_assets_forest(s->assets)) : NULL;
}
static BkActorPose *scene_primary(EndingNormalScene *s) {
  return s ? (s->tertiary_assets
      ? bk_ending_tertiary_assets_pose(s->tertiary_assets, 0)
      : s->auxiliary_assets
      ? bk_ending_auxiliary_assets_pose(s->auxiliary_assets, 0)
      : s->secondary_assets
      ? bk_ending_secondary_assets_pose(s->secondary_assets, 0)
      : s->selected_assets
      ? bk_ending_selected_assets_pose(s->selected_assets, 0)
      : bk_ending_normal_assets_pose(s->assets, 0)) : NULL;
}
static int auxiliary_state4_prepare_actor(void *context, char e[256]) {
  EndingNormalScene *s = context;
  BkActorPose *primary = scene_primary(s);
  static const unsigned slots[] = {2, 3};
  if (!primary) {
    if (e)
      snprintf(e, 256, "ending normal scene: 47DC79 primary XAN is unavailable");
    return 0;
  }
  return bk_actor_pose_reset_sources(primary, slots, 2, e);
}
static uint32_t scene_node(EndingNormalScene *s, unsigned index) {
  return s->tertiary_assets
      ? bk_ending_tertiary_assets_node(s->tertiary_assets, index)
      : s->auxiliary_assets
      ? bk_ending_auxiliary_assets_node(s->auxiliary_assets, index)
      : s->secondary_assets
      ? bk_ending_secondary_assets_node(s->secondary_assets, index)
      : s->selected_assets
      ? bk_ending_selected_assets_node(s->selected_assets, index)
      : bk_ending_normal_assets_node(s->assets, index);
}
static BkEyeAssets *scene_eyes(EndingNormalScene *s) {
  return s ? (s->tertiary_assets
      ? bk_ending_tertiary_assets_eyes(s->tertiary_assets)
      : s->auxiliary_assets
      ? bk_ending_auxiliary_assets_eyes(s->auxiliary_assets)
      : s->secondary_assets
      ? bk_ending_secondary_assets_eyes(s->secondary_assets)
      : s->selected_assets
      ? bk_ending_selected_assets_eyes(s->selected_assets)
      : bk_ending_normal_assets_eyes(s->assets)) : NULL;
}
static BkEndingCameraAssets *scene_cameras(EndingNormalScene *s) {
  return s->tertiary_assets
      ? bk_ending_tertiary_assets_cameras(s->tertiary_assets)
      : s->auxiliary_assets
      ? bk_ending_auxiliary_assets_cameras(s->auxiliary_assets)
      : s->secondary_assets
      ? bk_ending_secondary_assets_cameras(s->secondary_assets)
      : s->selected_assets
      ? bk_ending_selected_assets_cameras(s->selected_assets)
      : bk_ending_normal_assets_cameras(s->assets);
}
/*Logical roles: primary, upper, lower, track0, track1, background.*/
static unsigned scene_legacy_role(const EndingNormalScene *s, unsigned role) {
  static const unsigned normal[] = {0, 1, BK_MODEL_NONE, 2, 3, 4};
  static const unsigned secondary[] = {0, BK_MODEL_NONE, BK_MODEL_NONE, 1, 2, 3};
  static const unsigned selected[] = {0, BK_MODEL_NONE, BK_MODEL_NONE, 1, 2, 3};
  return role < 6 ? (s->secondary_assets || s->auxiliary_assets ? secondary[role] :
      s->selected_assets ? selected[role] : normal[role]) : BK_MODEL_NONE;
}
static BkActorPose *scene_actor(EndingNormalScene *s, unsigned role) {
  if (!s || role >= 6)
    return NULL;
  if (s->tertiary_assets)
    return bk_ending_tertiary_assets_pose(s->tertiary_assets, role);
  if (s->auxiliary_assets)
    return bk_ending_auxiliary_assets_pose(s->auxiliary_assets, scene_legacy_role(s, role));
  unsigned index = scene_legacy_role(s, role);
  return index == BK_MODEL_NONE ? NULL : s->secondary_assets
      ? bk_ending_secondary_assets_pose(s->secondary_assets, index)
      : s->selected_assets ? bk_ending_selected_assets_pose(s->selected_assets, index)
      : bk_ending_normal_assets_pose(s->assets, index);
}
static uint32_t scene_registry(EndingNormalScene *s, unsigned role) {
  if (!scene_actor(s, role))
    return BK_MODEL_NONE;
  return s->tertiary_assets ? bk_ending_tertiary_assets_registry(s->tertiary_assets, role)
                            : scene_legacy_role(s, role);
}
static uint32_t scene_root(EndingNormalScene *s, unsigned role) {
  if (!scene_actor(s, role))
    return 0;
  if (s->tertiary_assets)
    return bk_ending_tertiary_assets_root(s->tertiary_assets, role);
  unsigned index = scene_legacy_role(s, role);
  return s->auxiliary_assets ? bk_ending_auxiliary_assets_root(s->auxiliary_assets, index)
                             : s->secondary_assets ? bk_ending_secondary_assets_root(s->secondary_assets, index)
                             : s->selected_assets ? bk_ending_selected_assets_root(s->selected_assets, index)
                             : bk_ending_normal_assets_root(s->assets, index);
}

static int fail(char e[256], const char *why) {
  if (e)
    snprintf(e, 256, "ending normal scene: %s", why);
  return 0;
}

static int warp(void *context, float x, float y, char e[256]) {
  EndingNormalScene *s = context;
  if (!s || !isfinite(x) || !isfinite(y))
    return fail(e, "invalid window pointer warp");
  s->pointer.position[0] = x;
  s->pointer.position[1] = y;
  return 1;
}

static int stop_audio(EndingNormalScene *s, char e[256]) {
  if (!s)
    return fail(e, "missing ending audio owner");
  if (s->stopped)
    return 1;
  if (s->audio && !bk_ending_audio_stop(s->audio, e))
    return 0;
  for (unsigned i = 0; i < 4; ++i)
    if (s->control_audio[i] &&
        !bk_system_audio_stop(s->control_audio[i], e))
      return 0;
  /* These IDs belong to the logically released forest. Clear them while
   * this entry still owns the live state, never in a later old-snapshot
   * destructor after another entry has rebound the same process owner. */
  if (s->state) {
    s->state->retained.normal.follow_target = 0;
    s->state->retained.normal.direct_reference = 0;
    s->state->retained.normal.direct_node = 0;
    s->state->retained.normal.word_719b40 = 0;
  }
  s->stopped = 1;
  return 1;
}

static void release_resources(EndingNormalScene *s) {
  if (!s)
    return;
  char ignored[256];
  /* Logical release may already have handed the mixer slots to another
   * scene. Never clear them again while retiring the last GPU snapshot. */
  stop_audio(s, ignored);
  bk_ending_ui_batch_destroy(s->ui_batch);
  s->ui_batch = NULL;
  bk_ending_ui_render_destroy(s->ui_stage_render);
  s->ui_stage_render = NULL;
  bk_ending_ui_render_destroy(s->ui_render);
  s->ui_render = NULL;
  bk_curtain_render_destroy(s->ui_curtain);
  s->ui_curtain = NULL;
  bk_ending_audio_destroy(s->audio);
  s->audio = NULL;
  bk_ending_normal_render_destroy(s->render);
  s->render = NULL;
  free(s->special_materials);
  s->special_materials = NULL;
  s->special_material_count = 0;
  if (s->assets)
    bk_material_pose_destroy(s->materials);
  s->materials = NULL;
  bk_ending_normal_assets_destroy(s->assets);
  s->assets = NULL;
  bk_ending_secondary_assets_destroy(s->secondary_assets);
  s->secondary_assets = NULL;
  bk_ending_tertiary_assets_destroy(s->tertiary_assets);
  s->tertiary_assets = NULL;
  bk_ending_selected_assets_destroy(s->selected_assets);
  s->selected_assets = NULL;
  bk_ending_auxiliary_assets_destroy(s->auxiliary_assets);
  s->auxiliary_assets = NULL;
  release_retired(&s->retired);
  s->snapshot_render = NULL;
  bk_ending_background_assets_destroy(s->retained_background);
  s->retained_background = NULL;
  memset(&s->retained_lights, 0, sizeof(s->retained_lights));
  s->has_retained_lights = s->final_image = s->snapshot_ui_only = 0;
  for (unsigned i = 0; i < 4; ++i) {
    bk_system_audio_destroy(s->control_audio[i]);
    s->control_audio[i] = NULL;
  }
  s->disabled_count = 0;
}

static int control_key(void *context, unsigned code, unsigned mode,
                       uint32_t *result, char e[256]) {
  EndingNormalScene *s = context;
  if (!s || !result || mode > 3)
    return fail(e, "invalid ending control key request");
  /* A/B emulate the primary left/right input only. Reusing A for the
   * native Z/0x33450 aliases would also trigger the independent cancel
   * query in the SAME frame as confirm. No keyboard alias is synthesized. */
  uint32_t button = code == 0 ? BK_BUTTON_CONFIRM
                               : code == 1 ? BK_BUTTON_BACK : 0;
  /* Native mode0 matches the idle-up input state, after the release edge. */
  *result = mode == 0 ? !((s->input.held | s->input.pressed |
                            s->input.released) & button)
            : mode == 1 ? !!(s->input.pressed & button)
            : mode == 2 ? !!(s->input.held & button)
                        : !!(s->input.released & button);
  return 1;
}

static int raw_key(void *context, unsigned code, uint32_t *result,
                    char e[256]) {
  EndingNormalScene *s = context;
  if (!s || !result)
    return fail(e, "invalid physical keyboard query");
  /* BkInput currently reports a gamepad and pointer, no keyboard device.
   * Do not synthesize a held Windows key from an unrelated gamepad button. */
  (void)code;
  *result = 0;
  return 1;
}

static int control_sound(void *context, unsigned slot, int32_t volume,
                         char e[256]) {
  EndingNormalScene *s = context;
  unsigned index = slot == 0 ? 0 : slot == 5 ? 1 : slot == 3 ? 2
                                                       : slot == 2 ? 3 : 4;
  (void)volume;
  if (!s || index >= 4 || !s->control_audio[index])
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

static int auxiliary_presentation_advance(
    void *context, BkEndingAuxiliaryPresentationActor actor, float seconds,
    char e[256]) {
  EndingNormalScene *s = context;
  if (!s || !s->auxiliary_assets)
    return fail(e, "auxiliary presentation asset owner is missing");
  unsigned index = actor == BK_ENDING_AUX_PRESENT_PRIMARY ? 0 : 3;
  return bk_ending_auxiliary_assets_advance(s->auxiliary_assets, index,
                                            seconds, e);
}
static int auxiliary_presentation_plain(
    void *context, BkEndingAuxiliaryPresentationActor actor, float seconds,
    BkClipPlainMode mode, char e[256]) {
  EndingNormalScene *s = context;
  if (!s || actor != BK_ENDING_AUX_PRESENT_PRIMARY || !s->auxiliary_assets)
    return fail(e, "auxiliary plain scheduler owner is missing");
  return bk_ending_auxiliary_assets_advance_plain(s->auxiliary_assets,
                                                   seconds, mode, e);
}
static int auxiliary_presentation_active(
    void *context, BkEndingAuxiliaryPresentationActor actor, int32_t *out,
    char e[256]) {
  EndingNormalScene *s = context;
  BkActorPose *pose = s && s->auxiliary_assets
      ? bk_ending_auxiliary_assets_pose(s->auxiliary_assets,
          actor == BK_ENDING_AUX_PRESENT_PRIMARY ? 0 : 3) : NULL;
  BkClipState state;
  if (!out || !pose || !bk_actor_pose_state(pose, &state))
    return fail(e, "auxiliary active clip is unavailable");
  *out = state.slot;
  return 1;
}
static int auxiliary_presentation_hide(void *context, uint32_t node,
                                       uint32_t hidden, char e[256]) {
  EndingNormalScene *s = context;
  if (!node) return 1;
  return s && s->auxiliary_assets && bk_actor_forest_visibility(
      bk_ending_auxiliary_assets_forest(s->auxiliary_assets), node, hidden, e);
}
static int auxiliary_presentation_material(void *context, const char *name,
                                           uint32_t hidden, float alpha,
                                           char e[256]) {
  EndingNormalScene *s = context;
  return s && s->auxiliary_assets
      ? bk_ending_auxiliary_assets_material_alpha(s->auxiliary_assets, name,
                                                   hidden, alpha, e)
      : fail(e, "auxiliary material owner is missing");
}
static int auxiliary_presentation_publish(void *context, char e[256]) {
  EndingNormalScene *s = context;
  return s && s->auxiliary_assets && bk_actor_forest_refresh(
      bk_ending_auxiliary_assets_forest(s->auxiliary_assets), e);
}
static int auxiliary_presentation_expression(void *context, int32_t value,
                                             char e[256]) {
  EndingNormalScene *s = context;
  BkFaceState *face = s && s->auxiliary_assets
      ? bk_ending_auxiliary_assets_face_state(s->auxiliary_assets) : NULL;
  if (!face) return fail(e, "auxiliary face owner is missing");
  return bk_face_request(face, value, s->now_ms, e);
}
static int auxiliary_presentation_eye_range(void *context, float minimum,
                                            float maximum, char e[256]) {
  EndingNormalScene *s = context;
  BkFaceState *face = s && s->auxiliary_assets
      ? bk_ending_auxiliary_assets_face_state(s->auxiliary_assets) : NULL;
  return face ? bk_face_eye_range(face, minimum, maximum, s->random, e)
              : fail(e, "auxiliary eye owner is missing");
}
static int auxiliary_presentation_gaze(void *context, float minimum,
                                       float maximum, char e[256]) {
  EndingNormalScene *s = context;
  BkEyeAssets *eyes = s && s->auxiliary_assets
      ? bk_ending_auxiliary_assets_eyes(s->auxiliary_assets) : NULL;
  const BkEyeBinding *binding = bk_eye_assets_binding(eyes);
  if (!s || !s->auxiliary_assets || !binding)
    return fail(e, "auxiliary gaze binding is missing");
  const float *world = bk_actor_forest_world(
      bk_ending_auxiliary_assets_forest(s->auxiliary_assets),
      bk_ending_auxiliary_assets_root(s->auxiliary_assets, 0));
  return world && bk_actor_pose_eyes(
      bk_ending_auxiliary_assets_pose(s->auxiliary_assets, 0), binding->frames,
      world, binding->texture_mode, bk_eye_assets_gaze_variant(eyes),
      minimum, maximum, e);
}
static int auxiliary_presentation_blink(void *context, uint32_t timestamp,
                                        char e[256]) {
  EndingNormalScene *s = context;
  BkFaceState *face = s && s->auxiliary_assets
      ? bk_ending_auxiliary_assets_face_state(s->auxiliary_assets) : NULL;
  BkFaceAssets *assets = s && s->auxiliary_assets
      ? bk_ending_auxiliary_assets_face(s->auxiliary_assets) : NULL;
  BkFaceCommands commands;
  if (!face || !assets) return fail(e, "auxiliary blink owner is missing");
  return bk_face_blink(face, timestamp, s->now_ms, s->random, &commands, e) &&
         bk_face_assets_apply(assets, &commands, e);
}
static int auxiliary_presentation_level(void *context, float *out,
                                        char e[256]) {
  EndingNormalScene *s = context;
  return s && s->audio && s->presentation && out
      ? bk_ending_audio_level(s->audio, 0, &s->presentation->voice, out, e)
      : fail(e, "auxiliary voice envelope is missing");
}
static int auxiliary_presentation_mouth(void *context, float value,
                                        uint32_t timestamp, char e[256]) {
  EndingNormalScene *s = context;
  BkFaceState *face = s && s->auxiliary_assets
      ? bk_ending_auxiliary_assets_face_state(s->auxiliary_assets) : NULL;
  BkFaceAssets *assets = s && s->auxiliary_assets
      ? bk_ending_auxiliary_assets_face(s->auxiliary_assets) : NULL;
  BkFaceCommands commands;
  (void)timestamp;
  if (!face || !assets) return fail(e, "auxiliary mouth owner is missing");
  return bk_face_mouth(face, value, s->now_ms, &commands, e) &&
         bk_face_assets_apply(assets, &commands, e);
}
static int auxiliary_presentation_step(EndingNormalScene *s, char e[256]) {
  if (!s || !s->auxiliary_assets || !s->audio || !s->presentation)
    return fail(e, "auxiliary presentation owners are unavailable");
  BkActorForest *forest = bk_ending_auxiliary_assets_forest(s->auxiliary_assets);
  uint32_t primary = bk_ending_auxiliary_assets_root(s->auxiliary_assets, 0);
  uint32_t background = bk_ending_auxiliary_assets_root(s->auxiliary_assets, 3);
  if (!forest || primary == BK_FRAME_NONE || background == BK_FRAME_NONE)
    return fail(e, "auxiliary presentation roots are missing");
  uint32_t hidden[3];
  for (unsigned i = 0; i < 3; ++i) {
    uint32_t frame = bk_ending_auxiliary_assets_visible_node(s->auxiliary_assets, i);
    hidden[i] = frame == BK_MODEL_NONE ? 0 :
        bk_actor_forest_node(forest, 0, frame);
  if (hidden[i] == BK_FRAME_NONE) return fail(e, "stale auxiliary hidden node");
  }
  uint32_t secondary_node = (uint32_t)s->state->retained.normal.word_719b40;
  BkEndingAuxiliaryPresentationBindings b = {
      &s->state->frame, &s->state->control, &s->state->auxiliary,
      bk_ending_auxiliary_assets_face_state(s->auxiliary_assets),
      &s->state->face_mode, &s->state->retained.stage3.word_6bbe48,
      &s->state->eye_lower, &s->state->retained.stage4.word_6c7f48,
      s->state->control.toggles, &primary, &background, hidden,
      &secondary_node};
  BkEndingAuxiliaryPresentationOps ops = {
      s, frame_clock, auxiliary_presentation_advance,
      auxiliary_presentation_plain, auxiliary_presentation_active,
      auxiliary_presentation_hide, auxiliary_presentation_material,
      auxiliary_presentation_publish, auxiliary_presentation_expression,
      auxiliary_presentation_eye_range, auxiliary_presentation_gaze,
      auxiliary_presentation_blink, auxiliary_presentation_level,
      auxiliary_presentation_mouth};
  return bk_ending_auxiliary_presentation_step(&b, s->active_seconds, &ops, e);
}

static int ui_position(void *context, float out[2], char e[256]) {
  EndingNormalScene *s = context;
  if (!s || !out || !isfinite(s->pointer.position[0]) ||
      !isfinite(s->pointer.position[1]))
    return fail(e, "invalid ending UI pointer");
  out[0] = s->pointer.position[0];
  out[1] = s->pointer.position[1];
  return 1;
}

static int ui_motion(void *context, float out[2], char e[256]) {
  EndingNormalScene *s = context;
  if (!s || !out || !isfinite(s->pointer.motion[0]) ||
      !isfinite(s->pointer.motion[1]))
    return fail(e, "invalid ending UI pointer motion");
  out[0] = s->pointer.motion[0];
  out[1] = s->pointer.motion[1];
  return 1;
}

static uint32_t ui_button(unsigned code) {
  /* The UI's held-button queries select camera cursors1/2 and keep the
   * toolbar from sliding into a camera drag. Match the manual camera's L/R
   * modifiers; confirm/cancel and gameplay clicks still use control_key. */
  if (code == 0)
    return BK_BUTTON_CAMERA_ORBIT;
  if (code == 1)
    return BK_BUTTON_CAMERA_ADJUST;
  if (code == 0x70)
    return BK_BUTTON_BACK;
  return 0;
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
  return bk_ending_audio_call(s->audio, s->state->frame.group,
                              s->state->auxiliary.variant,
                              s->state->auxiliary.selection, &call, playing, e);
}

typedef struct {
  EndingNormalScene *scene;
} EndingUiTailContext;

static int ui_tail_active(void *context, int32_t *slot, char e[256]) {
  EndingUiTailContext *c = context;
  BkClipState state;
  if (!c || !c->scene || !slot ||
      !bk_actor_pose_state(scene_primary(c->scene), &state))
    return fail(e, "ending UI active clip is unavailable");
  *slot = (int32_t)state.slot;
  return 1;
}

static int ui_tail_write(void *context, unsigned slot, BkEndingClipWrite kind,
                         int32_t value, char e[256]) {
  EndingUiTailContext *c = context;
  BkActorPose *primary;
  BkClipEdit edit = {.slot = slot};
  if (!c || !c->scene || !(primary = scene_primary(c->scene)))
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
      c ? scene_primary(c->scene) : NULL;
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
  return bk_ending_audio_call(s->audio, s->state->frame.group,
                              s->state->auxiliary.variant,
                              s->state->auxiliary.selection, call, playing, e);
}

static int ui_tail_eyes(void *context, unsigned slot, char e[256]) {
  EndingUiTailContext *c = context;
  BkEyeAssets *eyes = c ? scene_eyes(c->scene) : NULL;
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
  EndingNormalScene *s = context;
  if (!s || (!s->retired.render && !s->retained_background) ||
      s->final_image || s->render || !s->audio ||
      !s->ui_render || !s->ui_batch || !s->ui_curtain)
    return fail(e, "stage reload requires released assets and live common owners");
  return normal_load(s, loader, argument, e);
}

static int ui_reload_release(void *context, BkEndingLoader loader, char e[256]) {
  EndingNormalScene *s = context;
  int secondary = loader == BK_ENDING_LOAD_4D00FA;
  int tertiary = loader == BK_ENDING_LOAD_4D2320;
  int selected = loader == BK_ENDING_LOAD_4D1025;
  int auxiliary = loader == BK_ENDING_LOAD_4D39E6;
  if (!s || s->retired.render || !s->render ||
      s->snapshot_render != s->render ||
      (tertiary ? !s->tertiary_assets : auxiliary ? !s->auxiliary_assets : selected ? !s->selected_assets :
       secondary ? !s->secondary_assets :
         (loader != BK_ENDING_LOAD_4CF318 || !s->assets)))
    return fail(e, "unsupported release or stage snapshot is not captured");
  if (secondary) {
    s->state->retained.normal.word_719b40 = 0;
    s->state->retained.normal.follow_target = 0;
  }
  if (!bk_ending_audio_release_speech(s->audio, e))
    return 0;
  if (!bk_ending_stage_ui_release(&s->ui, &s->stage_ui,
        tertiary ? BK_ENDING_UI_THIRD : auxiliary ? BK_ENDING_UI_AUXILIARY : selected ? BK_ENDING_UI_FOURTH :
        secondary ? BK_ENDING_UI_SECONDARY : BK_ENDING_UI_NORMAL, e))
    return 0;
  s->retired = (EndingRetiredStage){.normal = s->assets, .secondary = s->secondary_assets,
      .tertiary = s->tertiary_assets, .selected = s->selected_assets,
      .auxiliary = s->auxiliary_assets, .render = s->render,
      .materials = s->materials, .special_materials = s->special_materials};
  s->assets = NULL;
  s->secondary_assets = NULL;
  s->tertiary_assets = NULL;
  s->selected_assets = NULL;
  s->auxiliary_assets = NULL;
  s->render = NULL;
  s->materials = NULL;
  s->special_materials = NULL;
  s->special_material_count = s->disabled_count = 0;
  bk_ending_ui_render_destroy(s->ui_stage_render);
  s->ui_stage_render = NULL; /*The current UI batch pins the old images.*/
  s->state->retained.normal.follow_target = 0;
  s->state->retained.normal.direct_node = 0;
  s->state->retained.normal.direct_reference = 0;
  s->state->frame.camera_cached = -1;
  s->state->frame.camera_request = 1;
  s->state->frame.camera_mode = 5;
  return 1;
}

static int ui_reload_final_image(void *context, int create, unsigned group,
                                 char e[256]) {
  EndingNormalScene *s = context;
  /*The native release callback has no group argument; the dispatcher passes
   * zero as a placeholder. Only creation selects a group's image. */
  if (!s || (create != 0 && create != 1) ||
      (create && group != s->state->frame.group))
    return fail(e, "invalid final image owner/group");
  if (!create) {
    if (!s->final_image || !s->ui_stage_render || !s->retained_background ||
        !s->has_retained_lights ||
        !bk_ending_stage_ui_release(&s->ui, &s->stage_ui, BK_ENDING_UI_FINAL, e))
      return fail(e, "final image is not loaded");
    bk_ending_ui_render_destroy(s->ui_stage_render);
    s->ui_stage_render = NULL; /*The captured batch pins the last image.*/
    s->final_image = 0;
    return 1;
  }
  if (s->final_image || s->ui_stage_render || s->render || scene_forest(s) ||
      !s->retired.render || s->retained_background || s->has_retained_lights)
    return fail(e, "final image requires the released 3D stage");
  BkEndingBackgroundAssets *background = scene_retired_background(s);
  const BkEndingBackgroundData *data = bk_ending_background_assets_data(background);
  BkRetainedLightState lights;
  if (!data || !bk_ending_normal_render_save_lighting(
                    s->retired.render, data->model, &lights, e))
    return fail(e, "final image cannot preserve the outer background lights");
  BkEndingUi base = s->ui;
  BkEndingStageUi stage = s->stage_ui;
  if (!bk_ending_stage_ui_initialize(&base, &stage, BK_ENDING_UI_FINAL,
        group, s->variant, s->viewport.width, e))
    return 0;
  BkEndingUiRender *render = bk_ending_ui_render_create_stage(
      s->services.renderer, s->services.resources, &stage, e);
  if (!render)
    return 0;
  s->retained_background = bk_ending_background_assets_retain(background);
  if (!s->retained_background) {
    bk_ending_ui_render_destroy(render);
    return fail(e, "outer background reference limit");
  }
  s->retained_lights = lights;
  s->has_retained_lights = 1;
  s->ui = base;
  s->stage_ui = stage;
  s->ui_stage_render = render;
  s->final_image = 1;
  return 1;
}

static int ui_reload_leave(void *context, BkEndingLeave leave, char e[256]) {
  EndingNormalScene *s = context;
  return s ? bk_ending_state_leave(s->state, leave, e)
           : fail(e, "ending retained-state owner is missing");
}

static int ui_reload_lighting(void *context, BkEndingReloadLight operation,
                              char e[256]) {
  EndingNormalScene *s = context;
  if (!s)
    return fail(e, "missing ending light registry owner");
  BkEndingNormalRender *render = s->render;
  if (!render && operation == BK_ENDING_LIGHT_RESET)
    render = s->retired.render;
  /*A released pure-UI stage has no registration arrays. Its retained
   * device values survive RESET, just as4a4140 preserves those values.*/
  if (!render && operation == BK_ENDING_LIGHT_RESET &&
      s->retained_background && s->has_retained_lights)
    return 1;
  return bk_ending_normal_render_relight(render, operation, e);
}

static int ui_reload_schedule(void *context, uint8_t target, uint8_t mode,
                              char e[256]) {
  EndingNormalScene *s = context;
  if (!s || !s->flow.schedule || s->flow.common != s->common)
    return fail(e, "ending application flow scheduler is not bound");
  return s->flow.schedule(s->flow.context, target, mode, e);
}

static int ui_reload_present(void *context, unsigned slot, int *present,
                             char e[256]) {
  EndingNormalScene *s = context;
  if (!s || !s->audio || !present)
    return fail(e, "ending reload audio owner is missing");
  return bk_ending_audio_present(s->audio, slot, present)
             ? 1
             : fail(e, "ending reload audio slot is unavailable");
}

static int ui_reload_status(void *context, unsigned slot, int *playing,
                            char e[256]) {
  EndingNormalScene *s = context;
  BkEndingAudioCall call = {.operation = BK_ENDING_AUDIO_STATUS, .slot = slot};
  if (!s || !s->audio || !playing || slot >= BK_ENDING_SOUND_BUFFERS)
    return fail(e, "ending reload audio slot is unavailable");
  return bk_ending_audio_call(s->audio, s->state->frame.group,
                              s->state->auxiliary.variant,
                              s->state->auxiliary.selection, &call, playing, e);
}

static int ui_reload_pause(void *context, unsigned slot, char e[256]) {
  EndingNormalScene *s = context;
  BkEndingAudioCall call = {.operation = BK_ENDING_AUDIO_PAUSE, .slot = slot};
  int playing = 0;
  if (!s || !s->audio || slot >= BK_ENDING_SOUND_BUFFERS)
    return fail(e, "ending reload pause slot is unavailable");
  return bk_ending_audio_call(s->audio, s->state->frame.group,
                              s->state->auxiliary.variant,
                              s->state->auxiliary.selection, &call, &playing, e);
}

static int prepare_ui_geometry(EndingNormalScene *s, unsigned width,
                               unsigned height, char e[256]) {
  BkActorForest *forest;
  BkActorPose *primary;
  BkNodeReference camera_node;
  BkClipState clip;
  const float *view;
  if (!s || !(forest = scene_forest(s)) ||
      !(primary = scene_primary(s)) ||
      !width || !height || !bk_actor_pose_state(primary, &clip) ||
      !bk_actor_pose_timing(primary, clip.slot, &s->ui_active_timing) ||
      !bk_actor_forest_anchor_reference(forest, 1, &camera_node, e) ||
      !(view = bk_actor_forest_view(forest)))
    return fail(e, "ending UI geometry owner is unavailable");
  /*4da3ca/4da4f7 read the active camera645604 local, not the animation
   * track's root. The track can remain held while the user rotates the view. */
  memcpy(s->ui_camera_local, camera_node.local, sizeof(s->ui_camera_local));
  s->ui_active_clip = (int32_t)clip.slot;
  memcpy(s->ui_view, view, sizeof(s->ui_view));
  if (!bk_camera_projection(
          s->ui_projection,
          &(BkCameraLens){s->camera.fov, .75f, .5f, 126384}))
    return fail(e, "ending UI projection is invalid");
  memset(s->ui_viewport, 0, sizeof(s->ui_viewport));
  s->ui_viewport[0] = (float)width * .5f;
  /* Projection already flips Y for Vulkan. Positive viewport Y matches the
   * pixels drawn; applying the D3D Y flip again mirrors target icons/hits. */
  s->ui_viewport[5] = (float)height * .5f;
  s->ui_viewport[10] = s->ui_viewport[15] = 1;
  s->ui_viewport[12] = (float)width * .5f;
  s->ui_viewport[13] = (float)height * .5f;
  memcpy(s->ui_camera_position, s->camera.pose.position,
         sizeof(s->ui_camera_position));
  memset(s->ui_present, 0, sizeof(s->ui_present));
  for (unsigned i = 0; i < BK_ENDING_NORMAL_NODES; ++i) {
    uint32_t frame = scene_node(s, i);
    if (frame == BK_MODEL_NONE)
      continue;
    uint32_t node = bk_actor_forest_node(forest, scene_registry(s, 0), frame);
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
      (!s->final_image && !prepare_ui_geometry(s, width, height, e)) ||
      !bk_ending_state_import_frame_aliases(s->state, s->common,
                                            &s->stage_ui))
    return fail(e, "ending UI frame preparation failed");
  views = (BkEndingStateUiViews){
      s->final_image ? NULL : &s->ui_active_clip,
      s->final_image ? NULL : &s->ui_active_timing,
      s->tertiary_assets ? bk_ending_tertiary_assets_config(s->tertiary_assets)->actions :
          s->assets ? bk_ending_normal_assets_config(s->assets)->actions : NULL,
      s->final_image ? NULL : s->ui_camera_local,
      s->final_image ? NULL : &s->ui_pick,
      &s->ui_notices,
      &s->ui_flash_wanted,
      &s->ui_item,
      &s->previous_flow,
      &s->voice_volume,
      s->random};
  if (!bk_ending_state_ui_bindings(s->state, s->common, &views, &bindings))
    return fail(e, "ending UI live bindings are unavailable");
  float scale = (float)((double)width / 1280.0);
  if (!bk_ending_ui_frame(&s->ui, &s->stage_ui, &s->state->ui_controller,
                          &bindings, &ops, scale, seconds, &s->ui_frame, e) ||
      !bk_ending_state_import_frame_aliases(s->state, s->common,
                                            &s->stage_ui) ||
      !bk_ending_ui_batch_prepare(s->ui_batch, &s->ui_frame, s->ui_render,
                                  s->ui_stage_render, width, height, e))
    return 0;
  return 1;
}

static int ending_target_node(EndingNormalScene *s, unsigned index,
                              uint32_t *node,
                              char e[256]) {
  uint32_t frame = scene_node(s, index);
  if (frame == BK_MODEL_NONE)
    return fail(e, "ending camera target is missing");
  *node = bk_actor_forest_node(scene_forest(s), scene_registry(s, 0), frame);
  return *node != BK_FRAME_NONE
             ? 1
             : fail(e, "ending camera target is not attached");
}

static const float *ending_cached_target(EndingNormalScene *s,
                                         unsigned index) {
  BkActorForest *forest = scene_forest(s);
  uint32_t frame = scene_node(s, index);
  if (!forest || frame == BK_MODEL_NONE)
    return NULL;
  const float *world = bk_actor_forest_world(
      forest, bk_actor_forest_node(forest, 0, frame));
  return world ? world + 12 : NULL;
}

static int auxiliary_state4_present(void *context, unsigned owner, int *present,
                                    char e[256]) {
  EndingNormalScene *s = context;
  if (!s || !s->audio || owner >= 2 || !present)
    return fail(e, "auxiliary state4 media owner is unavailable");
  return bk_ending_audio_present(s->audio, owner, present)
             ? 1
             : fail(e, "auxiliary state4 media slot is unavailable");
}

static int auxiliary_state3_child(void *context,
                                  const BkEndingFrameInput *input,
                                  char e[256]);

static int auxiliary_state3_hit(void *context, const float center[2],
                                float radius, const float pointer[2],
                                int *hit, char e[256]) {
  EndingNormalScene *s = context;
  float distance = 0;
  if (!s || !center || !pointer || !hit ||
      !bk_ending_ui_circle_hit(center, radius, pointer, hit, &distance))
    return fail(e, "auxiliary state3 circle owner is unavailable");
  return 1;
}

static int auxiliary_state4_status(void *context, unsigned owner, int *playing,
                                   char e[256]) {
  EndingNormalScene *s = context;
  BkEndingAudioCall call = {.operation = BK_ENDING_AUDIO_STATUS,
                             .slot = owner};
  if (!s || !s->audio || owner >= 2 || !playing)
    return fail(e, "auxiliary state4 media status is unavailable");
  return bk_ending_audio_call(s->audio, s->state->frame.group,
                              s->state->auxiliary.variant,
                              s->state->auxiliary.selection, &call, playing, e);
}

static int auxiliary_state4_expression(void *context, int32_t a, int32_t b,
                                       unsigned eye, char e[256]) {
  EndingNormalScene *s = context;
  BkFaceState *face = s && s->auxiliary_assets
                          ? bk_ending_auxiliary_assets_face_state(
                                s->auxiliary_assets)
                          : NULL;
  BkEyeAssets *eyes = s && s->auxiliary_assets
                          ? bk_ending_auxiliary_assets_eyes(s->auxiliary_assets)
                          : NULL;
  (void)b;
  if (!face || !eyes)
    return fail(e, "auxiliary state4 face/eye owner is unavailable");
  return bk_face_request(face, a, s->now_ms, e) &&
         bk_eye_assets_select(eyes, eye, e);
}

static int auxiliary_state4_target(void *context, float position[3],
                                   char e[256]) {
  EndingNormalScene *s = context;
  uint32_t node;
  const float *world;
  if (!s || !position || !ending_target_node(s, 5, &node, e) ||
      !(world = bk_actor_forest_world(scene_forest(s), node)))
    return fail(e, "auxiliary state4 camera target is unavailable");
  memcpy(position, world + 12, sizeof(float) * 3);
  return 1;
}

static int auxiliary_state4_camera(void *context, BkEndingOpeningCamera kind,
                                   int32_t choice, const uint32_t offset[3],
                                   uint32_t extra, uint32_t *result,
                                   char e[256]) {
  EndingNormalScene *s = context;
  const uint32_t tracks[2] = {scene_registry(s, 3), scene_registry(s, 4)};
  BkEndingCameraAssets *assets = scene_cameras(s);
  BkActorForest *forest = scene_forest(s);
  uint32_t target_node;
  int complete = 0;
  if (!s || !assets || !forest || !result || !offset || choice < 0 ||
      choice >= 4 || !ending_target_node(s, 5, &target_node, e))
    return fail(e, "auxiliary state4 camera owner is unavailable");
  if (kind == BK_ENDING_OPENING_TRACK) {
    BkEndingCameraOpeningInput input = {s, control_key};
    if (!bk_ending_camera_assets_opening(
            assets, forest, tracks, &s->camera,
            bk_actor_forest_node(forest, scene_registry(s, 0),
                                 bk_ending_auxiliary_assets_follow(
                                     s->auxiliary_assets)),
            s->active_seconds, &input, &complete, e))
      return 0;
  } else {
    float offset_f[3];
    memcpy(offset_f, offset, sizeof(offset_f));
    BkEndingCameraPresetGate gate = {
        (uint8_t)s->previous_flow,
        s->state->frame.phase,
        s->state->selected,
        s->state->frame.state_721ee0,
        s->state->frame.state_721ee4,
        s->state->control.state_721eec,
        s->state->auxiliary.gate,
        s->state->next_mode};
    (void)extra;
    if (!bk_ending_camera_assets_preset(
            assets, forest, tracks, &s->camera, &s->camera_transitions,
            &s->presets, BK_ENDING_PRESET, (unsigned)choice, offset_f, &gate,
            0x10, s->active_seconds, &complete, e))
      return 0;
  }
  (void)target_node;
  *result = (uint32_t)complete;
  return 1;
}

static int auxiliary_state4_effect(void *context, unsigned a, unsigned b,
                                   unsigned c, char e[256]) {
  EndingNormalScene *s = context;
  if (!s || !s->auxiliary_assets ||
      !((a == 1 && b == 0 && c == 0) || (a == 2 && b == 1 && c == 0)))
    return fail(e, "unsupported auxiliary state4 effect");
  /* 6C7F4C is the retained effect selector cleared by 482F91. Keep the
   * selector in the process owner until the later presentation consumes it. */
  s->state->retained.stage4.word_6c7f4c = (int32_t)a;
  return 1;
}

static int auxiliary_state8_group_sound(void *context, unsigned group,
                                        char e[256]) {
  EndingNormalScene *s = context;
  static const unsigned effects[2] = {10, 29};
  if (!s || !s->audio || group >= 2)
    return fail(e, "auxiliary state8 group sound is unavailable");
  BkEndingAudioCall call = {.operation = BK_ENDING_AUDIO_RESTART,
                            .slot = 2 + effects[group],
                            .flags = 0,
                            .volume = s->effect_volume};
  int playing = 0;
  return bk_ending_audio_call(s->audio, s->state->frame.group,
                              s->state->auxiliary.variant,
                              s->state->auxiliary.selection, &call, &playing,
                              e);
}

static int auxiliary_state8_camera_setup(void *context, unsigned group,
                                         const float values[4], int preset,
                                         char e[256]) {
  EndingNormalScene *s = context;
  if (!s || !values || group != s->state->frame.group || group >= 5 ||
      (preset != 0 && preset != 1))
    return fail(e, "auxiliary state8 camera owner is unavailable");
  s->camera.yaw = values[0];
  s->camera.pitch = values[1];
  s->camera.radius = values[2];
  s->camera.height = values[3];
  if (preset) {
    for (unsigned i = 0; i < 4; ++i)
      s->presets.active[i][0] = values[i];
  }
  return 1;
}

static int auxiliary_state8_target(void *context, float position[3],
                                   char e[256]) {
  if (!auxiliary_state4_target(context, position, e)) {
    if (e) snprintf(e, 256, "auxiliary state8 target unavailable");
    return 0;
  }
  return 1;
}

static int auxiliary_state1_audio(void *context, const BkEndingAudioCall *call,
                                  int *playing, char e[256]) {
  EndingNormalScene *s = context;
  if (!s || !s->audio || !call || !playing)
    return fail(e, "auxiliary state1 audio owner is unavailable");
  return bk_ending_audio_call(s->audio, s->state->frame.group,
                              s->state->auxiliary.variant,
                              s->state->auxiliary.selection, call, playing, e);
}

static int auxiliary_state1_random(void *context, int32_t *result,
                                   char e[256]) {
  EndingNormalScene *s = context;
  if (!s || !s->random || !result)
    return fail(e, "auxiliary state1 RNG owner is unavailable");
  *result = (int32_t)bk_random_next(s->random);
  return 1;
}

static int auxiliary_state1_pick(void *context, const float pointer[2],
                                 int32_t preferred, int32_t *result,
                                 char e[256]) {
  EndingNormalScene *s = context;
  BkActorForest *forest = scene_forest(s);
  uint32_t node = s ? (uint32_t)s->state->retained.normal.word_719b40 : 0;
  const float *alternate = node ? bk_actor_forest_world(forest, node) : NULL;
  if (!s || !pointer || !result || (node && !alternate))
    return fail(e, "auxiliary state1 pick owner is unavailable");
  BkEndingSecondaryPickBindings bindings = {
      &s->state->frame, NULL, NULL, s->state->targets, s->state->alternate,
      &s->ui.sprites[50], &s->ui_pick, alternate};
  return bk_ending_secondary_pick(&bindings, pointer, preferred, result, e);
}

static int auxiliary_state1_menu(void *context, int32_t selected, int32_t *out,
                                 char e[256]) {
  EndingNormalScene *s = context;
  if (!s || !out || selected < 0 || selected >= 39)
    return fail(e, "auxiliary state1 menu target is unavailable");
  BkEndingSecondaryMenuGeometry geometry = {
      s->viewport.width, s->viewport.height,
      (float)((double)s->viewport.width / 1280.0), s->ui.sprites[51].rect[2]};
  int32_t zone = -1;
  if (!bk_ending_secondary_menu_zone(&geometry, s->state->targets[selected],
                                     &zone, e))
    return 0;
  *out = zone;
  if (zone < 0)
    return 1;
  s->state->choices[0] = 0;
  s->state->choices[1] = s->state->choices[2] = -1;
  static const int32_t base[9] = {180, 225, 45, 135, 315,
                                  270, 90, 180, 0};
  if (zone >= 0 &&
      !bk_ending_radial_menu_place(&geometry, base[zone], 90,
                                   s->state->targets[selected],
                                   s->state->points, e))
    return 0;
  s->state->retained.stage4.word_54e2f8 = zone;
  return 1;
}

static int auxiliary_state1_voice(void *context, int32_t cue, unsigned slot,
                                  int32_t flags, int32_t volume, char e[256]) {
  EndingNormalScene *s = context;
  if (!s || !s->audio || flags != 0 || slot > 1 || cue < 0 || cue > 99)
    return fail(e, "auxiliary state1 voice request is unavailable");
  if (snprintf(s->state->speech_names[slot],
               sizeof(s->state->speech_names[slot]), "PH%u33%02d.wav",
               s->state->frame.group + 1, cue) < 0)
    return fail(e, "auxiliary state1 voice name formatting failed");
  return bk_ending_audio_auxiliary_voice(s->audio, s->state->frame.group,
                                         cue, slot, volume, e);
}

static int auxiliary_state1_request(void *context, unsigned slot,
                                    char e[256]) {
  EndingNormalScene *s = context;
  BkActorPose *primary = scene_primary(s);
  if (!primary || slot >= 32)
    return fail(e, "auxiliary state1 clip owner is unavailable");
  return bk_actor_pose_request_mode(primary, slot, BK_CLIP_REQUEST_CONFIGURED,
                                    e);
}

static int auxiliary_state3_active(void *context, int32_t *slot,
                                   char e[256]) {
  EndingNormalScene *s = context;
  BkClipState state;
  if (!s || !slot || !bk_actor_pose_state(scene_primary(s), &state))
    return fail(e, "auxiliary state3 active clip is unavailable");
  *slot = state.slot;
  return 1;
}

static int auxiliary_state3_source(void *context, unsigned slot, float source,
                                   char e[256]) {
  EndingNormalScene *s = context;
  if (!s || !s->auxiliary_assets || slot >= BK_CLIP_SLOTS ||
      !isfinite(source))
    return fail(e, "auxiliary state3 source owner is unavailable");
  BkActorPose *primary = scene_primary(s);
  if (!primary)
    return fail(e, "auxiliary state3 source actor is unavailable");
  BkClipEdit edit = {.slot = slot,
                     .fields = BK_CLIP_EDIT_SOURCE,
                     .source = source};
  return bk_actor_pose_edit_clips(primary, &edit, 1, e);
}

static int auxiliary_state3_rewind(void *context, unsigned slot, char e[256]) {
  EndingNormalScene *s = context;
  BkActorPose *primary = s ? scene_primary(s) : NULL;
  if (!primary || slot >= BK_CLIP_SLOTS)
    return fail(e, "auxiliary state3 source rewind owner is unavailable");
  return bk_actor_pose_reset_sources(primary, &slot, 1, e);
}

static int auxiliary_state3_random(void *context, int32_t *value,
                                   char e[256]) {
  EndingNormalScene *s = context;
  if (!s || !s->random || !value)
    return fail(e, "auxiliary state3 RNG owner is unavailable");
  *value = (int32_t)bk_random_next(s->random);
  return 1;
}

static int auxiliary_state3_effect(void *context, unsigned effect,
                                   unsigned flags, int32_t volume,
                                   char e[256]) {
  EndingNormalScene *s = context;
  if (!s || !s->audio || effect >= BK_ENDING_EFFECTS || flags > 1 ||
      volume < -10000 || volume > 0)
    return fail(e, "auxiliary state3 effect is unavailable");
  BkEndingAudioCall call = {.operation = BK_ENDING_AUDIO_RESTART,
                            .slot = 2 + effect,
                            .flags = (int32_t)flags,
                            .volume = volume};
  int playing = 0;
  return bk_ending_audio_call(s->audio, s->state->frame.group,
                              s->state->auxiliary.variant,
                              s->state->auxiliary.selection, &call, &playing,
                              e);
}

static int auxiliary_state3_effect_stop(void *context, unsigned effect,
                                        char e[256]) {
  EndingNormalScene *s = context;
  if (!s || !s->audio || effect >= BK_ENDING_EFFECTS)
    return fail(e, "auxiliary state3 effect stop is unavailable");
  BkEndingAudioCall call = {.operation = BK_ENDING_AUDIO_PAUSE,
                            .slot = 2 + effect};
  int playing = 0;
  return bk_ending_audio_call(s->audio, s->state->frame.group,
                              s->state->auxiliary.variant,
                              s->state->auxiliary.selection, &call, &playing,
                              e);
}

static int auxiliary_state3_timing(void *context, unsigned slot,
                                   BkClipTiming *timing, char e[256]) {
  EndingNormalScene *s = context;
  if (!s || !timing || slot >= BK_CLIP_SLOTS ||
      !bk_actor_pose_timing(scene_primary(s), slot, timing))
    return fail(e, "auxiliary state3 clip timing is unavailable");
  return 1;
}

static int auxiliary_state3_effect_present(void *context, unsigned effect,
                                           int *present, char e[256]) {
  EndingNormalScene *s = context;
  if (!s || !s->audio || !present || effect >= BK_ENDING_EFFECTS ||
      !bk_ending_audio_present(s->audio, 2 + effect, present))
    return fail(e, "auxiliary state3 effect presence is unavailable");
  return 1;
}

static int auxiliary_state3_effect_status(void *context, unsigned effect,
                                           int *playing, char e[256]) {
  EndingNormalScene *s = context;
  if (!s || !s->audio || !playing || effect >= BK_ENDING_EFFECTS)
    return fail(e, "auxiliary state3 effect status is unavailable");
  BkEndingAudioCall call = {.operation = BK_ENDING_AUDIO_STATUS,
                            .slot = 2 + effect};
  return bk_ending_audio_call(s->audio, s->state->frame.group,
                              s->state->auxiliary.variant,
                              s->state->auxiliary.selection, &call, playing,
                              e);
}

static int auxiliary_state3_child(void *context,
                                  const BkEndingFrameInput *input,
                                  char e[256]) {
  EndingNormalScene *s = context;
  if (!s || !s->auxiliary_assets)
    return fail(e, "auxiliary state3 child assets are unavailable");
  BkEndingAuxiliaryChildBindings bindings = {
      &s->state->frame,
      &s->state->control,
      &s->state->auxiliary,
      s->state->points[0],
      s->state->targets,
      &s->ui.sprites[51].rect[2],
      &s->state->retained.stage4.word_54e2f8,
      &s->state->retained.stage4.words_6c7f44[0],
      &s->state->retained.stage4.bytes_6c7f60[0]};
  BkEndingAuxiliaryChildOps ops = {
      s,
      auxiliary_state3_active,
      auxiliary_state3_source,
      auxiliary_state3_rewind,
      auxiliary_state4_present,
      auxiliary_state4_status,
      auxiliary_state3_hit,
      auxiliary_state1_request,
      auxiliary_state4_expression,
      auxiliary_state1_voice,
      auxiliary_state3_random,
      auxiliary_state3_timing,
      auxiliary_state3_effect_present,
      auxiliary_state3_effect_status,
      auxiliary_state3_effect,
      auxiliary_state3_effect_stop,
      s->voice_volume,
      s->effect_volume};
  return bk_ending_auxiliary_state3_child_step(&bindings, input, &ops, e);
}

static int selected_key(void *context, unsigned code, unsigned mode,
                        uint32_t *result, char e[256]);
static int selected_raw_key(void *context, unsigned code, uint32_t *result,
                            char e[256]);
static int selected_audio(void *context, const BkEndingAudioCall *call,
                          int *playing, char e[256]);
static int selected_load(void *context, unsigned slot, const char *name,
                         char e[256]);
static int selected_expression(void *context, int32_t a, int32_t b,
                               int32_t mode, char e[256]);
static int selected_active(void *context, int32_t *slot, char e[256]);
static int selected_request(void *context, unsigned slot, char e[256]);
static int selected_write(void *context, unsigned slot, BkEndingClipWrite kind,
                          int32_t value, char e[256]);
static int selected_target(void *context, unsigned node, float position[3],
                           char e[256]);
static int selected_camera(void *context, BkEndingOpeningCamera kind,
                           int32_t choice, const uint32_t offset[3],
                           uint32_t extra, uint32_t *result, char e[256]);
static int selected_manual(void *context, int32_t proposed, int32_t *accepted,
                           char e[256]);
static int selected_pick(void *context, const float pointer[2], int32_t *result,
                         char e[256]);
static int selected_choose(void *context, const int32_t point[2], int32_t *result,
                           char e[256]);
static int selected_begin(void *context, char e[256]);
static int selected_action(void *context, const BkEndingFrameInput *input,
                           float seconds, char e[256]);
static int selected_random(void *context, int32_t *result, char e[256]);
static int selected_clip(void *context, unsigned slot,
                         BkEndingSelectedMotionClip *out, char e[256]);
static int selected_source(void *context, unsigned slot, float source,
                           char e[256]);
static int selected_pointer(void *context, const BkEndingFrameInput *input,
                            char e[256]);
static int selected_drag(void *context, const float motion[2], float *result,
                         char e[256]);
static int selected_hit(void *context, unsigned menu, const int32_t point[2],
                        int *result, char e[256]);
static int selected_clock(void *context, uint32_t *milliseconds, char e[256]);
static int selected_stop(void *context, unsigned slot, char e[256]);
static int selected_repeat(void *context, unsigned slot, char e[256]);
static int selected_ui_byte(void *context, unsigned slot,
                            BkEndingSelectedUiByte field, uint8_t value,
                            char e[256]);
static int selected_ui_uv_reset(void *context, unsigned slot, char e[256]);
static int selected_ui_fade(void *context, unsigned slot, float alpha,
                            char e[256]);

static BkEndingSelectedControlBindings selected_control_bindings(
    EndingNormalScene *s) {
  BkEndingUiAuxNotice *ui = &s->state->ui_controller.auxiliary;
  return (BkEndingSelectedControlBindings){
      .frame = &s->state->frame,
      .control = &s->state->control,
      .auxiliary = &s->state->auxiliary,
      .camera = &s->camera,
      .presets = &s->presets,
      .substate = &s->state->retained.auxiliary.byte_6ea358,
      .mode = &ui->mode,
      .choice = &ui->choice,
      .reset_c = &ui->reset_c,
      .configuration = s->state->aux_config,
      .camera_words = s->state->retained.auxiliary.words_6ea028,
      .group_prefix = s->state->retained.auxiliary.group_prefix,
      .voice_latches = s->state->retained.auxiliary.words_6ea2c0,
      .inputs = s->state->aux_inputs,
      .processed = ui->processed,
      .selected = &s->state->selected,
      .open = &s->state->open,
      .gauge_y = &s->state->gauge_y,
      .scale = &s->auxiliary_cycle->scale,
      .plain_scheduled = &s->selected_plain_scheduled,
      .previous_flow = &s->previous_flow,
      .targets = s->state->targets,
      .alternate = s->state->alternate,
      .voice_volume = &s->voice_volume,
      .effect_volume = &s->effect_volume,
      .speech_name = s->state->speech_names[0]};
}

static BkEndingSelectedControlOps selected_control_ops(void) {
  return (BkEndingSelectedControlOps){
      .context = NULL, .key = selected_key, .raw_key = selected_raw_key,
      .audio = selected_audio, .load = selected_load,
      .expression = selected_expression, .active = selected_active,
      .request = selected_request, .write = selected_write,
      .target = selected_target, .camera = selected_camera,
      .manual = selected_manual, .pick = selected_pick,
      .choose = selected_choose, .begin = selected_begin,
      .action = selected_action, .random = selected_random};
}

static BkEndingSelectedActionBindings selected_action_bindings(
    EndingNormalScene *s, BkEndingSelectedControlBindings control) {
  BkEndingUiAuxNotice *ui = &s->state->ui_controller.auxiliary;
  BkEndingRetainedAuxiliary *a = &s->state->retained.auxiliary;
  return (BkEndingSelectedActionBindings){
      .control = control,
      .controller = s->selected_controller,
      .presets = &s->presets,
      .selected = &s->state->selected,
      .records = s->records,
      .working = s->state->working,
      .face_mode = &s->state->face_mode,
      .once = &ui->once,
      .random_latch = &ui->random_latch,
      .reset_a = &ui->reset_a,
      .small_motion = &a->word_6dde90,
      .large_motion = &a->word_6ddec8,
      .fast_motion = &a->word_6ea16c,
      .reverse = &a->word_6ea314,
      .previous_sound = &a->word_5546a0,
      .stage = &a->word_6ea348,
      .reset_340 = &a->word_6ea340,
      .reset_354 = &a->word_6ea354,
      .words_318 = a->words_6ea318,
      .group_seen = ui->group_seen,
      .group_suffix = a->group_suffix,
      .sequence_elapsed = &ui->sequence_elapsed,
      .sequence = &ui->sequence,
      .animation_scale = &s->auxiliary_cycle->scale};
}

static BkEndingSelectedActionOps selected_action_ops(void) {
  BkEndingSelectedActionOps out = {
      .control = selected_control_ops(), .clip = selected_clip,
      .source = selected_source, .pointer = selected_pointer,
      .drag = selected_drag, .hit = selected_hit, .choices = NULL,
      .clock = selected_clock, .stop = selected_stop, .repeat = selected_repeat,
      .ui_byte = selected_ui_byte, .ui_uv_reset = selected_ui_uv_reset,
      .ui_fade = selected_ui_fade};
  return out;
}

static int selected_key(void *context, unsigned code, unsigned mode,
                        uint32_t *result, char e[256]) {
  return control_key(context, code, mode, result, e);
}
static int selected_raw_key(void *context, unsigned code, uint32_t *result,
                            char e[256]) {
  return raw_key(context, code, result, e);
}
static int selected_audio(void *context, const BkEndingAudioCall *call,
                          int *playing, char e[256]) {
  EndingNormalScene *s = context;
  if (!s || !s->audio || !call || !playing)
    return fail(e, "selected audio owner is missing");
  return bk_ending_audio_call(s->audio, s->state->frame.group,
                              s->state->auxiliary.variant,
                              s->state->auxiliary.selection, call, playing, e);
}
static int selected_load(void *context, unsigned slot, const char *name,
                         char e[256]) {
  EndingNormalScene *s = context;
  return s && s->audio ? bk_ending_audio_load_speech(s->audio, slot, name, e)
                       : fail(e, "selected speech owner is missing");
}
static int selected_expression(void *context, int32_t a, int32_t b,
                               int32_t mode, char e[256]) {
  EndingNormalScene *s = context;
  (void)b;
  (void)mode;
  BkFaceState *face = s && s->selected_assets
      ? bk_ending_selected_assets_face_state(s->selected_assets) : NULL;
  if (!face) return fail(e, "selected face owner is missing");
  uint32_t now = s->now_ms;
  return bk_face_request(face, a, now, e);
}
static int selected_active(void *context, int32_t *slot, char e[256]) {
  EndingNormalScene *s = context;
  BkClipState state;
  if (!s || !s->selected_assets || !slot ||
      !bk_actor_pose_state(bk_ending_selected_assets_pose(s->selected_assets, 0),
                           &state))
    return fail(e, "selected active clip is unavailable");
  *slot = state.slot;
  return 1;
}
static int selected_request(void *context, unsigned slot, char e[256]) {
  EndingNormalScene *s = context;
  BkActorPose *pose = s && s->selected_assets
      ? bk_ending_selected_assets_pose(s->selected_assets, 0) : NULL;
  return pose && slot < 32
      ? bk_actor_pose_request_mode(pose, slot, BK_CLIP_REQUEST_CONFIGURED, e)
      : fail(e, "selected configured request is unavailable");
}
static int selected_write(void *context, unsigned slot, BkEndingClipWrite kind,
                          int32_t value, char e[256]) {
  EndingNormalScene *s = context;
  BkActorPose *pose = s && s->selected_assets
      ? bk_ending_selected_assets_pose(s->selected_assets, 0) : NULL;
  if (!pose || slot >= 32) return fail(e, "selected descriptor is unavailable");
  BkClipEdit edit = {.slot = slot};
  if (kind == BK_ENDING_CLIP_CHAIN) {
    edit.fields = BK_CLIP_EDIT_CHAIN;
    edit.chain = value;
  } else if (kind == BK_ENDING_CLIP_NEXT) {
    edit.fields = BK_CLIP_EDIT_NEXT;
    edit.next = value;
  } else if (kind == BK_ENDING_CLIP_REWIND) {
    BkClipTiming timing;
    if (!bk_actor_pose_timing(pose, slot, &timing))
      return fail(e, "selected rewind descriptor is unavailable");
    edit.fields = BK_CLIP_EDIT_SOURCE;
    edit.source = timing.start;
  } else return fail(e, "unknown selected descriptor write");
  return bk_actor_pose_edit_clips(pose, &edit, 1, e);
}
static int selected_target(void *context, unsigned node, float position[3],
                           char e[256]) {
  EndingNormalScene *s = context;
  unsigned index = node == 5 ? 5 : node == 13 ? 13 : node == 0 ? 0 : BK_MODEL_NONE;
  if (!s || !s->selected_assets || index == BK_MODEL_NONE || !position)
    return fail(e, "selected camera target is unavailable");
  uint32_t frame = scene_node(s, index);
  uint32_t id = bk_actor_forest_node(scene_forest(s), scene_registry(s, 0), frame);
  const float *world = bk_actor_forest_world(scene_forest(s), id);
  if (!world) return fail(e, "selected camera target is not published");
  memcpy(position, world + 12, sizeof(float) * 3);
  return 1;
}
static int selected_camera(void *context, BkEndingOpeningCamera kind,
                           int32_t choice, const uint32_t offset[3],
                           uint32_t extra, uint32_t *result, char e[256]) {
  EndingNormalScene *s = context;
  const uint32_t tracks[2] = {scene_registry(s, 3), scene_registry(s, 4)};
  if (!s || !s->selected_assets || !result || !offset)
    return fail(e, "selected camera owner is unavailable");
  if (kind == BK_ENDING_OPENING_TRACK) {
    BkEndingCameraOpeningInput input = {s, selected_key};
    int complete = 0;
    if (!bk_ending_camera_assets_opening(scene_cameras(s), scene_forest(s),
          tracks, &s->camera, s->state->retained.normal.follow_target,
          s->active_seconds, &input, &complete, e)) return 0;
    *result = (uint32_t)complete;
    return 1;
  }
  BkEndingCameraPresetGate gate = {
      (uint8_t)s->previous_flow, s->state->frame.phase, s->state->selected,
      s->state->frame.state_721ee0, s->state->frame.state_721ee4,
      s->state->control.state_721eec, s->state->auxiliary.gate,
      s->state->next_mode};
  int complete = 0;
  if (choice < 0 || choice >= 3)
    return fail(e, "selected camera choice is outside presets");
  float offset_f[3];
  memcpy(offset_f, offset, sizeof(offset_f));
  if (!bk_ending_camera_assets_preset(scene_cameras(s), scene_forest(s), tracks,
          &s->camera, &s->camera_transitions, &s->presets,
          BK_ENDING_PRESET, (unsigned)choice, offset_f, &gate,
          0x10, s->active_seconds, &complete, e)) return 0;
  (void)extra;
  *result = (uint32_t)complete;
  return 1;
}
static int selected_manual(void *context, int32_t proposed, int32_t *accepted,
                           char e[256]) {
  EndingNormalScene *s = context;
  if (!s || !accepted) return fail(e, "selected manual owner is missing");
  BkEndingSelectedManualOps ops = {0};
  ops.context = s;
  ops.active = selected_active;
  ops.request_ten = selected_request;
  ops.write = selected_write;
  ops.audio = selected_audio;
  return bk_ending_selected_manual(&s->state->auxiliary, proposed, &ops,
                                   accepted, e);
}
static int selected_pick(void *context, const float pointer[2], int32_t *result,
                         char e[256]) {
  EndingNormalScene *s = context;
  if (!s || !pointer || !result) return fail(e, "selected target picker is missing");
  *result = 0;
  float dx = pointer[0] - (float)s->state->alternate[0];
  float dy = pointer[1] - (float)s->state->alternate[1];
  if (dx * dx + dy * dy <= 150.f * 150.f) *result = 2;
  return 1;
}
static int selected_choose(void *context, const int32_t point[2], int32_t *result,
                           char e[256]) {
  EndingNormalScene *s = context;
  if (!s || !point || !result) return fail(e, "selected menu chooser is missing");
  BkEndingSecondaryMenuGeometry geometry = {
      s->viewport.width, s->viewport.height,
      (float)((double)s->viewport.width / 1280.0), s->ui.sprites[51].rect[2]};
  return bk_ending_secondary_menu_zone(&geometry, point, result, e);
}
static int selected_begin(void *context, char e[256]) {
  EndingNormalScene *s = context;
  if (!s || !s->selected_assets) return fail(e, "selected menu owner is missing");
  BkEndingSecondaryMenuGeometry geometry = {
      s->viewport.width, s->viewport.height,
      (float)((double)s->viewport.width / 1280.0), s->ui.sprites[51].rect[2]};
  int32_t selected = s->state->frame.camera_cached;
  if (selected < 0 || selected >= 39) selected = 0;
  return bk_ending_selected_menu(&geometry, &s->ui_pick,
      s->state->frame.camera_event ? s->state->frame.camera_event : 1,
      s->state->auxiliary.selection, selected, s->state->targets,
      s->state->alternate, s->state->choices, s->state->points, e);
}
static int selected_action(void *context, const BkEndingFrameInput *input,
                           float seconds, char e[256]) {
  EndingNormalScene *s = context;
  if (!s || !s->selected_assets || !input)
    return fail(e, "selected action owner is missing");
  BkEndingSelectedControlBindings control = selected_control_bindings(s);
  BkEndingSelectedActionBindings bindings = selected_action_bindings(s, control);
  BkEndingSelectedActionOps ops = selected_action_ops();
  ops.control.context = s;
  return bk_ending_selected_action_step(&s->selected_action, &bindings, input,
                                        seconds, &ops, e);
}
static int selected_random(void *context, int32_t *result, char e[256]) {
  EndingNormalScene *s = context;
  if (!s || !s->random || !result) return fail(e, "selected RNG owner is missing");
  *result = (int32_t)bk_random_next(s->random);
  return 1;
}
static int selected_clip(void *context, unsigned slot,
                         BkEndingSelectedMotionClip *out, char e[256]) {
  EndingNormalScene *s = context;
  BkActorPose *pose = s && s->selected_assets
      ? bk_ending_selected_assets_pose(s->selected_assets, 0) : NULL;
  BkClipState state;
  BkClipTiming timing;
  BkClipPrediction prediction;
  if (!pose || !out || slot >= 32 || !bk_actor_pose_state(pose, &state) ||
      !bk_actor_pose_timing(pose, slot, &timing) ||
      !bk_actor_pose_prediction(pose, slot, &prediction))
    return fail(e, "selected motion descriptor is unavailable");
  *out = (BkEndingSelectedMotionClip){prediction.duration, timing.start,
      timing.end, timing.source, prediction.rate, state.elapsed};
  return 1;
}
static int selected_source(void *context, unsigned slot, float source,
                           char e[256]) {
  EndingNormalScene *s = context;
  BkActorPose *pose = s && s->selected_assets
      ? bk_ending_selected_assets_pose(s->selected_assets, 0) : NULL;
  BkClipEdit edit = {.slot = slot, .fields = BK_CLIP_EDIT_SOURCE, .source = source};
  return pose && slot < 32 ? bk_actor_pose_edit_clips(pose, &edit, 1, e)
                           : fail(e, "selected source descriptor is unavailable");
}
static int selected_pointer(void *context, const BkEndingFrameInput *input,
                            char e[256]) {
  EndingNormalScene *s = context;
  int32_t pointer[2];
  if (!s || !s->selected_assets || !input) return fail(e, "selected pointer owner is missing");
  memcpy(pointer, input->words + 9, sizeof(pointer));
  return bk_ending_selected_assets_pointer(s->selected_assets,
      s->state->alternate, s->state->points[0], pointer, e);
}
static int selected_drag(void *context, const float motion[2], float *result,
                         char e[256]) {
  EndingNormalScene *s = context;
  if (!s || !s->selected_assets || !motion || !result)
    return fail(e, "selected drag owner is missing");
  *result = 0;
  return bk_ending_selected_assets_drag(s->selected_assets,
      s->selected_plain_scheduled, s->state->retained.auxiliary.word_6ea314,
      motion, e);
}
static int selected_hit(void *context, unsigned menu, const int32_t point[2],
                        int *result, char e[256]) {
  EndingNormalScene *s = context;
  if (!s || menu >= 2 || !point || !result) return fail(e, "selected hit owner is missing");
  *result = 0;
  if (s->state->points[menu][0] || s->state->points[menu][1]) {
    float scale = (float)((double)s->viewport.width / 1280.0);
    float dx = (float)point[0] - (float)s->state->points[menu][0];
    float dy = (float)point[1] - (float)s->state->points[menu][1];
    float radius = 96.f * scale;
    *result = dx * dx + dy * dy <= radius * radius;
  }
  return 1;
}
static int selected_clock(void *context, uint32_t *milliseconds, char e[256]) {
  return frame_clock(context, milliseconds, e);
}
static int selected_stop(void *context, unsigned slot, char e[256]) {
  int playing;
  EndingNormalScene *s = context;
  BkEndingAudioCall call = {.operation = BK_ENDING_AUDIO_PAUSE, .slot = slot};
  return selected_audio(s, &call, &playing, e);
}
static int selected_repeat(void *context, unsigned slot, char e[256]) {
  int playing;
  EndingNormalScene *s = context;
  BkEndingAudioCall call = {.operation = BK_ENDING_AUDIO_RESTART, .slot = slot};
  return selected_audio(s, &call, &playing, e);
}
static int selected_ui_byte(void *context, unsigned slot,
                            BkEndingSelectedUiByte field, uint8_t value,
                            char e[256]) {
  EndingNormalScene *s = context;
  if (!s || slot >= 80 || (unsigned)field > BK_ENDING_SELECTED_UI_BYTE_167)
    return fail(e, "selected UI byte is outside owner");
  s->selected_ui_bytes[slot][field] = value;
  return 1;
}
static int selected_ui_uv_reset(void *context, unsigned slot, char e[256]) {
  EndingNormalScene *s = context;
  if (!s || slot >= 80) return fail(e, "selected UI UV owner is missing");
  memset(s->selected_ui_bytes[slot], 0, sizeof(s->selected_ui_bytes[slot]));
  return 1;
}
static int selected_ui_fade(void *context, unsigned slot, float alpha,
                            char e[256]) {
  EndingNormalScene *s = context;
  if (!s || slot >= 80 || !isfinite(alpha))
    return fail(e, "selected UI fade is outside owner");
  s->selected_ui_alpha[slot] = alpha;
  return 1;
}

static int frame_invoke(void *context, const BkEndingCall *call,
                        uint32_t *result, char e[256]) {
  EndingNormalScene *s = context;
  if (!s || !call || !result || (!scene_forest(s) && !s->final_image) || !s->frame_active)
    return fail(e, "invalid live ending frame binding");
  *result = 0;
  switch (call->operation) {
  case BK_ENDING_STAGE_48D8E9: {
    if (!s->selected_assets || !s->selected_controller)
      return fail(e, "selected controller owners are unavailable");
    BkEndingSelectedControlBindings bindings = selected_control_bindings(s);
    BkEndingSelectedControlOps ops = selected_control_ops();
    ops.context = s;
    return bk_ending_selected_control_step(s->selected_controller, &bindings,
                                           &call->input, s->active_seconds,
                                           &ops, e);
  }
  case BK_ENDING_STAGE_494015: {
    if (!s->selected_assets || !s->selected_controller || !s->audio)
      return fail(e, "selected presentation owners are unavailable");
    BkEndingSelectedPresentationScene presentation = {
        .assets = s->selected_assets, .audio = s->audio,
        .voice = &s->presentation->voice,
        .controller = s->selected_controller, .cycle = s->selected_cycle,
        .shared_cycle = s->auxiliary_cycle, .random = s->random,
        .plain_scheduled = &s->selected_plain_scheduled,
        .voice_volume = &s->voice_volume, .effect_volume = &s->effect_volume,
        .secondary_node = (const uint32_t *)&s->state->retained.normal.word_719b40,
        .clock_context = s, .clock = frame_clock};
    return bk_ending_selected_presentation_scene_step(
        &presentation, s->state, s->active_seconds, e);
  }
  case BK_ENDING_COMMON_4D7AC4: {
    unsigned width = s->viewport.width;
    float scale = (float)((double)width / 1280.0);
    if (!bk_ending_ui_control_rects(&s->ui, s->control_rects))
      return fail(e, "live ending control bounds are unavailable");
    const float *alternate = s->secondary_assets ? bk_actor_forest_world(
        scene_forest(s), (uint32_t)s->state->retained.normal.word_719b40) : NULL;
    BkEndingControlBindings bindings = {
        &s->state->frame,
        &s->camera,
        &s->presets,
        /*4d7ac4 reads old published721ef4/719b40/721f08/721f28.
         * The normal loader's three captured offsets are a different table.
         *719b40 belongs to the alternate loader, not another normal node. */
        {ending_cached_target(s, 0), alternate ? alternate + 12 : NULL,
         ending_cached_target(s, 5),
         ending_cached_target(s, 13)},
        s->control_rects,
        &scale,
        &s->effect_volume};
    BkEndingControlOps ops = {s, control_key, control_sound, NULL, warp};
    return bk_ending_control_step(&s->state->control, &bindings,
                                  &call->input, &ops, e);
  }
  case BK_ENDING_STAGE_4DB608: {
    if (!s->assets)
      return fail(e, "normal controller requires its own assets");
    if (!prepare_ui_geometry(s, s->viewport.width, s->viewport.height, e))
      return 0;
    BkEndingNormalControllerScene controller = {
        .assets = s->assets, .audio = s->audio, .state = s->state,
        .retained = s->normal_controller, .records = s->records,
        .camera = &s->camera, .transitions = &s->camera_transitions,
        .presets = &s->presets, .ring = &s->ui.sprites[50],
        .geometry = &s->ui_pick, .previous_flow = &s->previous_flow,
        .voice_volume = &s->voice_volume, .effect_volume = &s->effect_volume,
        .flip = &s->normal_controller->flip,
        .scene_world = bk_actor_forest_world(
            bk_ending_normal_assets_forest(s->assets), 0),
        .random = s->random, .input_context = s,
        .key = control_key, .raw_key = raw_key, .warp = warp};
    return bk_ending_normal_controller_scene_step(
        &controller, &call->input, s->active_seconds, e);
  }
  case BK_ENDING_STAGE_4DF6C0: {
    if (!s->assets)
      return fail(e, "normal presentation requires its own assets");
    uint32_t primary = bk_ending_normal_assets_root(s->assets, 0);
    uint32_t secondary = bk_ending_normal_assets_root(s->assets, 1);
    uint32_t background = bk_ending_normal_assets_root(s->assets, 4);
    if (primary == BK_FRAME_NONE || secondary == BK_FRAME_NONE ||
        background == BK_FRAME_NONE)
      return fail(e, "normal presentation root is missing");
    BkEndingPresentationBindings bindings = {
        &s->state->frame, &s->state->auxiliary, &s->state->normal_ready,
        s->state->control.toggles, &primary, &background, &secondary,
        &s->state->retained.normal.direct_reference,
        &s->state->retained.normal.direct_node, &s->normal_controller->flip};
    BkEndingPresentationScene presentation = {
        s->assets, s->audio, s->materials, s->disabled, s->disabled_count,
        s->presentation, s->random, s, frame_clock};
    return bk_ending_presentation_scene_step(
        &presentation, &bindings, &call->input, s->active_seconds, e);
  }
  case BK_ENDING_STAGE_47A5D0: {
    if (!s->secondary_assets || !s->secondary_controller ||
        !s->secondary_presentation ||
        !prepare_ui_geometry(s, s->viewport.width, s->viewport.height, e))
      return fail(e, "secondary controller owners are unavailable");
    BkEndingSecondaryControllerScene controller = {
        s->secondary_assets, s->audio, s->state, s->secondary_controller,
        &s->secondary_presentation->rate,
        &s->normal_controller->control.action_kind,
        &s->normal_controller->control.action_column,
        &s->camera, &s->camera_transitions, &s->presets, &s->ui, &s->ui_pick,
        s->viewport.width, s->viewport.height, &s->previous_flow,
        &s->voice_volume, s->random, s, control_key};
    return bk_ending_secondary_controller_scene_step(
        &controller, &call->input, s->active_seconds, e);
  }
  case BK_ENDING_STAGE_47D3CB: {
    if (!s->secondary_assets || !s->secondary_controller ||
        !s->secondary_presentation)
      return fail(e, "secondary presentation owners are unavailable");
    BkEndingSecondaryPresentationScene presentation = {
        s->secondary_assets, s->audio, s->secondary_presentation,
        &s->presentation->voice, s->random, s, frame_clock};
    return bk_ending_secondary_presentation_scene_step(&presentation,
        &s->state->frame, &s->state->auxiliary, &s->secondary_controller->automatic,
        s->state->control.toggles, s->active_seconds, e);
  }
  case BK_ENDING_STAGE_476720: {
    if (!s->tertiary_assets || !s->tertiary_controller || !s->records ||
        !prepare_ui_geometry(s, s->viewport.width, s->viewport.height, e))
      return fail(e, "third controller owners are unavailable");
    const BkEndingTertiaryConfig *config =
        bk_ending_tertiary_assets_config(s->tertiary_assets);
    BkEndingRetainedStage2 *retained = &s->state->retained.stage2;
    float scale = (float)((double)s->viewport.width / 1280.0);
    BkEndingTertiaryActionBindings bindings = {
      .control = {
        .frame = &s->state->frame, .control = &s->state->control,
        .auxiliary = &s->state->auxiliary, .camera = &s->camera,
        .records = s->records, .state = &s->state->stage3_state,
        .face_mode = &s->state->face_mode, .part_mode = &retained->word_6a3c20,
        .voice_latches = retained->words_6afcfc, .return_ready = &retained->word_6afd08,
        .unavailable = s->state->unavailable,
        .movement_ready = &s->state->ui_controller.hints.movement_ready,
        .substate = &retained->byte_6afd18, .expression_override = &retained->word_6a3c24,
        .pending_effect = &retained->word_54ccc8, .fov = &retained->value_54ccd0,
        .open = &s->state->open, .previous_flow = &s->previous_flow,
        .finish_setting = (const int8_t *)&s->ui_item,
        .actions = config->actions + 5, .targets = s->state->targets,
        .initial_targets = bk_ending_tertiary_initial_targets(),
        .voice_volume = &s->voice_volume, .effect_volume = &s->effect_volume,
        .random = s->random},
      .gauge_y = &s->state->gauge_y, .scale = &scale, .choices = s->state->choices};
    BkEndingTertiaryControllerScene controller = {
      .assets = s->tertiary_assets, .audio = s->audio,
      .retained = s->tertiary_controller, .transitions = &s->camera_transitions,
      .presets = &s->presets, .ui = &s->ui, .geometry = &s->ui_pick,
      .width = s->viewport.width, .height = s->viewport.height,
      .speech_names = s->state->speech_names, .targets = s->state->targets,
      .points = s->state->points,
      .action_kind = &s->normal_controller->control.action_kind,
      .action_column = &s->normal_controller->control.action_column,
      .follow_target = &s->state->retained.normal.follow_target,
      .selected = &s->state->selected, .next_mode = &s->state->next_mode,
      .input_context = s, .key = control_key};
    return bk_ending_tertiary_controller_scene_step(
        &controller, &bindings, &call->input, s->active_seconds, e);
  }
  case BK_ENDING_STAGE_479137: {
    if (!s->tertiary_assets || !s->presentation)
      return fail(e, "third presentation owners are unavailable");
    BkEndingTertiaryPresentationScene presentation = {
      .assets = s->tertiary_assets, .audio = s->audio, .voice = &s->presentation->voice,
      .random = s->random, .bom_disabled = s->disabled, .bom_count = s->disabled_count,
      .clock_context = s, .clock = frame_clock};
    return bk_ending_tertiary_presentation_scene_step(&presentation,
        s->state, &s->state->face_mode, &s->state->eye_lower, s->active_seconds, e);
  }
  case BK_ENDING_STAGE_47DC79: {
    if (!s->auxiliary_assets)
      return fail(e, "47DC79 requires the auxiliary loader");
    if (s->state->control.state_721eec == 0) {
      /* Native jump-table state0 is the one-word transition into state1. */
      s->state->control.state_721eec = 1;
      return 1;
    }
    if (s->state->control.state_721eec == 1) {
      BkEndingAuxiliaryState1Bindings bindings = {
          &s->state->frame, &s->state->control, &s->state->auxiliary,
          &s->state->retained.stage4.timer_6c7f6c,
          &s->state->retained.stage4.delay_54f8e0};
      BkEndingAuxiliaryState1Ops ops = {
          s, control_key, auxiliary_state4_present, auxiliary_state4_status,
          auxiliary_state1_audio, auxiliary_state1_random,
          auxiliary_state1_pick, auxiliary_state1_menu, auxiliary_state1_voice,
          auxiliary_state1_request, auxiliary_state4_expression};
      float pointer[2];
      for (unsigned i = 0; i < 2; ++i) {
        int32_t coordinate;
        memcpy(&coordinate, &call->input.words[9 + i], sizeof(coordinate));
        pointer[i] = (float)coordinate;
      }
      return bk_ending_auxiliary_state1_step(
          &bindings, pointer, s->active_seconds, s->voice_volume, &ops, e);
    }
    if (s->state->control.state_721eec == 2) {
      BkEndingAuxiliaryState1Ops ops = {s, control_key, NULL, NULL, NULL,
                                        NULL, NULL, NULL, NULL, NULL, NULL};
      return bk_ending_auxiliary_state2_step(&s->state->control, &ops, e);
    }
    if (s->state->control.state_721eec == 3) {
      BkEndingAuxiliaryState3Bindings bindings = {
          &s->state->frame, &s->state->control, &s->state->auxiliary,
          s->state->points[0], &s->ui.sprites[51].rect[2]};
      BkEndingAuxiliaryState3Ops ops = {
          s,
          auxiliary_state3_child,
          control_key,
          auxiliary_state3_hit,
          auxiliary_state4_expression,
          auxiliary_state1_request,
          auxiliary_state1_voice,
          s->voice_volume};
      return bk_ending_auxiliary_state3_step(&bindings, &call->input, &ops,
                                             e);
    }
    if (s->state->control.state_721eec == 8) {
      BkEndingAuxiliaryControllerBindings bindings = {
          &s->state->frame,
          &s->state->control,
          &s->state->auxiliary,
          &s->state->retained.stage4.delay_54f8e0,
          &s->state->retained.stage3.byte_6bbe4c};
      BkEndingAuxiliaryControllerOps ops = {
          s,
          auxiliary_state4_status,
          auxiliary_state1_request,
          auxiliary_state4_expression,
          auxiliary_state1_voice,
          auxiliary_state8_group_sound,
          auxiliary_state8_camera_setup,
          auxiliary_state8_target,
          s->voice_volume};
      return bk_ending_auxiliary_controller_begin(&bindings, &ops, e);
    }
    if (s->state->control.state_721eec != 4)
      return fail(e, "47DC79 state is not yet implemented");
    BkEndingAuxiliaryState4Bindings bindings = {
        &s->state->frame,
        &s->state->control,
        &s->state->auxiliary,
        &s->camera,
        &s->state->retained.stage4.byte_6c7f50,
        &s->state->retained.stage4.value_54e310,
        &s->previous_flow};
    BkEndingAuxiliaryState4Ops ops = {
        s,
        auxiliary_state4_present,
        auxiliary_state4_status,
        auxiliary_state4_prepare_actor,
        auxiliary_state4_expression,
        auxiliary_state4_target,
        auxiliary_state4_camera,
        auxiliary_state4_effect};
    return bk_ending_auxiliary_state4_step(&bindings, s->active_seconds,
                                          &ops, e);
  }
  case BK_ENDING_STAGE_48181F:
    return auxiliary_presentation_step(s, e);
  case BK_ENDING_AUXILIARY_4965B9: {
    BkEndingAuxiliaryServices services = {
        scene_primary(s), scene_eyes(s), s->audio};
    return bk_ending_auxiliary_tick_apply(
        &services, &s->state->auxiliary, &s->state->frame,
        s->auxiliary_cycle, s->active_seconds, &s->voice_volume, s->random, e);
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
    const uint32_t tracks[2] = {scene_registry(s, 3), scene_registry(s, 4)};
    unsigned buttons = (s->input.held & BK_BUTTON_CAMERA_ORBIT ? 1u : 0u) |
                       (s->input.held & BK_BUTTON_CAMERA_ADJUST ? 2u : 0u);
    /* L/R replace the mouse-button modifiers. Convert both stick axes to
     * the opposite mouse-drag direction: R-right closes radius, R-up raises
     * height. Keep the sixfold manual gain and authored track time/FOV. */
    float motion[2] = {-s->input.look_x * 6.f, -s->input.look_y * 6.f};
    uint32_t target;
    if (call->operation == BK_ENDING_CAMERA_4BB0A4)
      return bk_ending_camera_assets_step(
          scene_cameras(s), scene_forest(s), tracks, &s->camera,
          BK_ENDING_CAMERA_ORBIT, offset, motion, buttons, BK_FRAME_NONE,
          s->active_seconds, e);
    if (call->operation == BK_ENDING_CAMERA_4DF411)
      return bk_ending_camera_assets_step(
          scene_cameras(s), scene_forest(s), tracks, &s->camera,
          BK_ENDING_CAMERA_FIXED, offset, NULL, 0, BK_FRAME_NONE, 0, e);
    /*4e1711 follows719448 (A_kuch);4e1d16 explicitly aims at721ef4
     * (A_kao). Preset controllers do not read either node. */
    if ((call->operation == BK_ENDING_CAMERA_4E1711 ||
         call->operation == BK_ENDING_CAMERA_4E1D16) &&
        !ending_target_node(s,
                            call->operation == BK_ENDING_CAMERA_4E1711 ? 1 : 0,
                            &target, e))
      return 0;
    if (call->operation == BK_ENDING_CAMERA_4E1711)
      return bk_ending_camera_assets_step(
          scene_cameras(s), scene_forest(s), tracks, &s->camera,
          BK_ENDING_CAMERA_AUTO, offset, NULL, 0, target, s->active_seconds, e);
    if (call->operation == BK_ENDING_CAMERA_4E1D16)
      return bk_ending_camera_assets_step(
          scene_cameras(s), scene_forest(s), tracks, &s->camera,
          BK_ENDING_CAMERA_MANUAL, offset, motion, buttons, target,
          s->active_seconds, e);
    if (call->count < 5)
      return fail(e, "ending preset call is truncated");
    BkEndingCameraPresetGate gate = {(uint8_t)s->previous_flow,
                                    s->state->frame.phase,
                                    s->state->selected,
                                    s->state->frame.state_721ee0,
                                    s->state->frame.state_721ee4,
                                    s->state->control.state_721eec,
                                    s->state->auxiliary.gate,
                                    s->state->next_mode};
    int done = 0;
    int ok = bk_ending_camera_assets_preset(
        scene_cameras(s), scene_forest(s), tracks, &s->camera,
        &s->camera_transitions, &s->presets,
        call->operation == BK_ENDING_CAMERA_4BC444 ? BK_ENDING_PRESET_ZOOM
                                                   : BK_ENDING_PRESET,
        call->args[1], offset, &gate, 0x10, s->active_seconds, &done, e);
    *result = (uint32_t)done;
    return ok;
  }
  case BK_ENDING_STAGE_48302B:
  case BK_ENDING_STAGE_48BCBB:
    return fail(e, "ending phase controller is not implemented");
  case BK_ENDING_STAGE_4E2223: {
    BkEndingControlRect rects[2];
    if (!bk_ending_ui_confirm_rects(&s->ui, rects))
      return fail(e, "live ending confirmation bounds are unavailable");
    BkEndingConfirmBindings bindings = {
        &s->state->frame, &s->state->frame.transition_action,
        &s->state->frame.curtain_wanted, rects, &s->effect_volume};
    BkEndingControlOps ops = {s, control_key, control_sound, NULL, NULL};
    return bk_ending_confirm_step(&s->state->control, &bindings,
                                   &call->input, &ops, e);
  }
  default:
    return fail(e, "unknown ending frame operation");
  }
}

static int prepare_story_frame(EndingNormalScene *s, char e[256]) {
  BkEndingFrameInput input = {0};
  for (unsigned i = 0; i < 2; ++i) {
    if (!isfinite(s->pointer.position[i]) ||
        (double)s->pointer.position[i] < INT32_MIN ||
        (double)s->pointer.position[i] > INT32_MAX ||
        !isfinite(s->pointer.motion[i]) ||
        (double)s->pointer.motion[i] < INT32_MIN ||
        (double)s->pointer.motion[i] > INT32_MAX)
      return fail(e, "ending pointer is outside native integer range");
    int32_t coordinate = (int32_t)s->pointer.position[i];
    int32_t motion = (int32_t)s->pointer.motion[i];
    memcpy(&input.words[9 + i], &coordinate, sizeof(coordinate));
    memcpy(&input.words[6 + i], &motion, sizeof(motion));
  }
  BkEndingFrameOps ops = {s, frame_invoke, frame_clock, frame_key};
  return bk_ending_state_import_frame_aliases(s->state, s->common,
                                              &s->stage_ui) &&
         bk_ending_frame_step(&s->state->frame, s->state->working, &input, &ops,
                               e) &&
         bk_ending_state_export_frame_requests(s->state, s->common);
}

static int special_material_registry(EndingNormalScene *s, char e[256]) {
  if (s->tertiary_assets) {
    /*4d2320 retains the outer background. A replacement's registry starts
     * with that owner; a fresh entry registers it after the body models. */
    const unsigned fresh[] = {0, 1, 2, 5};
    const unsigned retained[] = {5, 0, 1, 2};
    const unsigned *order = (s->retired.render || s->retained_background) ? retained : fresh;
    size_t count = 0;
    for (unsigned a = 0; a < 4; ++a) {
      BkActorPose *pose = scene_actor(s, order[a]);
      if (!pose)
        continue;
      const BkModel *model = bk_actor_pose_model(pose);
      if (!model || !bk_ending_tertiary_assets_materials(s->tertiary_assets, order[a]))
        return fail(e, "missing third material owner");
      count += model->material_count;
    }
    if (count && !(s->special_materials = calloc(count, sizeof(*s->special_materials))))
      return fail(e, "third material registry allocation failed");
    size_t index = 0;
    for (unsigned a = 0; a < 4; ++a) {
      BkActorPose *pose = scene_actor(s, order[a]);
      if (!pose)
        continue;
      const BkModel *model = bk_actor_pose_model(pose);
      BkMaterialPose *materials = bk_ending_tertiary_assets_materials(s->tertiary_assets, order[a]);
      for (uint32_t m = 0; m < model->material_count; ++m)
        s->special_materials[index++] = (BkEndingSpecialMaterial){materials, m};
    }
    s->special_material_count = count;
    return 1;
  }
  if (s->secondary_assets) {
    const BkModel *models[2];
    BkMaterialPose *poses[2];
    size_t count = 0;
    for (unsigned a = 0; a < 2; ++a) {
      unsigned actor = a ? 3 : 0;
      models[a] = bk_actor_pose_model(
          bk_ending_secondary_assets_pose(s->secondary_assets, actor));
      poses[a] = bk_ending_secondary_assets_materials(s->secondary_assets, actor);
      if (!models[a] || !poses[a])
        return fail(e, "missing secondary material owner");
      count += models[a]->material_count;
    }
    if (count) {
      s->special_materials = calloc(count, sizeof(*s->special_materials));
      if (!s->special_materials)
        return fail(e, "secondary material registry allocation failed");
    }
    size_t index = 0;
    for (unsigned a = 0; a < 2; ++a)
      for (uint32_t m = 0; m < models[a]->material_count; ++m)
        s->special_materials[index++] = (BkEndingSpecialMaterial){poses[a], m};
    s->special_material_count = count;
    return 1;
  }
  if (s->selected_assets) {
    const BkModel *primary = bk_actor_pose_model(
        bk_ending_selected_assets_pose(s->selected_assets, 0));
    BkMaterialPose *materials = bk_ending_selected_assets_materials(
        s->selected_assets, 0);
    if (!primary || !materials)
      return fail(e, "missing selected material registry");
    if (primary->material_count) {
      s->special_materials = calloc(primary->material_count,
                                    sizeof(*s->special_materials));
      if (!s->special_materials)
        return fail(e, "selected material registry allocation failed");
    }
    for (uint32_t i = 0; i < primary->material_count; ++i)
      s->special_materials[i] = (BkEndingSpecialMaterial){materials, i};
    s->special_material_count = primary->material_count;
    return 1;
  }
  if (s->auxiliary_assets) {
    const BkModel *primary = bk_actor_pose_model(
        bk_ending_auxiliary_assets_pose(s->auxiliary_assets, 0));
    BkMaterialPose *materials = bk_ending_auxiliary_assets_materials(
        s->auxiliary_assets, 0);
    if (!primary || !materials)
      return fail(e, "missing auxiliary material registry");
    if (primary->material_count) {
      s->special_materials = calloc(primary->material_count,
                                    sizeof(*s->special_materials));
      if (!s->special_materials)
        return fail(e, "auxiliary material registry allocation failed");
    }
    for (uint32_t i = 0; i < primary->material_count; ++i)
      s->special_materials[i] = (BkEndingSpecialMaterial){materials, i};
    s->special_material_count = primary->material_count;
    return 1;
  }
  const BkModel *primary =
      bk_actor_pose_model(bk_ending_normal_assets_pose(s->assets, 0));
  if (!primary || !s->materials)
    return fail(e, "missing normal material registry");
  /*The original lookup searches the actual loaded registry in load order. Normal4cf318
   * mutable materials belong to the primary model. A replacement asset with
   * the requested special material in another owner needs its own rendering
   * binding; do not turn such a match into an apparently successful miss. */
  int found = 0;
  const unsigned order[] = {0, 1, 4};
  for (unsigned a = 0; a < 3 && !found; a++) {
    const BkModel *model =
        bk_actor_pose_model(bk_ending_normal_assets_pose(s->assets, order[a]));
    if (!model)
      return fail(e, "missing loaded material owner");
    for (uint32_t i = 0; i < model->material_count; i++)
      if (!strcmp(model->materials[i].name, "Om_syokusyu_maki")) {
        if (a)
          return fail(e, "special material needs a secondary rendering owner");
        found = 1;
        break;
      }
  }
  if (primary->material_count) {
    s->special_materials =
        calloc(primary->material_count, sizeof(*s->special_materials));
    if (!s->special_materials)
      return fail(e, "material registry allocation failed");
  }
  for (uint32_t i = 0; i < primary->material_count; i++)
    s->special_materials[i] = (BkEndingSpecialMaterial){s->materials, i};
  s->special_material_count = primary->material_count;
  return 1;
}

static int complete_load(EndingNormalScene *s, unsigned variant) {
  /*The new stage has retained or explicitly replaced the background. Only
   * its live light registry now owns device values; no old 3D owner is kept.*/
  bk_ending_background_assets_destroy(s->retained_background);
  s->retained_background = NULL;
  memset(&s->retained_lights, 0, sizeof(s->retained_lights));
  s->has_retained_lights = 0;
  s->variant = variant;
  return 1;
}

static int normal_load(void *context, BkEndingLoader loader, int32_t argument,
                       char e[256]) {
  EndingNormalScene *s = context;
  unsigned variant;
  BkEndingUiStageKind stage_kind;
  if (!s || s->assets || s->secondary_assets || s->tertiary_assets ||
      s->selected_assets || s->auxiliary_assets || s->render || s->final_image)
    return fail(e, "invalid ending loader owner");
  /*4d00fa has a different topology. The normal action-table variant is
   *721e04 and must not be confused with gallery selection1/phase2. */
  if (loader == BK_ENDING_LOAD_4CF318) {
    variant = s->state->auxiliary.variant != 0;
    stage_kind = BK_ENDING_UI_NORMAL;
  } else if (loader == BK_ENDING_LOAD_4D00FA) {
    variant = s->state->auxiliary.variant != 0;
    stage_kind = BK_ENDING_UI_SECONDARY;
    if (!s->secondary_controller || !s->secondary_presentation)
      return fail(e, "secondary process owners are required before loading");
    if (s->flow.common && (!s->flow.secondary_controller ||
                           !s->flow.secondary_presentation))
      return fail(e, "application stage reload requires secondary process owners");
  } else if (loader == BK_ENDING_LOAD_4D2320) {
    variant = s->state->auxiliary.variant != 0;
    stage_kind = BK_ENDING_UI_THIRD;
    if (!s->tertiary_controller || !s->records ||
        (s->flow.common && !s->flow.tertiary_controller))
      return fail(e, "third loader requires retained controller and recording owners");
  } else if (loader == BK_ENDING_LOAD_4D1025) {
    if (argument < 0 || argument >= 3 ||
        (s->gallery_selection != 2 && s->gallery_selection != 5))
      return fail(e, "invalid selected-ending loader argument");
    variant = s->state->auxiliary.variant != 0;
    stage_kind = BK_ENDING_UI_FOURTH;
  } else if (loader == BK_ENDING_LOAD_4D39E6) {
    if (argument != -1)
      return fail(e, "invalid auxiliary loader argument");
    variant = s->state->auxiliary.variant != 0;
    stage_kind = BK_ENDING_UI_AUXILIARY;
  } else
    return fail(e, "normal scene does not own this ending loader");
  if (loader != BK_ENDING_LOAD_4D1025 && argument != -1)
    return fail(e, "invalid normal loader argument");
  uint32_t clocks[4] = {s->now_ms, s->now_ms, s->now_ms, s->now_ms};
  BkEndingBackgroundAssets *background = s->retained_background
      ? s->retained_background : scene_retired_background(s);
  if (s->retired.render && !background)
    return fail(e, "released stage has no retained outer background");
  if (stage_kind == BK_ENDING_UI_AUXILIARY) {
    if (!background)
      return fail(e, "4D39E6 requires the preceding retained outer background");
    s->auxiliary_assets = bk_ending_auxiliary_assets_create_reloaded(
        s->services.resources, s->state->frame.group, variant, background,
        clocks, s->random, &s->camera, &s->presets, e);
    if (!s->auxiliary_assets)
      goto bad;
  } else if (stage_kind == BK_ENDING_UI_THIRD) {
    s->tertiary_assets = background
        ? bk_ending_tertiary_assets_create_reloaded(s->services.resources,
              s->state->frame.group, variant, background, clocks, s->random,
              &s->camera, &s->presets, e)
        : bk_ending_tertiary_assets_create(s->services.resources,
              s->state->frame.group, variant, clocks, s->random,
              &s->camera, &s->presets, e);
    if (!s->tertiary_assets)
      goto bad;
    /*4d34d1..4d35bf, before a fresh outer background is registered. The
     *5736a4 loader table matches the controller's548f88 six-name table. */
    for (unsigned i = 0; i < 6; ++i) {
      const char *name = bk_ending_tertiary_material_name(
          BK_ENDING_TERTIARY_MATERIAL_SIX, s->state->frame.group, i);
      if (!name || !bk_ending_tertiary_assets_material_alpha(
                       s->tertiary_assets, name, i != 2 && i != 5, 1, e))
        goto bad;
    }
  } else if (stage_kind == BK_ENDING_UI_SECONDARY) {
    s->secondary_assets = background
        ? bk_ending_secondary_assets_create_reloaded(s->services.resources,
              s->state->frame.group, variant, background, clocks, s->random,
              &s->camera, &s->presets, e)
        : bk_ending_secondary_assets_create(s->services.resources,
              s->state->frame.group, variant, clocks, s->random,
              &s->camera, &s->presets, e);
  } else if (stage_kind == BK_ENDING_UI_FOURTH) {
    BkEndingSelectedConfig selected_config;
    if (!bk_ending_selected_config(&selected_config, s->state->frame.group,
                                   variant, (unsigned)argument))
      goto bad;
    BkEndingSelectedLoad load = {
        s->state->frame.group, variant, (unsigned)argument,
        s->gallery_selection, s->state->model_paths[selected_config.event],
        background};
    s->selected_assets = bk_ending_selected_assets_create(
        s->services.resources, &load, clocks, s->random, &s->camera,
        &s->presets, e);
  } else {
    s->assets = background
        ? bk_ending_normal_assets_create_reloaded(s->services.resources,
              s->state->frame.group, variant, background, clocks, s->random,
              &s->camera, &s->presets, e)
        : bk_ending_normal_assets_create(s->services.resources,
              s->state->frame.group, variant, clocks, s->random,
              &s->camera, &s->presets, e);
  }
  if (!scene_forest(s))
    goto bad;
  /*4cfbfa..4cfcb2: transfer the loader's pre-background target snapshots
   * to the live control/dispatcher owners. Leaving camera_values cleared
   * makes the first4bb0a4 frame orbit the world origin instead of the actor.
   * These remain captured values until a native target-refresh operation;
   * do not replace them with a per-frame moving focus. */
  for (unsigned i = 0; i < 3; ++i) {
    const float *target = s->tertiary_assets
        ? bk_ending_tertiary_assets_target(s->tertiary_assets, i)
        : s->secondary_assets
        ? bk_ending_secondary_assets_target(s->secondary_assets, i)
        : s->selected_assets
        ? bk_ending_selected_assets_target(s->selected_assets, i)
        : s->auxiliary_assets
        ? bk_ending_auxiliary_assets_target(s->auxiliary_assets, i)
        : bk_ending_normal_assets_target(s->assets, i);
    if (!target) {
      fail(e, "normal loader camera target is unavailable");
      goto bad;
    }
    memcpy(s->state->control.targets[i], target,
           sizeof(s->state->control.targets[i]));
  }
  memcpy(s->state->frame.camera_values, s->state->control.targets[0],
         sizeof(s->state->frame.camera_values));
  memcpy(s->state->frame.camera_table,
         s->tertiary_assets
             ? bk_ending_tertiary_assets_config(s->tertiary_assets)->camera_table
             : s->secondary_assets
             ? bk_ending_secondary_assets_config(s->secondary_assets)->camera_table
             : s->selected_assets
             ? bk_ending_selected_assets_config(s->selected_assets)->camera_table
             : s->auxiliary_assets
             ? bk_ending_auxiliary_assets_config(s->auxiliary_assets)->camera_table
             : bk_ending_normal_assets_config(s->assets)->camera_table,
         sizeof(s->state->frame.camera_table));
  if (s->auxiliary_assets) {
    if (!bk_ending_auxiliary_assets_background(s->auxiliary_assets))
      goto bad;
    s->render = bk_ending_auxiliary_render_create(
        s->services.renderer, s->services.resources, s->auxiliary_assets,
        (int32_t)s->now_ms, e);
    s->materials = bk_ending_auxiliary_assets_materials(s->auxiliary_assets, 0);
  } else if (s->tertiary_assets) {
    if (!bk_ending_tertiary_assets_load_background(s->tertiary_assets,
                                                   s->services.resources, e))
      goto bad;
    s->render = bk_ending_tertiary_render_create(s->services.renderer,
        s->services.resources, s->tertiary_assets, (int32_t)s->now_ms, e);
    s->materials = bk_ending_tertiary_assets_materials(s->tertiary_assets, 0);
  } else if (s->secondary_assets) {
    if (!bk_ending_secondary_assets_load_background(s->secondary_assets,
                                                     s->services.resources, e))
      goto bad;
    s->render = bk_ending_secondary_render_create(s->services.renderer,
        s->services.resources, s->secondary_assets, (int32_t)s->now_ms, e);
    s->materials = bk_ending_secondary_assets_materials(s->secondary_assets, 0);
  } else if (s->selected_assets) {
    if (!bk_ending_selected_assets_load_background(s->selected_assets,
                                                   s->services.resources, e))
      goto bad;
    s->render = bk_ending_selected_render_create(s->services.renderer,
        s->services.resources, s->selected_assets, (int32_t)s->now_ms, e);
    s->materials = bk_ending_selected_assets_materials(s->selected_assets, 0);
  } else {
    if (!bk_ending_normal_assets_load_background(s->assets, s->services.resources, e))
      goto bad;
    s->render = bk_ending_normal_render_create_special(
        s->services.renderer, s->services.resources, s->assets, (int32_t)s->now_ms, e);
    s->materials = bk_material_pose_create(bk_actor_pose_model(scene_primary(s)), e);
  }
  if (!s->render)
    goto bad;
  if (s->has_retained_lights
          ? !bk_ending_normal_render_restore_lighting(s->render, &s->retained_lights, e)
          : s->retired.render && !bk_ending_normal_render_inherit_lighting(
                                     s->render, s->retired.render, e))
    goto bad;
  if (!s->materials || !special_material_registry(s, e))
    goto bad;
  s->disabled_count = s->tertiary_assets
      ? bk_bom_dual_assets_count(bk_ending_tertiary_assets_bom(s->tertiary_assets))
      : (s->selected_assets || s->auxiliary_assets) ? 0
      : s->assets ? bk_bom_assets_count(bk_ending_normal_assets_bom(s->assets)) : 0;
  if (s->disabled_count > sizeof(s->disabled) / sizeof(s->disabled[0])) {
    fail(e, "normal disabled-model table is too large");
    goto bad;
  }
  memset(s->disabled, 0, sizeof(s->disabled));
  if (!s->audio)
    s->audio = bk_ending_audio_create_entry(
        s->services.resources, s->services.audio, 0, s->state->frame.group, variant,
        -900, e);
  if (!s->audio)
    goto bad;
  unsigned width = s->viewport.width;
  if (!bk_ending_stage_ui_initialize(&s->ui, &s->stage_ui,
                                     stage_kind,
                                     s->state->frame.group, variant, width, e) ||
      (!s->ui_render && !(s->ui_render = bk_ending_ui_render_create(
            s->services.renderer, s->services.resources, e))) ||
      (!s->ui_curtain && !(s->ui_curtain = bk_curtain_render_create(
            s->services.renderer, s->services.resources, e))) ||
      (!s->ui_batch && !(s->ui_batch = bk_ending_ui_batch_create(s->ui_curtain, e))))
    goto bad;
  if (s->stage_ui.loaded &&
      !(s->ui_stage_render = bk_ending_ui_render_create_stage(
            s->services.renderer, s->services.resources, &s->stage_ui, e)))
    goto bad;
  static const unsigned control_slots[4] = {0, 5, 3, 2};
  for (unsigned i = 0; i < 4; ++i) {
    if (!s->control_audio[i])
      s->control_audio[i] = bk_system_audio_create_slot(
          s->services.resources, s->services.audio, 57 + i, control_slots[i], -600, e);
    if (!s->control_audio[i])
      goto bad;
  }
  if (s->selected_assets) {
    const BkEndingSelectedConfig *config =
        bk_ending_selected_assets_config(s->selected_assets);
    s->state->auxiliary.expression_a = config->expression_a;
    s->state->auxiliary.expression_b = config->expression_b;
    s->state->control.toggles[0] = 0;
    s->state->control.toggles[1] = bk_ending_selected_assets_missing_visible(
        s->selected_assets) == 3;
    s->state->control.toggles[2] = !s->state->control.toggles[1];
    s->state->control.toggles[4] = s->option_a == 1;
    s->state->control.toggles[6] = s->option_b == 1;
    s->state->control.toggles[5] = 1;
    s->state->control.toggles[3] = s->state->frame.group != 1;
    s->state->frame.camera_cached = -1;
    s->state->frame.camera_mode = 5;
    s->state->retained.normal.follow_target = bk_actor_forest_node(
        scene_forest(s), 0, bk_ending_selected_assets_follow(s->selected_assets));
    if (s->state->retained.normal.follow_target == BK_FRAME_NONE)
      goto bad;
    s->state->frame.phase = config->phase;
    s->state->frame.state_721ee4 = 4;
    s->state->selected = (int32_t)s->gallery_selection;
    s->state->auxiliary.selection = (int32_t)argument;
    s->state->auxiliary.pending = 0;
    s->state->frame.camera_clip = 0;
    return complete_load(s, variant);
  }
  if (s->auxiliary_assets) {
    const BkEnding4d39Config *config =
        bk_ending_auxiliary_assets_config(s->auxiliary_assets);
    unsigned missing = bk_ending_auxiliary_assets_missing_visible(s->auxiliary_assets);
    s->state->auxiliary.expression_a = config->expression_a;
    s->state->auxiliary.expression_b = config->expression_b;
    /*4D39E6 writes the auxiliary-stage flags after the two optional-name
     * lookups. Missing nodes remain explicit state; no retained slot is
     * cleared by an absent lookup. */
    s->state->control.toggles[0] = 0;
    s->state->control.toggles[1] = missing == 3;
    s->state->control.toggles[2] = missing != 3;
    s->state->control.toggles[3] = s->state->frame.group != 1;
    s->state->control.toggles[4] = s->state->frame.group == 1;
    s->state->control.toggles[5] = 1;
    s->state->control.toggles[6] = s->option_b == 1;
    s->state->frame.camera_cached = -1;
    s->state->frame.camera_mode = 5;
    s->state->retained.normal.follow_target = bk_actor_forest_node(
        scene_forest(s), scene_registry(s, 0),
        bk_ending_auxiliary_assets_follow(s->auxiliary_assets));
    if (s->state->retained.normal.follow_target == BK_FRAME_NONE)
      goto bad;
    s->state->frame.phase = 4;
    s->state->frame.state_721ee4 = 4;
    s->state->control.state_721eec = 4;
    s->state->stage3_state = 0;
    s->state->auxiliary.pending = 0;
    s->state->face_mode = 0;
    s->state->frame.camera_clip = 0;
    return complete_load(s, variant);
  }
  if (s->tertiary_assets) {
    const BkEndingTertiaryConfig *config =
        bk_ending_tertiary_assets_config(s->tertiary_assets);
    s->state->auxiliary.expression_a = config->expression_a;
    s->state->auxiliary.expression_b = config->expression_b;
    unsigned missing = bk_ending_tertiary_assets_missing_visible(s->tertiary_assets);
    s->state->control.toggles[0] = 0;
    s->state->control.toggles[1] = missing == 3;
    s->state->control.toggles[2] = missing != 3;
    s->state->control.toggles[4] = s->option_a == 1;
    s->state->control.toggles[6] = s->option_b == 1;
    s->state->control.toggles[5] = 1;
    s->state->control.toggles[3] = s->state->frame.group != 1;
    if (s->state->frame.group == 1)
      s->state->control.toggles[4] = 1;
    s->state->frame.camera_cached = -1;
    s->state->control.target_choice = 0;
    s->state->frame.camera_mode = 5;
    s->state->retained.normal.follow_target = bk_actor_forest_node(scene_forest(s),
        scene_registry(s, 0), bk_ending_tertiary_assets_follow(s->tertiary_assets));
    if (s->state->retained.normal.follow_target == BK_FRAME_NONE) {
      fail(e, "third loader follow node is unavailable");
      goto bad;
    }
    s->state->frame.phase = 3;
    s->state->stage3_state = 4;
    s->state->auxiliary.pending = 0;
    s->state->face_mode = 0;
    s->state->frame.camera_clip = 0;
    /*Unlike4cf318/4d00fa, this loader never forces selected=1 for group1.*/
    return complete_load(s, variant);
  }
  if (s->secondary_assets) {
    /*4D00FA's scalar writes; all parent/display counters, saved camera and
     * envelope state live outside this loader and retain their owners. */
    const BkEndingSecondaryConfig *config =
        bk_ending_secondary_assets_config(s->secondary_assets);
    s->state->auxiliary.expression_a = config->expression_a;
    s->state->auxiliary.expression_b = config->expression_b;
    unsigned missing = bk_ending_secondary_assets_missing_visible(s->secondary_assets);
    s->state->control.toggles[0] = 0;
    s->state->control.toggles[1] = missing == 3;
    s->state->control.toggles[2] = missing != 3;
    s->state->control.toggles[4] = s->option_a == 1;
    s->state->control.toggles[6] = s->option_b == 1;
    s->state->control.toggles[5] = 1;
    s->state->control.toggles[3] = s->state->frame.group != 1;
    if (s->state->frame.group == 1)
      s->state->control.toggles[4] = 1;
    s->state->frame.camera_cached = -1;
    s->state->control.target_choice = 0; /*721ecc*/
    s->state->frame.camera_mode = 5;
    s->state->retained.normal.follow_target = bk_actor_forest_node(scene_forest(s), 0,
        bk_ending_secondary_assets_follow(s->secondary_assets));
    uint32_t anchor = bk_actor_forest_node(scene_forest(s), 0,
        bk_ending_secondary_assets_anchor(s->secondary_assets));
    if (s->state->retained.normal.follow_target == BK_FRAME_NONE ||
        anchor == BK_FRAME_NONE || anchor > INT32_MAX) {
      fail(e, "secondary loader node identities are unavailable");
      goto bad;
    }
    s->state->retained.normal.word_719b40 = (int32_t)anchor;
    s->state->frame.phase = 2;
    s->state->frame.state_721ee4 = 4;
    s->state->auxiliary.pending = 0;
    s->state->frame.camera_clip = 0;
    if (s->state->frame.group == 1)
      s->state->selected = 1;
    return complete_load(s, variant);
  }
  /* Actual4cf318 scalar writes. Independent process controller clocks,
   * manual state and envelopes survive loading. Node IDs belong to assets. */
  const BkEndingNormalConfig *config = bk_ending_normal_assets_config(s->assets);
  s->state->auxiliary.expression_a = config->expression_a;
  s->state->auxiliary.expression_b = config->expression_b;
  if (!s->state->next_mode)
    memset(s->state->normal_inputs, 0, sizeof(s->state->normal_inputs));
  s->state->frame.camera_cached = -1;
  s->state->control.toggles[4] = s->option_a == 1;
  s->state->control.toggles[6] = s->option_b == 1;
  s->state->control.toggles[5] = 1;
  s->state->control.toggles[3] = s->state->frame.group != 1;
  if (s->state->frame.group == 1)
    s->state->control.toggles[4] = 1;
  s->state->frame.camera_mode = 5;
  if (!ending_target_node(s, 1, &s->state->retained.normal.follow_target, e))
    goto bad;
  s->state->normal_ready = 0;
  s->state->control.toggles[0] = 0;
  s->state->control.toggles[1] = 1;
  s->state->frame.phase = 1;
  s->state->frame.state_721ee0 = 4;
  s->state->auxiliary.pending = 0;
  s->state->frame.camera_clip = 0;
  if (s->state->frame.group == 1)
    s->state->selected = 1;
  return complete_load(s, variant);
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

static int prepare_scene_draw(EndingNormalScene *s, char e[256]) {
  if (!s || !scene_forest(s) || !s->render)
    return fail(e, "missing normal draw owner");
  BkDrawDispatchInput in;
  BkFog fog = {0};
  /*Native roots721b28/2c/34 are primary/upper/background. Third's lower
   *body is visited through the real hierarchy, not a fabricated extra pass.*/
  uint32_t roots[3] = {scene_root(s, 0), scene_root(s, 1), scene_root(s, 5)};
  if (!roots[0] || !roots[2] || roots[0] == BK_FRAME_NONE ||
      roots[1] == BK_FRAME_NONE || roots[2] == BK_FRAME_NONE)
    return fail(e, "ending draw root is unavailable");
  in = (BkDrawDispatchInput){.flow = 16,
                              .group = (int32_t)s->state->frame.group,
                              .event_state = s->state->frame.phase,
                              .root_721b28 = roots[0],
                              .root_721b2c = roots[1],
                              .root_721b34 = roots[2]};
  BkEndingSpecialBindings bindings = {
      .frame = &s->state->frame,
      .action_variant = &s->state->auxiliary.variant,
      .camera_variant = &s->state->control.variant,
      .restore_hidden = &s->state->control.toggles[1],
      .camera_index = &s->state->auxiliary.index,
      .offset_mode = &s->state->auxiliary.selection,
      .cameras = s->state->special_cameras,
      .target = ending_cached_target(s, 0),
      .primary_root = roots,
      .auxiliary_root = roots + 1};
  /*51c736/4d9733 consumes the live draw mode and speech1 state, including
   * phase9. Dual views capture independent geometry before camera/material
   * restoration; phase7 and pure redraws never consume event/audio time. */
  BkEndingEventRenderInput event = {
      .roots = in,
      .event_mode = s->state->control.mode_721ec4,
      .scene = {.bindings = &bindings,
                 .camera = &s->camera,
                 .fog = &fog,
                 .primary_materials = s->materials,
                 .materials = s->special_materials,
                 .material_count = s->special_material_count,
                 .bom_disabled = s->secondary_assets ? NULL : s->disabled,
                 .bom_count = s->disabled_count,
                 .main_viewport = s->viewport,
                 .special_viewport = s->special_viewport},
      .audio = s->audio,
      .duck_transition = s->duck_transition,
      .voice_master = s->voice_volume,
      .seconds = s->active_seconds,
      .movie_clock = (int32_t)s->now_ms,
      .movie_restart_clock = (int32_t)(s->now_ms + 1u)};
  return bk_ending_normal_render_prepare_event(s->render, &event, e);
}

static int prepare_frame(EndingNormalScene *s, float seconds, char e[256]) {
  BkActorPose *primary, *camera_track;
  if (!s || (!s->final_image && (!scene_forest(s) || !s->render)) ||
      (s->final_image && (!s->frame_active || scene_forest(s) || s->render)) ||
      !isfinite(seconds) || seconds <= 0 ||
      seconds > 1)
    return fail(e, "invalid live normal scene");
  s->active_seconds = seconds;
  double wall = s->wall_seconds + (s->clock_supplied ? 0.0 : (double)seconds);
  if (!isfinite(wall) || wall < 0 || wall > 1e12)
    return fail(e, "invalid ending wall clock");
  s->wall_seconds = wall;
  s->now_ms = (uint32_t)(uint64_t)(wall * 1000.0);
  s->clock_supplied = 0;
  release_retired(&s->retired);
  s->snapshot_render = NULL;
  s->snapshot_ui_only = 0;
  if (!bk_ending_ui_batch_begin(s->ui_batch, s->ui_render, s->ui_stage_render,
                                e))
    return 0;
  if (s->frame_active) {
    if (!prepare_story_frame(s, e))
      return 0;
  } else {
    if (!s->assets)
      return fail(e, "secondary scene requires its actual frame dispatcher");
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
            s->now_ms, s->now_ms, s->now_ms, s->now_ms, s->random, e) ||
        !bk_actor_forest_refresh(bk_ending_normal_assets_forest(s->assets), e))
      return 0;
  }
  if (!s->final_image && !prepare_scene_draw(s, e))
    return 0;
  s->snapshot_render = s->render;
  s->snapshot_ui_only = s->final_image;
  if (!prepare_ui_frame(s, seconds, s->viewport.width, s->viewport.height, e))
    return 0;
  s->pending = 1;
  s->valid = 1;
  s->drawn = 0;
  return 1;
}

static int step(void *context, double seconds, const BkInput *input,
                char e[256]) {
  EndingNormalScene *s = context;
  if (s && s->stopped)
    return fail(e, "ending scene is stopped");
  if (!s || !input || s->pending || s->failed)
    return fail(e, "previous ending snapshot not presented");
  if (!isfinite(seconds) || seconds <= 0 || seconds > 1 ||
      !bk_virtual_pointer_step(&s->pointer, &s->viewport, input, seconds, e))
    return fail(e, "invalid ending pointer input/time");
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
  if (!s || (!s->pending && !s->valid) ||
      (!s->snapshot_render && !s->snapshot_ui_only))
    return fail(e, "no ending snapshot");
  if (!bk_renderer_viewport(s->services.renderer, &s->viewport, e))
    return 0;
  int ok = (!s->snapshot_render || bk_ending_normal_render_draw(s->snapshot_render, e)) &&
           bk_ending_ui_batch_draw(s->ui_batch, e);
  char restore_error[256];
  int restored = bk_renderer_viewport(s->services.renderer, NULL,
                                      ok ? e : restore_error);
  if (!ok || !restored)
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
                             const uint8_t unlocked[5][8],
                             const BkEndingNormalFlow *flow,
                             char e[256]) {
  if (!services || !services->renderer || !services->resources ||
      !services->audio || group >= 5 || variant > 1)
    return NULL;
  if (flow && (!flow->common || !flow->schedule || !flow->state || !flow->auxiliary_cycle ||
               !flow->random || !flow->normal_controller || !flow->presentation ||
               !flow->duck_transition ||
               !isfinite(flow->wall_seconds) || flow->wall_seconds < 0 ||
               flow->wall_seconds > 1e12)) {
    fail(e, "shared process owners, scheduler and valid wall clock are required");
    return NULL;
  }
  if (flow && previous == 0x18 && selected == 1 &&
      (!flow->secondary_controller || !flow->secondary_presentation)) {
    fail(e, "application secondary entry requires retained process owners");
    return NULL;
  }
  if (flow && previous == 8 && variant && !flow->tertiary_controller) {
    fail(e, "application third entry requires its retained controller");
    return NULL;
  }
  EndingNormalScene *s = calloc(1, sizeof(*s));
  if (!s)
    return NULL;
  s->services = *services;
  s->state = flow ? flow->state : &s->diagnostic_state;
  if (!flow)
    bk_ending_state_initialize(s->state);
  s->voice_volume = -1000;
  s->effect_volume = -600;
  s->group = group;
  s->variant = variant;
  s->records = records;
  s->gallery_selection = selected;
  s->frame_active = records != NULL || (previous == 0x18 &&
      (selected == 1 || selected == 2 || selected == 5));
  s->previous_flow = previous;
  s->diagnostic_random =
      UINT32_C(0x4cc582) ^ (group * UINT32_C(0x9e3779b9));
  s->random = &s->diagnostic_random;
  s->overlay = (BkFadeSprite){0, 2, 0};
  s->diagnostic_cycle = bk_ending_auxiliary_cycle_initial();
  s->auxiliary_cycle = &s->diagnostic_cycle;
  bk_ending_normal_controller_initialize(&s->diagnostic_controller);
  s->normal_controller = &s->diagnostic_controller;
  s->presentation = &s->diagnostic_presentation;
  s->duck_transition = &s->diagnostic_duck_transition;
  s->diagnostic_secondary_controller = bk_ending_secondary_control_initial();
  s->secondary_controller = &s->diagnostic_secondary_controller;
  s->diagnostic_secondary_presentation = bk_ending_secondary_presentation_initial();
  s->secondary_presentation = &s->diagnostic_secondary_presentation;
  bk_ending_tertiary_controller_initialize(&s->diagnostic_tertiary_controller);
  s->tertiary_controller = &s->diagnostic_tertiary_controller;
  s->diagnostic_selected_controller = bk_ending_selected_control_initial();
  s->selected_controller = &s->diagnostic_selected_controller;
  s->selected_action = bk_ending_selected_action_initial();
  s->diagnostic_selected_cycle = bk_ending_selected_cycle_initial();
  s->selected_cycle = &s->diagnostic_selected_cycle;
  if (flow) {
    s->flow = *flow;
    s->common = flow->common;
    s->auxiliary_cycle = flow->auxiliary_cycle;
    s->random = flow->random;
    s->normal_controller = flow->normal_controller;
    s->presentation = flow->presentation;
    s->duck_transition = flow->duck_transition;
    if (flow->secondary_controller)
      s->secondary_controller = flow->secondary_controller;
    if (flow->secondary_presentation)
      s->secondary_presentation = flow->secondary_presentation;
    if (flow->tertiary_controller)
      s->tertiary_controller = flow->tertiary_controller;
    s->wall_seconds = flow->wall_seconds;
    s->now_ms = (uint32_t)(uint64_t)(s->wall_seconds * 1000.0);
    s->clock_supplied = 1;
  } else {
    s->common = &s->diagnostic_common;
    bk_common_hud_initialize(s->common);
  }
  unsigned width, height;
  bk_renderer_extent(services->renderer, &width, &height);
  if (!bk_camera_fit(&s->viewport, width, height, 4, 3)) {
    fail(e, "cannot fit ending content viewport");
    goto bad;
  }
  width = s->viewport.width;
  height = s->viewport.height;
  float scale = (float)((double)width / 1280.0);
  BkEndingStateOps state_ops = {s, warp};
  if (!bk_menu_camera_dialogue(&s->camera) ||
      !bk_ending_state_begin(s->state, &s->overlay, group, variant, scale,
                             (int32_t[2]){0, 0}, &state_ops, e))
    goto bad;
  /*721e10/7220f0 lie inside the original4cc582 clear interval; the other
   * normal controller fields are separate retained process globals. */
  s->normal_controller->control.action_kind = 0;
  s->normal_controller->control.action_column = 0;
  if (!bk_ending_ui_initialize(&s->ui, width, s->state->control.pause_flags,
                               &s->state->gauge_y, e) ||
      !bk_ending_special_viewport(&s->special_viewport, &s->viewport, e) ||
      !bk_ending_ui_control_rects(&s->ui, s->control_rects))
    goto bad;
  BkEndingEntryBindings bindings;
  if (!bk_ending_state_entry_bindings(s->state, &s->option_a, &s->option_b,
                                      &s->selected_group, &bindings))
    goto bad;
  BkEndingEntryOps ops = {s, normal_load, records ? clear_record : NULL, NULL};
  /*The implemented loaders are selected by the original entry table;
   * camera variant1 is never substituted for normal action variant1. */
  if (!bk_ending_entry_dispatch(&bindings, previous, selected, scale, &ops, e))
    goto bad;
  if (!scene_forest(s) || !s->render || !s->audio)
    goto bad;
  /*4eb8f6 calls50ca48 AFTER4cc582 and its selected loader return. Preserve
   * non-boolean bytes and all other groups before any ending events run. */
  if (unlocked)
    memcpy(s->state->working, unlocked, sizeof(s->state->working));
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
  return create_entry(services, group, variant, 0x18, 0, NULL, NULL,
                       NULL, e);
}

BkScene *bk_ending_normal_scene_create_story(const BkSceneServices *services,
                                             unsigned group, unsigned variant,
                                             BkEndingRecords *records,
                                             const uint8_t unlocked[5][8],
                                             const BkEndingNormalFlow *flow,
                                             char e[256]) {
  if (!records || !unlocked) {
    fail(e, "story records and saved unlock table are required");
    return NULL;
  }
  return create_entry(services, group, variant, 8, variant, records, unlocked,
                       flow, e);
}

BkScene *bk_ending_secondary_scene_create_gallery(const BkSceneServices *services,
    unsigned group, unsigned variant, const uint8_t unlocked[5][8],
    const BkEndingNormalFlow *flow, char e[256]) {
  if (!unlocked) {
    fail(e, "secondary gallery entry requires the saved unlock table");
    return NULL;
  }
  return create_entry(services, group, variant, 0x18, 1, NULL, unlocked, flow, e);
}

BkScene *bk_ending_selected_scene_create_gallery(const BkSceneServices *services,
    unsigned group, unsigned variant, uint32_t selection,
    const uint8_t unlocked[5][8], const BkEndingNormalFlow *flow,
    char e[256]) {
  if (selection != 2 && selection != 5) {
    fail(e, "selected gallery entry must use selection2 or selection5");
    return NULL;
  }
  if (!unlocked) {
    fail(e, "selected gallery entry requires the saved unlock table");
    return NULL;
  }
  return create_entry(services, group, variant, 0x18, selection, NULL,
                      unlocked, flow, e);
}

int bk_ending_normal_scene_stop(BkScene *scene, char e[256]) {
  return stop_audio(bk_scene_custom_context(scene), e);
}

int bk_ending_normal_scene_step_at(BkScene *scene, double seconds, double wall,
                                   const BkInput *input, char e[256]) {
  EndingNormalScene *s = bk_scene_custom_context(scene);
  if (!s || !input || s->stopped || s->pending || s->failed ||
      !isfinite(seconds) || seconds <= 0 || seconds > 1 ||
      !isfinite(wall) || wall < s->wall_seconds || wall < 0 || wall > 1e12)
    return fail(e, "invalid ending game/wall clock or unpresented snapshot");
  s->wall_seconds = wall;
  s->clock_supplied = 1;
  return bk_scene_step(scene, seconds, input, e);
}

double bk_ending_normal_scene_wall_seconds(const BkScene *scene) {
  EndingNormalScene *s = bk_scene_custom_context((BkScene *)(uintptr_t)scene);
  return s ? s->wall_seconds : 0;
}
uint32_t bk_ending_normal_scene_milliseconds(const BkScene *scene) {
  EndingNormalScene *s = bk_scene_custom_context((BkScene *)(uintptr_t)scene);
  return s ? s->now_ms : 0;
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
  return s ? s->state : NULL;
}
