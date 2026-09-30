#include "scene/ending_state.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
void bk_ending_state_initialize(BkEndingState *s) {
  if (!s)
    return;
  memset(s, 0, sizeof(*s));
  s->ui_controller.normal.uv_right = .25f; /*575638*/
  s->ui_controller.auxiliary.reset_b = INT32_C(0x3f000000); /*5546a4 raw float bits*/
  s->retained.auxiliary.word_5546a0 = -1;
  s->retained.stage2.word_54ccc8 = -1;
  s->retained.stage2.value_54ccd0 = 1;
  s->retained.stage4.word_54e2f8 = -1;
  s->retained.stage4.delay_54f8e0 = 30;
  s->retained.stage4.value_54e310 = 1;
}
static int fail(char e[256], const char *why) {
  if (e)
    snprintf(e, 256, "ending state: %s", why);
  return 0;
}
int bk_ending_state_leave(BkEndingState *s, BkEndingLeave operation,
                          char e[256]) {
  if (!s || (unsigned)operation > BK_ENDING_LEAVE_48D7F2)
    return fail(e, "invalid retained-state reset");
  switch (operation) {
  case BK_ENDING_LEAVE_4E1C8F: {
    BkEndingRetainedNormal *n = &s->retained.normal;
    s->normal_ready = s->next_mode = 0;
    n->word_719b24 = n->word_719448 = n->word_719b28 = n->word_719b2c = 0;
    n->word_719b40 = 0;
    s->normal_side = 0;
    n->word_719b50 = 0;
    memset(n->words_719b54, 0, sizeof(n->words_719b54));
    break;
  }
  case BK_ENDING_LEAVE_47DBCC:
    memset(s->retained.stage3.words_6bbe2c, 0,
           sizeof(s->retained.stage3.words_6bbe2c));
    s->retained.stage3.byte_6bbe34 = 0;
    memset(s->auxiliary_saved_target, 0, sizeof(s->auxiliary_saved_target));
    break;
  case BK_ENDING_LEAVE_49739A: {
    BkEndingRetainedAuxiliary *a = &s->retained.auxiliary;
    BkEndingUiAuxNotice *ui = &s->ui_controller.auxiliary;
    ui->mode = 0;
    a->word_6ea16c = 0;
    memset(s->aux_inputs, 0, sizeof(s->aux_inputs));
    memset(ui->processed, 0, sizeof(ui->processed));
    ui->reset_c = 0;
    memset(a->group_prefix, 0, sizeof(a->group_prefix));
    memset(ui->group_seen, 0, sizeof(ui->group_seen));
    memset(a->group_suffix, 0, sizeof(a->group_suffix));
    memset(a->words_6ea2c0, 0, sizeof(a->words_6ea2c0));
    ui->once = a->word_6ea314 = 0;
    a->word_5546a0 = -1;
    memset(s->aux_config, 0, sizeof(s->aux_config));
    memset(a->words_6ea028, 0, sizeof(a->words_6ea028));
    memset(a->words_6ea318, 0, sizeof(a->words_6ea318));
    a->word_6ea340 = a->word_6ddec0 = ui->reset_a = a->word_6ddec8 = 0;
    a->word_6dde90 = a->word_6ea348 = 0;
    ui->sequence = ui->sequence_count = 0;
    ui->sequence_elapsed = 0;
    /* This existing owner is raw bits: the leave routine writes float.5,
     * while the UI tail writes integer0 to the same original address. */
    ui->reset_b = INT32_C(0x3f000000);
    a->word_6ea354 = a->word_6ea020 = 0;
    a->byte_6ea358 = 0;
    break;
  }
  case BK_ENDING_LEAVE_47A033: {
    BkEndingRetainedStage2 *a = &s->retained.stage2;
    a->word_6a3c20 = 0;
    memset(a->words_6afcfc, 0, sizeof(a->words_6afcfc));
    a->word_6afd08 = 0;
    memset(s->unavailable, 0, sizeof(s->unavailable));
    s->ui_controller.hints.movement_ready = 0;
    a->word_6a3c24 = 9;
    a->word_54ccc8 = -1;
    a->byte_6afd18 = 0;
    a->value_54ccd0 = 1;
    break;
  }
  case BK_ENDING_LEAVE_482F91: {
    BkEndingRetainedStage4 *a = &s->retained.stage4;
    memset(a->words_6c7f44, 0, sizeof(a->words_6c7f44));
    memset(a->bytes_6c7f54, 0, sizeof(a->bytes_6c7f54));
    memset(a->bytes_6c7f60, 0, sizeof(a->bytes_6c7f60));
    a->word_54e2f8 = -1;
    a->word_6c7f4c = 0;
    a->byte_6c7f50 = 0;
    a->timer_6c7f6c = 0;
    a->delay_54f8e0 = 30;
    a->value_54e310 = 1;
    memset(s->auxiliary_saved_orbit, 0, sizeof(s->auxiliary_saved_orbit));
    break;
  }
  case BK_ENDING_LEAVE_48D7F2: {
    BkEndingRetainedFinal *a = &s->retained.final;
    a->byte_6c7f70 = a->byte_6d1be0 = 0;
    s->final_state = 0;
    a->byte_6d1bd4 = a->byte_6d1c0d = a->byte_6ddce0 = a->byte_6d1be1 = 0;
    a->word_6c7f74 = a->word_6dde4c = a->word_6d1bcc = 0;
    a->byte_6dde50 = a->byte_6dde51 = 0;
    a->word_6d1bd8 = a->word_6d1bdc = a->word_6dde54 = 0;
    a->byte_6dde58 = 0;
    memset(a->workspace_6c7f80, 0, sizeof(a->workspace_6c7f80));
    memset(a->words_6dde24, 0, sizeof(a->words_6dde24));
    memset(a->words_6d1be8, 0, sizeof(a->words_6d1be8));
    memset(a->words_6ddce4, 0, sizeof(a->words_6ddce4));
    break;
  }
  }
  return 1;
}
/* Only modeled fields whose native address is in721b28..725713. Do not
 * memset these C aggregates: many fields in each have retained addresses. */
