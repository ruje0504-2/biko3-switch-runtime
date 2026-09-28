#include "scene/ending_ui_hints.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
int main(void) {
  BkEndingUi ui = {0};
  BkEndingStageUi stage = {0};
  BkEndingFrameState frame = {.phase = 3, .camera_cached = 0};
  BkEndingControlState control = {0};
  BkEndingAuxiliaryState aux = {.gate = 3, .progress = .5f};
  BkEndingUiHintsState state = {.movement_ready = 99, .once_flags = 128};
  int32_t gate = 3, clip = 7;
  int32_t points[3][2] = {{100, 100}, {100, 100}, {100, 100}};
  int32_t choices[3] = {0, 1, 2}, targets[5][2] = {{300, 300}},
          alternate[2] = {20, 30};
  float gauge = 100;
  BkClipTiming timing = {.start = 100, .source = 110};
  BkEndingUiHintsBindings b = {&frame, &control,  &aux,    &gate,
                               &clip,  points,    choices, targets,
                               5,      alternate, &gauge,  &timing};
  const float pointer[2] = {100, 100}, motion[2] = {-1000, 1000};
  BkEndingUiFrame out = {0};
  char e[256];
  assert(bk_ending_ui_initialize(&ui, 1280, control.pause_flags, &gauge, e));
  assert(bk_ending_stage_ui_initialize(&ui, &stage, BK_ENDING_UI_THIRD, 0, 0,
                                       1280, e));
  assert(bk_ending_ui_hints_start(&state, 1, e));
  assert(state.length == 130 && state.once_flags == 129 &&
         state.movement_ready == 99);
  assert(bk_ending_ui_hints_start(&state, .5f, e) && state.length == 130);
  state.length = 399;
  assert(bk_ending_ui_hints(&ui, &stage, &state, &b, pointer, motion, 1, 0,
                            &out, e));
  assert(state.length == 400 && state.movement_ready == 1 && out.count == 15);
  unsigned rings = 0, lines = 0;
  for (unsigned i = 0; i < out.count; i++) {
    rings += out.draws[i].slot == 50;
    lines += out.draws[i].slot == 63;
  }
  assert(rings == 3 && lines == 3);
  /* Not ready suppresses rings without suppressing line/point/label draws. */
  out.count = 0;
  state.length = 130;
  assert(bk_ending_ui_hints(&ui, &stage, &state, &b, pointer, (float[2]){0, 0},
                            1, .1f, &out, e));
  assert(state.movement_ready == 0 && out.count == 12);
  gate = 2;
  state.movement_ready = 77;
  out.count = 0;
  assert(bk_ending_ui_hints(&ui, &stage, &state, &b, pointer, NULL, .5f, .1f,
                            &out, e));
  assert(state.length == 65 && state.movement_ready == 77 && out.count == 2);
  /* Skipped points don't read invalid unused target indices in phase5. */
  frame.phase = 5;
  frame.camera_event = 1;
  frame.camera_cached = -1;
  choices[0] = choices[1] = choices[2] = -1;
  out.count = 0;
  assert(bk_ending_ui_hints(&ui, &stage, &state, &b, pointer, NULL, 1, .1f,
                            &out, e));
  choices[0] = 0;
  out.count = 0;
  BkEndingUi before = ui;
  BkEndingStageUi before_stage = stage;
  assert(!bk_ending_ui_hints(&ui, &stage, &state, &b, pointer, NULL, 1, .1f,
                             &out, e));
  assert(!out.count && !memcmp(&before, &ui, sizeof(ui)) &&
         !memcmp(&before_stage, &stage, sizeof(stage)));
  /* Invalid event stops before the target point; meter still follows gauge. */
  assert(bk_ending_stage_ui_initialize(&ui, &stage, BK_ENDING_UI_AUXILIARY, 0,
                                       0, 1280, e));
  frame.camera_event = 0;
  clip = 6;
  out.count = 0;
  assert(bk_ending_ui_hints(&ui, &stage, &state, &b, pointer, NULL, 1, .1f,
                            &out, e));
  assert(out.count == 4 && out.draws[2].slot == 70 && out.draws[3].slot == 71);
  assert(state.meter_x == 1021 && stage.sprites[8].rect[0] == 1021);
  /* Timing failure retains the already drawn gauge/base meter prefix. */
  b.active_timing = NULL;
  out.count = 0;
  assert(!bk_ending_ui_hints(&ui, &stage, &state, &b, pointer, NULL, 1, .1f,
                             &out, e));
  assert(out.count == 3 && out.draws[2].slot == 70);
  /* Capacity failure retains the line snapshot but does not append a point. */
  frame.phase = 3;
  gate = 3;
  clip = 2;
  frame.camera_cached = 0;
  out.count = BK_ENDING_UI_DRAWS - 1;
  assert(!bk_ending_ui_hints(&ui, &stage, &state, &b, pointer, NULL, 1, .1f,
                             &out, e));
  assert(out.count == BK_ENDING_UI_DRAWS &&
         out.draws[out.count - 1].slot == 63);
  puts("PASS ending hints: branch gates, dt-independent growth, ready/retained "
       "states, skipped targets, meter timing and ordered failures");
  return 0;
}
