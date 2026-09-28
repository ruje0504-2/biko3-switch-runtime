#include "scene/ending_ui_frame.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
static int fail(char e[256], const char *why) {
  if (e)
    snprintf(e, 256, "ending UI frame: %s", why);
  return 0;
}
static int bound(const BkEndingUiFrameBindings *b) {
  if (!b || !b->common || !b->toolbar.frame || !b->toolbar.control ||
      !b->toolbar.auxiliary || !b->toolbar.open)
    return 0;
  const BkEndingFrameState *f = b->toolbar.frame;
  const BkEndingControlState *c = b->toolbar.control;
  const BkEndingAuxiliaryState *a = b->toolbar.auxiliary;
  return b->hints.frame == f && b->select.frame == f && b->cursor.frame == f &&
         b->reload.frame == f && b->tail.frame == f && b->hints.control == c &&
         b->select.control == c && b->reload.control == c &&
         b->tail.control == c && b->hints.auxiliary == a &&
         b->select.auxiliary == a && b->cursor.auxiliary == a &&
         b->reload.auxiliary == a && b->tail.auxiliary == a &&
         b->select.open == b->toolbar.open &&
         b->hints.stage3_state == b->select.stage3_state &&
         b->hints.active_clip == b->select.active_clip &&
         b->cursor.active_clip == b->select.active_clip &&
         b->hints.targets == b->select.targets &&
         b->hints.target_count == b->select.target_count &&
         b->hints.gauge_y == b->tail.gauge_y &&
         b->cursor.notices == b->tail.notices &&
         b->reload.action == &b->common->action &&
         b->reload.curtain_wanted == &b->common->blocked;
}
int bk_ending_ui_frame(BkEndingUi *ui, BkEndingStageUi *stage,
                       BkEndingUiController *s,
                       const BkEndingUiFrameBindings *b,
                       const BkEndingUiFrameOps *ops, float scale,
                       float seconds, BkEndingUiCompositeFrame *out,
                       char e[256]) {
  if (out)
    out->complete = 0;
  if (!ui || !stage || !s || !out || !ops || !bound(b) || !isfinite(scale) ||
      scale <= 0 || scale > 16 || !isfinite(seconds) || seconds < 0)
    return fail(e, "invalid shared owners/time");
  memset(out, 0, sizeof(*out));
  if (b->toolbar.frame->phase == 7 &&
      !bk_ending_ui_dispatch_sprite(ui, stage, 74, seconds, &out->sprites, e))
    return 0;
  if (!bk_ending_ui_hints_start(&s->hints, scale, e))
    return 0;
  float pointer[2], motion[2];
  if (!ops->position || !ops->position(ops->context, pointer, e))
    return fail(e, "position capture failed or missing");
  if (!ops->motion || !ops->motion(ops->context, motion, e))
    return fail(e, "motion capture failed or missing");
  uint8_t visible;
  BkEndingUiHoverOps hover = {ops->context, ops->key};
  BkEndingUiSelectOps select = {ops->context, ops->key, ops->voice_playing};
  int32_t selected;
  if (!bk_ending_ui_toolbar_shared(ui, stage, &b->toolbar, &b->common->blocked,
                                   pointer, scale, seconds, &hover, &visible,
                                   &out->sprites, e) ||
      !bk_ending_ui_hints(ui, stage, &s->hints, &b->hints, pointer, motion,
                          scale, seconds, &out->sprites, e))
    return 0;
  if (b->select.frame->phase == 7 && visible != 1)
    selected = -1; /* Defined safe policy for the proven uninitialized path. */
  else if (!bk_ending_ui_select(ui, stage, &b->select, pointer, seconds,
                                visible, &select, &selected, &out->sprites, e))
    return 0;
  if (!bk_ending_ui_cursor(ui, stage, &b->cursor, selected, visible, pointer,
                           seconds, &out->sprites, e) ||
      !bk_ending_ui_curtain(b->common, seconds, &out->curtain, e))
    return 0;
  out->curtain_after = out->sprites.count;
  if (!bk_ending_reload_transition(&b->reload, b->common->curtain.stage,
                                   &ops->reload, &out->early_return, e))
    return 0;
  if (!out->early_return &&
      !bk_ending_ui_tail(ui, stage, &s->normal, &s->auxiliary, &b->tail,
                         &ops->tail, scale, seconds, &out->sprites, e))
    return 0;
  out->complete = 1;
  return 1;
}
