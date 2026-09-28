#include "game/ending_normal.h"
#include "scene/ending_ui_select.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef struct {
  unsigned keys, voices;
  int playing, fail_key;
  BkEndingAuxiliaryState *aux;
} Context;
static int key(void *p, unsigned code, unsigned mode, uint32_t *v,
               char e[256]) {
  (void)e;
  Context *c = p;
  assert(mode == 2 && code < 2);
  c->keys++;
  if (c->fail_key)
    return 0;
  *v = code ? 1 : 256;
  return 1;
}
static int voice(void *p, int *v, char e[256]) {
  (void)e;
  Context *c = p;
  c->voices++;
  *v = c->playing;
  return 1;
}
int main(void) {
  BkEndingUi ui = {0};
  BkEndingStageUi stage = {0};
  BkEndingUiFrame out = {0};
  BkEndingFrameState f = {
      .phase = 2, .state_721ee4 = 2, .camera_event = 1, .camera_cached = 0};
  BkEndingControlState c = {0};
  BkEndingAuxiliaryState a = {0};
  int32_t gate = 1, clip = 1, open = 0, unavailable[2] = {0},
          targets[39][2] = {{100, 200}};
  uint8_t item = 1;
  float matrix[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}, gauge;
  BkEndingNormalConfig config;
  BkEndingUiSelectBindings b = {
      &f,      &c, &a,     &gate, &clip,       &open, config.actions,
      targets, 39, matrix, NULL,  unavailable, &item};
  float pointer[2] = {20, 30};
  char e[256];
  int32_t selected = -77;
  Context context = {.aux = &a};
  BkEndingUiSelectOps ops = {&context, key, voice};
  assert(bk_ending_ui_initialize(&ui, 1280, c.pause_flags, &gauge, e));
  assert(bk_ending_ui_select(&ui, &stage, &b, pointer, .1f, 0, &ops, &selected,
                             &out, e));
  assert(selected == 2 && context.keys == 2 && context.voices == 1 &&
         f.camera_request == 0 && out.count == 1);
  context.playing = 1;
  out.count = 0;
  assert(bk_ending_ui_select(&ui, &stage, &b, pointer, .1f, 0, &ops, &selected,
                             &out, e));
  assert(selected == 2 && f.camera_request == 1 && !out.count);
  /* Read only the services allowed by the native gate. */
  ops.voice_playing = NULL;
  f.state_721ee4 = 3;
  assert(bk_ending_ui_select(&ui, &stage, &b, pointer, 0, 0, &ops, &selected,
                             &out, e));
  assert(selected == 0);
  f.state_721ee4 = 2;
  selected = 77;
  assert(!bk_ending_ui_select(&ui, &stage, &b, pointer, 0, 1, &ops, &selected,
                              &out, e));
  assert(selected ==
         77); /* final visible override does not bypass earlier work */
  ops.voice_playing = voice;
  context.playing = 0;
  context.fail_key = 1;
  out.count = 0;
  assert(!bk_ending_ui_select(&ui, &stage, &b, pointer, .1f, 0, &ops, &selected,
                              &out, e));
  assert(out.count == 1 && f.camera_request == 0 && selected == 77);
  context.fail_key = 0;
  out.count = 0;
  f.phase = 7;
  assert(!bk_ending_ui_select(&ui, &stage, &b, pointer, 0, 0, &ops, &selected,
                              &out, e));
  assert(bk_ending_ui_select(&ui, &stage, &b, pointer, 0, 1, NULL, &selected,
                             &out, e) &&
         selected == 0);
  f.phase = 9;
  assert(bk_ending_ui_select(&ui, &stage, &b, pointer, 0, 0, NULL, &selected,
                             &out, e) &&
         selected == 0);
  /* All actual tables make row14 unreachable before camera queries. */
  for (unsigned group = 0; group < 5; group++)
    for (unsigned variant = 0; variant < 2; variant++) {
      assert(bk_ending_normal_config(&config, group, variant));
      assert(config.actions[70] == -1);
    }
  assert(bk_ending_normal_config(&config, 0, 0));
  f.phase = 1;
  f.group = 0;
  f.state_721ee0 = 1;
  f.camera_cached = 12;
  out.count = 0;
  selected = 77;
  assert(bk_ending_ui_select_normal(&ui, &stage, &b, pointer, .1f, &selected,
                                    &out, e));
  assert(selected == 4 && out.count == 1 && f.camera_request == 0);
  f.camera_cached = 38;
  config.actions[70] = 38;
  out.count = 0;
  selected = 77;
  assert(!bk_ending_ui_select_normal(&ui, &stage, &b, pointer, .1f, &selected,
                                     &out, e));
  assert(selected == 77 && !out.count);
  /* Capacity failure cannot write request0 after a ring that wasn't drawn. */
  assert(bk_ending_normal_config(&config, 0, 1));
  f.phase = 3;
  f.camera_cached = 17;
  f.camera_request = 9;
  out.count = BK_ENDING_UI_DRAWS;
  assert(!bk_ending_ui_select_third(&ui, &stage, &b, pointer, .1f, &selected,
                                    &out, e));
  assert(f.camera_request == 9 && selected == 77 &&
         out.count == BK_ENDING_UI_DRAWS);
  out.count = 0;
  clip = 6;
  gate = 1;
  b.pick = NULL;
  assert(!bk_ending_ui_select_third(&ui, &stage, &b, pointer, 0, &selected,
                                    &out, e));
  assert(f.camera_request == 9);
  gate = 3;
  assert(bk_ending_ui_select_third(&ui, &stage, &b, pointer, 0, &selected, &out,
                                   e));
  assert(selected == 0 && f.camera_request == 1);
  puts("ending UI select PASS");
  return 0;
}