static void clear_block(BkEndingState *s) {
  BkEndingFrameState *f = &s->frame;
  f->phase = f->camera_mode = f->camera_clip = f->auxiliary_mode = 0;
  f->state_721ee0 = f->state_721ee4 = 0;
  memset(f->camera_values, 0, sizeof(f->camera_values));
  f->camera_request = f->camera_cached = f->camera_event = 0;
  f->group = 0;
  BkEndingControlState *c = &s->control;
  c->mode_721ec4 = c->target_choice = c->state_721eec = 0;
  c->variant = 0;
  memset(c->toggles, 0, sizeof(c->toggles));
  BkEndingAuxiliaryState *a = &s->auxiliary;
  a->gate = a->variant = a->selection = a->index = a->pending = 0;
  a->progress = 0;
  a->expression_a = a->expression_b = 0;
  s->ui_controller.auxiliary.choice = 0;
  s->selected = s->stage3_state = s->open = s->contact_index = 0;
  s->eye_lower = s->face_mode = 0;
  s->gauge_y = 0;
  memset(s->node_state, 0, sizeof(s->node_state));
  memset(s->targets, 0, sizeof(s->targets));
  memset(s->points, 0, sizeof(s->points));
  memset(s->choices, 0, sizeof(s->choices));
  memset(s->working, 0, sizeof(s->working));
  memset(s->speech_names, 0, sizeof(s->speech_names));
}
int bk_ending_state_begin(BkEndingState *s, BkFadeSprite *overlay,
                          unsigned group, int8_t variant, float scale,
                          const int32_t origin[2], const BkEndingStateOps *ops,
                          char e[256]) {
  if (!s || !overlay || !origin || !ops || !ops->warp || group >= 5 ||
      !isfinite(scale) || scale <= 0 || scale > 16)
    return fail(e, "invalid input or missing pointer service");
  float x = (float)(640.0 * (double)scale + origin[0]);
  float y = (float)(480.0 * (double)scale + origin[1]);
  if (!ops->warp(ops->context, x, y, e))
    return 0;
  clear_block(s);
  memset(s->normal_processed, 0, sizeof(s->normal_processed));
  if (!bk_fade_sprite_request(overlay, 0))
    return fail(e, "invalid previous overlay");
  s->selected = s->frame.camera_cached = -1;
  s->frame.camera_request = 1;
  memset(s->normal_inputs, 0, sizeof(s->normal_inputs));
  s->frame.group = (uint8_t)group;
  s->auxiliary.variant = variant != 0;
  memset(s->model_paths, 0, sizeof(s->model_paths));
  memset(s->special_cameras, 0, sizeof(s->special_cameras));
  static const unsigned suffixes[] = {0, 2, 3, 4, 5, 10, 12, 13, 14, 15};
  for (unsigned i = 0; i < 10; i++)
    snprintf(s->model_paths[i], sizeof(s->model_paths[i]), "\\h%02u_%02u.xan",
             group + 1, suffixes[i]);
  if (!bk_ending_special_cameras(s->special_cameras, group))
    return fail(e, "invalid camera table");
  return 1;
}
int bk_ending_state_entry_bindings(BkEndingState *s, uint8_t *oa, uint8_t *ob,
                                   int32_t *selected,
                                   BkEndingEntryBindings *out) {
  if (!s || !oa || !ob || !selected || !out)
    return 0;
  *out = (BkEndingEntryBindings){&s->frame, &s->control, &s->auxiliary, oa,
                                 ob,        selected,    &s->gauge_y};
  return 1;
}
int bk_ending_state_gallery_bindings(BkEndingState *s, const BkEndingRecords *records,
                                     const int32_t *volume,
                                     BkEndingGalleryControlBindings *out) {
  if (!s || !records || !volume || !out) return 0;
  BkEndingRetainedFinal *a = &s->retained.final;
  *out = (BkEndingGalleryControlBindings){
      &s->frame, &s->control, &s->auxiliary, records,
      &a->byte_6ddce0, &a->byte_6d1be1, &a->byte_6dde58,
      &a->byte_6c7f70, &a->byte_6d1be0, &a->byte_6d1bd4, &a->byte_6d1c0d,
      &a->byte_6dde50, &s->final_state, &a->word_6c7f74, a->workspace_6c7f80,
      BK_ENDING_RECORD_CAPACITY, a->words_6dde24, &a->word_6d1bcc,
      &a->word_6dde4c, &s->next_mode, s->speech_names[0], volume};
  return 1;
}
int bk_ending_state_gallery_normal_bindings(BkEndingState *s,
    const int32_t *voice, const int32_t *effect, BkEndingGalleryNormalBindings *out) {
  if (!s || !voice || !effect || !out) return 0;
  BkEndingRetainedFinal *a = &s->retained.final;
  *out = (BkEndingGalleryNormalBindings){
      &s->frame, &s->control, &s->auxiliary, &a->byte_6c7f70, &a->byte_6dde58,
      &a->word_6c7f74, a->workspace_6c7f80, BK_ENDING_RECORD_CAPACITY,
      &a->word_6d1bcc, s->speech_names, voice, effect};
  return 1;
}
int bk_ending_state_gallery_secondary_bindings(BkEndingState *s, BkMenuCamera *camera,
    BkEndingGalleryCameraState *saved, BkEndingGallerySecondaryBindings *out) {
  if (!s || !camera || !saved || !out) return 0;
  BkEndingRetainedFinal *a = &s->retained.final;
  *out = (BkEndingGallerySecondaryBindings){
      &s->frame, &s->control, &s->auxiliary, camera, saved,
      &a->byte_6d1be0, &a->byte_6dde58, &a->word_6c7f74, a->words_6dde24,
      &a->word_6d1bd8, &a->word_6d1bdc, &a->word_6dde54};
  return 1;
}
int bk_ending_state_gallery_selected_bindings(BkEndingState *s,
    const BkEndingGallerySelectedViews *v, BkEndingGallerySelectedBindings *out) {
  if (!s || !v || !out || !v->camera || !v->presets || !v->saved ||
      !v->expression_override || !v->fade_stage || !v->flash_wanted ||
      !v->action || !v->curtain_wanted || !v->voice_volume || !v->effect_volume) return 0;
  BkEndingRetainedFinal *a = &s->retained.final;
  *out = (BkEndingGallerySelectedBindings){
      &s->frame, &s->control, &s->auxiliary, v->camera, v->presets, v->saved,
      &s->final_state, &a->byte_6dde58, &a->word_6c7f74, a->workspace_6c7f80,
      BK_ENDING_RECORD_CAPACITY, a->words_6dde24, a->words_6ddce4,
      &a->word_6d1bd8, &a->word_6d1bdc, &a->word_6dde54,
      &s->open, v->expression_override, &s->face_mode,
      v->fade_stage, v->flash_wanted, v->action, v->curtain_wanted,
      s->speech_names[0], v->voice_volume, v->effect_volume};
  return 1;
}
int bk_ending_state_gallery_tertiary_bindings(BkEndingState *s,
    BkEndingGalleryNormalState *normal, const BkEndingGalleryTertiaryViews *v,
    BkEndingGalleryTertiaryBindings *out) {
  if (!s || !normal || !v || !out || !v->reverse || !v->expression_override ||
      !v->expression_latch || !v->voice_volume || !v->effect_volume) return 0;
  BkEndingRetainedFinal *a = &s->retained.final;
  *out = (BkEndingGalleryTertiaryBindings){
      &s->frame, &s->control, &s->auxiliary, &a->byte_6d1bd4, &normal->clip,
      &a->byte_6dde50, &a->byte_6dde51, v->expression_latch,
      &a->word_6c7f74, a->workspace_6c7f80, BK_ENDING_RECORD_CAPACITY,
      a->words_6dde24, &a->word_6d1bcc, v->reverse, &s->face_mode,
      v->expression_override, v->voice_volume, v->effect_volume};
  return 1;
}
int bk_ending_state_gallery_auxiliary_bindings(BkEndingState *s,
    const BkEndingGalleryAuxiliaryViews *v, BkEndingGalleryAuxiliaryBindings *out) {
  if(!s||!v||!out||!v->camera||!v->presets||!v->saved||
      !v->expression_override||!v->effect_volume) return 0;
  BkEndingRetainedFinal *a=&s->retained.final;
  *out=(BkEndingGalleryAuxiliaryBindings){&s->frame,&s->control,&s->auxiliary,
      v->camera,v->presets,v->saved,&a->byte_6d1c0d,&a->byte_6dde58,
      &a->word_6c7f74,a->words_6dde24,&a->word_6d1bcc,
      v->expression_override,&s->face_mode,v->effect_volume};
  return 1;
}
int bk_ending_state_reload_bindings(BkEndingState *s, BkCommonHudState *common,
                                    const int8_t *previous,
                                    BkEndingReloadBindings *out) {
  if (!s || !common || !previous || !out)
    return 0;
  *out =
      (BkEndingReloadBindings){&s->frame,    &s->control,     &s->auxiliary,
                               previous,     &common->action, &common->blocked,
                               &s->selected, &s->next_mode,   s->saved_toggles};
  return 1;
}
int bk_ending_state_import_frame_aliases(BkEndingState *s,
                                         const BkCommonHudState *c,
                                         const BkEndingStageUi *ui) {
  if (!s || !c || !ui)
    return 0;
  s->frame.finish_fade_stage = ui->sprites[11].transform.fade.stage;
  s->frame.finish_blocked = c->curtain.stage;
  s->frame.transition_action = c->action;
  s->frame.curtain_wanted = c->blocked;
  return 1;
}
int bk_ending_state_export_frame_requests(const BkEndingState *s,
                                          BkCommonHudState *c) {
  if (!s || !c)
    return 0;
  c->action = s->frame.transition_action;
  c->blocked = s->frame.curtain_wanted;
  return 1;
}

