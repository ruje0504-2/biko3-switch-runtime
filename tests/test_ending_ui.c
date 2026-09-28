#include "scene/ending_ui.h"
#include "scene/ending_ui_toolbar.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
typedef struct {
  unsigned count, fail_at;
  uint32_t values[2];
} Keys;
static int key(void *ctx, unsigned code, unsigned mode, uint32_t *out,
               char e[256]) {
  (void)e;
  Keys *k = ctx;
  assert(mode == 2 && code < 2);
  ++k->count;
  if (k->count == k->fail_at)
    return 0;
  *out = k->values[code];
  return 1;
}
static void toolbar(void) {
  BkEndingUi ui = {0};
  BkEndingStageUi stage = {0};
  BkEndingFrameState frame = {.phase = 9};
  BkEndingControlState control = {.hover = 17};
  BkEndingAuxiliaryState auxiliary = {0};
  int32_t open = 0;
  BkEndingUiToolbarBindings b = {&frame, &control, &auxiliary, &open};
  BkEndingUiFrame out = {0};
  uint8_t visible = 255;
  float gauge = 0, point[] = {1200, 20};
  char e[256];
  assert(bk_ending_ui_initialize(&ui, 1280, control.pause_flags, &gauge, e));
  ui.sprites[17].transform.fade =
      (BkFadeSprite){.alpha = 0, .speed = 1, .stage = 1};
  assert(bk_ending_ui_toolbar(&ui, &stage, &b, point, 1, .1f, NULL, &visible,
                              &out, e));
  assert(!visible && !open && out.count == 27);
  assert(out.draws[4].slot == 17 && out.draws[9].slot == 17 &&
         out.draws[4].alpha == .05f && out.draws[9].alpha == .1f);
  assert(ui.sprites[60].transform.fade.stage == 1 &&
         ui.sprites[62].transform.fade.stage == 1); /* request, not drawn */
  /* Phase8 must bypass transition gates; phase9 only does so for manual. */
  frame.phase = 8;
  frame.curtain_wanted = 1;
  frame.state_721ee0 = auxiliary.gate = 3;
  BkEndingUi before = ui;
  out.count = 0;
  Keys k = {.fail_at = 1};
  BkEndingUiHoverOps ops = {&k, key};
  assert(!bk_ending_ui_toolbar(&ui, &stage, &b, point, 1, .1f, &ops, &visible,
                               &out, e));
  assert(k.count == 1 && !out.count && !memcmp(&before, &ui, sizeof(ui)));
  frame.phase = 9;
  frame.camera_manual = 1;
  k.count = 0;
  assert(!bk_ending_ui_toolbar(&ui, &stage, &b, point, 1, .1f, &ops, &visible,
                               &out, e));
  assert(k.count == 1 && !out.count && !memcmp(&before, &ui, sizeof(ui)));
  frame.camera_manual = 0;
  out.count = BK_ENDING_UI_DRAWS - 1;
  BkEndingUiSprite next = ui.sprites[14];
  assert(!bk_ending_ui_toolbar(&ui, &stage, &b, point, 1, .1f, NULL, &visible,
                               &out, e));
  assert(out.count == BK_ENDING_UI_DRAWS &&
         out.draws[out.count - 1].slot == 49 &&
         !memcmp(&next, &ui.sprites[14], sizeof(next)));
  before = ui;
  assert(!bk_ending_ui_toolbar(&ui, &stage, &b, point, NAN, .1f, NULL, &visible,
                               &out, e));
  assert(!memcmp(&before, &ui, sizeof(ui)));
}
int main(void) {
  toolbar();
  char e[256];
  BkEndingUi s = {0};
  memset(&s.sprites[8], 0x95, sizeof(s.sprites[8]));
  BkEndingUiSprite skipped = s.sprites[8];
  for (unsigned i = 0; i < 63; ++i) {
    if (i == 8)
      continue;
    s.sprites[i].timer = (BkTimer){125, 0xfffffff0, 255};
    s.sprites[i].transform.direction = 255;
    s.sprites[i].transform.motion[0] = .125f;
  }
  uint8_t flags[6] = {1, 2, 3, 4, 5, 6};
  float gauge = -99;
  assert(bk_ending_ui_initialize(&s, 1001, flags, &gauge, e));
  assert(!memcmp(&skipped, &s.sprites[8], sizeof(skipped)));
  assert(s.loaded == (((UINT64_C(1) << 63) - 1) & ~(UINT64_C(1) << 8)));
  assert(!memcmp(flags, (uint8_t[6]){0}, 6));
  assert(s.sprites[55].transform.scale[0] == 5);
  BkEndingUi old = s;
  assert(!bk_ending_ui_initialize(&s, 0, flags, &gauge, e));
  assert(!memcmp(&s, &old, sizeof(s)));
  BkEndingUiDraw draw = {.slot = 99}, before = draw;
  assert(!bk_ending_ui_sprite_step(&s, 8, .1f, &draw, e));
  assert(!bk_ending_ui_sprite_step(&s, 63, .1f, &draw, e));
  assert(!bk_ending_ui_sprite_step(&s, 55, NAN, &draw, e));
  assert(!memcmp(&s, &old, sizeof(s)) && !memcmp(&draw, &before, sizeof(draw)));
  s.sprites[55].transform.idle = 4;
  old = s;
  assert(!bk_ending_ui_sprite_step(&s, 55, .1f, &draw, e));
  assert(!memcmp(&s, &old, sizeof(s)));
  assert(bk_ending_ui_initialize(&s, 1280, flags, &gauge, e));
  int32_t open = 0, request = 1;
  uint8_t visible = 99;
  float point[2] = {1200, 20};
  Keys k = {0};
  BkEndingUiHoverOps ops = {&k, key};
  for (unsigned failure = 1; failure <= 2; ++failure) {
    k = (Keys){.fail_at = failure};
    old = s;
    assert(!bk_ending_ui_hover(&s, &open, &request, point, 1, .1f, &ops,
                               &visible, e));
    assert(k.count == failure && !memcmp(&s, &old, sizeof(s)) && !open &&
           visible == 99);
  }
  assert(!bk_ending_ui_hover(&s, &open, &request, point, 1, .1f, NULL, &visible,
                             e));
  k = (Keys){.values = {257, 0}};
  assert(bk_ending_ui_hover(&s, &open, &request, point, 1, .1f, &ops, &visible,
                            e));
  assert(k.count == 1 && !visible && !open);
  k = (Keys){.values = {256, 256}};
  assert(bk_ending_ui_hover(&s, &open, &request, point, 1, .1f, &ops, &visible,
                            e));
  assert(k.count == 2 && visible && open);
  old = s;
  assert(!bk_ending_ui_hover(&s, &open, &request, point, 1, FLT_MAX, &ops,
                             &visible, e));
  assert(!memcmp(&s, &old, sizeof(s)));
  point[0] = -1;
  assert(bk_ending_ui_hover(&s, &open, &request, point, 1, .1f, NULL, &visible,
                            e));
  assert(!visible);
  unsigned steps = 0;
  for (unsigned width = 640; width <= 1920; width += 640) {
    assert(bk_ending_ui_initialize(&s, width, flags, &gauge, e));
    for (unsigned tick = 0; tick < 240; ++tick)
      for (unsigned i = 0; i < 63; ++i) {
        if (i == 8)
          continue;
        BkEndingUiSprite *p = &s.sprites[i];
        assert(bk_fade_sprite_request(&p->transform.fade, (tick / 60) & 1));
        assert(bk_ending_ui_sprite_step(&s, i, 1.f / 60, &draw, e));
        assert(p->timer.armed == 255 && p->timer.duration == 125 &&
               p->timer.deadline == 0xfffffff0);
        for (unsigned j = 0; j < 8; ++j)
          assert(isfinite(draw.xy[j]));
        ++steps;
      }
  }
  /* Axis-asymmetric exit2 must retain a negative X while Y remains positive. */
  BkEffectSprite p = {0};
  float rect[4] = {0, 0, 32, 16};
  assert(bk_effect_sprite_initialize(&p, rect, 1, 2, 0));
  p.fade.stage = 5;
  p.fade.speed = 0;
  p.scale[0] = 0;
  p.scale[1] = 2;
  assert(bk_effect_sprite_advance(&p, rect, 1));
  assert(p.scale[0] == -.5f && p.scale[1] == 1.5f && p.fade.stage == 5);
  printf("ending UI retention, failure atomicity, key order and %u sprite "
         "steps passed\n",
         steps);
  return 0;
}
