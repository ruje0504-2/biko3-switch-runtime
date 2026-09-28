#include "scene/ending_state.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
static int fail(char e[256], const char *why) {
  if (e)
    snprintf(e, 256, "ending state: %s", why);
  return 0;
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