int bk_ending_state_ui_bindings(BkEndingState *s, BkCommonHudState *common,
                                const BkEndingStateUiViews *v,
                                BkEndingUiFrameBindings *out) {
  if (!s || !common || !v || !v->notices || !v->previous_flow || !out)
    return 0;
  BkEndingUiFrameBindings b = {
      .common = common,
      .toolbar = {&s->frame, &s->control, &s->auxiliary, &s->open},
      .hints = {&s->frame, &s->control, &s->auxiliary, &s->stage3_state,
                v->active_clip, s->points, s->choices, s->targets, 39,
                s->alternate, &s->gauge_y, v->active_timing},
      .select = {&s->frame, &s->control, &s->auxiliary, &s->stage3_state,
                 v->active_clip, &s->open, v->actions, s->targets, 39,
                 v->camera_local, v->pick, s->unavailable, v->item},
      .cursor = {&s->frame, &s->auxiliary, v->active_clip, &s->normal_ready,
                 v->notices},
      .tail = {&s->frame, &s->control, &s->auxiliary, v->notices,
               v->flash_wanted, &s->final_state, &s->normal_side,
               &s->normal_target, s->normal_inputs, s->normal_processed,
               &s->gauge_y, &s->contact_index, s->aux_inputs, s->aux_config,
               v->random, s->speech_names, v->voice_volume}};
  if (!bk_ending_state_reload_bindings(s, common, v->previous_flow, &b.reload))
    return 0;
  *out = b;
  return 1;
}
