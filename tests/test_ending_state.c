#include "scene/ending_state.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef struct {
  BkEndingState *state;
  unsigned calls;
  int fail;
} Test;
static int warp(void *p, float x, float y, char e[256]) {
  (void)e;
  Test *t = p;
  t->calls++;
  assert(x == 327.0f && y == 231.0f);
  assert(t->state->frame.phase == 73 && t->state->selected == 84);
  return !t->fail;
}
static int capture(void *p, float out[2], char e[256]) {
  (void)p;
  (void)e;
  out[0] = out[1] = 0;
  return 1;
}
int main(void) {
  BkEndingState s = {0}, old;
  BkFadeSprite overlay = {.alpha = .5f, .speed = 2, .stage = 3};
  s.frame.phase = 73;
  s.selected = 84;
  s.normal_inputs[3] = 23;
  s.normal_processed[2] = 41;
  s.auxiliary.base = 5;
  s.auxiliary.direction = 9;
  s.frame.previous_clock = 19;
  s.control.hover = 23;
  s.ui_controller.normal.cycles = 8;
  s.normal_side = 1;
  old = s;
  char e[256];
  Test t = {&s, 0, 1};
  BkEndingStateOps ops = {&t, warp};
  assert(!bk_ending_state_begin(&s, &overlay, 0, 0, .5f, (int32_t[2]){7, -9},
                                &ops, e));
  assert(!memcmp(&s, &old, sizeof(s)) && overlay.stage == 3 && t.calls == 1);
  t.fail = 0;
  overlay.stage = 7;
  assert(!bk_ending_state_begin(&s, &overlay, 0, 0, .5f, (int32_t[2]){7, -9},
                                &ops, e));
  assert(!s.frame.phase && !s.selected && !s.normal_processed[2]);
  assert(s.normal_inputs[3] == 23 &&
         s.frame.camera_request == 0); /* native prefix */
  s = old;
  overlay.stage = 3;
  assert(bk_ending_state_begin(&s, &overlay, 4, -128, .5f, (int32_t[2]){7, -9},
                               &ops, e));
  assert(s.frame.group == 4 && s.auxiliary.variant == 1 && s.selected == -1 &&
         s.frame.camera_cached == -1 && s.frame.camera_request == 1 &&
         overlay.stage == 4);
  assert(s.auxiliary.base == 5 && s.auxiliary.direction == 9 &&
         s.frame.previous_clock == 19 && s.control.hover == 23 &&
         s.ui_controller.normal.cycles == 8 && s.normal_side == 1);
  assert(!s.normal_inputs[3] && !s.normal_processed[2]);
  assert(!strcmp(s.model_paths[9], "\\h05_15.xan"));
  unsigned before = t.calls;
  assert(!bk_ending_state_begin(&s, &overlay, 5, 0, .5f, (int32_t[2]){0}, &ops,
                                e));
  assert(t.calls == before);
  uint8_t oa = 1, ob = 2;
  int32_t selected = 3;
  int8_t previous = 8;
  BkEndingEntryBindings entry;
  assert(bk_ending_state_entry_bindings(&s, &oa, &ob, &selected, &entry));
  assert(entry.frame == &s.frame && entry.gauge_y == &s.gauge_y &&
         entry.selected_group == &selected);
  BkCommonHudState common = {.action = 74, .blocked = 1, .curtain = {1, 2, 3}};
  BkEndingStageUi stage = {0};
  stage.sprites[11].transform.fade.stage = 5;
  assert(bk_ending_state_import_frame_aliases(&s, &common, &stage));
  assert(s.frame.finish_fade_stage == 5 && s.frame.finish_blocked == 3 &&
         s.frame.transition_action == 74 && s.frame.curtain_wanted == 1);
  s.frame.transition_action = 49;
  s.frame.curtain_wanted = 0;
  s.frame.finish_blocked = 0;
  assert(bk_ending_state_export_frame_requests(&s, &common));
  assert(common.action == 49 && !common.blocked && common.curtain.stage == 3);
  BkEndingUiNoticeState notices = {0};
  BkEndingStateUiViews views = {.notices = &notices,
                                .previous_flow = &previous};
  BkEndingUiFrameBindings bindings;
  assert(bk_ending_state_ui_bindings(&s, &common, &views, &bindings));
  BkEndingUi ui = {0};
  assert(
      bk_ending_ui_initialize(&ui, 640, s.control.pause_flags, &s.gauge_y, e));
  s.frame.phase = 9;
  common.action = 0;
  common.curtain = (BkFadeSprite){0, 2, 0};
  BkEndingUiFrameOps uops = {.position = capture, .motion = capture};
  BkEndingUiCompositeFrame out;
  assert(bk_ending_ui_frame(&ui, &stage, &s.ui_controller, &bindings, &uops,
                            .5f, .1f, &out, e));
  assert(out.complete && out.sprites.count > 0 &&
         bindings.tail.gauge_y == &s.gauge_y &&
         bindings.reload.action == &common.action);
  /* Rejected binding creation preserves previously published bindings. */
  BkEndingUiFrameBindings saved = bindings;
  views.previous_flow = NULL;
  assert(!bk_ending_state_ui_bindings(&s, &common, &views, &bindings));
  assert(!memcmp(&saved, &bindings, sizeof(saved)));
  puts("ending state reset/retention/live-owner composition PASS");
}
