#include "scene/ending_stage_ui.h"
#include "scene/ending_ui_cursor.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static void cursor(void) {
  BkEndingUi ui = {0};
  BkEndingStageUi stage = {0};
  BkEndingFrameState frame = {.phase = 2};
  BkEndingAuxiliaryState auxiliary = {0};
  int32_t clip = 0, ready = 0;
  BkEndingUiNoticeState notices = {0};
  BkEndingUiCursorBindings b = {&frame, &auxiliary, &clip, &ready, &notices};
  BkEndingUiFrame out = {0};
  uint8_t flags[6];
  float gauge;
  char e[256];
  assert(bk_ending_ui_initialize(&ui, 640, flags, &gauge, e));
  for (unsigned i = 0; i < 8; i++)
    ui.sprites[i].transform.fade = (BkFadeSprite){1, 2, 3};
  ui.sprites[8].transform.fade = (BkFadeSprite){.3f, 2, 1};
  assert(bk_ending_ui_cursor(&ui, &stage, &b, 1, 0, (float[2]){120, 80}, 1,
                             &out, e));
  assert(out.count == 12 && out.draws[0].alpha == 1 &&
         ui.sprites[0].transform.fade.stage == 4);
  assert(ui.sprites[1].transform.fade.stage == 3 &&
         ui.sprites[8].transform.fade.stage == 1);
  assert(ui.sprites[8].rect[0] == 120 && ui.sprites[8].rect[1] == 80);
  unsigned order[] = {54, 53, 56, 55};
  for (unsigned i = 0; i < 4; i++)
    assert(out.draws[8 + i].slot == order[i]);
  frame.phase = 9;
  out.count = 0;
  assert(bk_ending_ui_cursor(&ui, &stage, &b, 8, 0, (float[2]){130, 90}, 1,
                             &out, e));
  /* Override is cursor0, and requests happen after the completed fade step. */
  assert(ui.sprites[0].transform.fade.stage == 1);
  frame.phase = 1;
  frame.state_721ee0 = 3;
  b.normal_ready = NULL;
  out.count = 0;
  assert(!bk_ending_ui_cursor(&ui, &stage, &b, 0, 0, (float[2]){140, 100}, .1f,
                              &out, e));
  assert(out.count == 1 && out.draws[0].slot == 0);
  BkEndingUi before = ui;
  out.count = 0;
  assert(!bk_ending_ui_cursor(&ui, &stage, &b, 0, 0, (float[2]){NAN, 0}, .1f,
                              &out, e));
  assert(!out.count && !memcmp(&ui, &before, sizeof(ui)));
}
static void dispatch(void) {
  BkEndingUi base = {0};
  BkEndingStageUi stage = {0};
  BkEndingUiFrame frame = {0};
  char e[256];
  /* Cold globals and previously released globals are both real call sites. */
  for (unsigned slot = 0; slot < BK_ENDING_UI_TOTAL_SPRITES; ++slot) {
    BkEndingUiSprite *p =
        slot < 63 ? &base.sprites[slot] : &stage.sprites[slot - 63];
    assert(bk_fade_sprite_request(&p->transform.fade, 1));
    assert(bk_ending_ui_dispatch_sprite(&base, &stage, slot, 0, &frame, e));
    assert(p->transform.fade.stage == 2 && p->transform.fade.alpha == 1);
    assert(!frame.count);
  }
  assert(bk_ending_stage_ui_initialize(&base, &stage, BK_ENDING_UI_AUXILIARY, 0,
                                       0, 1280, e));
  BkEndingUiSprite *p = &stage.sprites[9]; /* popup72 */
  assert(bk_fade_sprite_request(&p->transform.fade, 1));
  for (unsigned i = 0; i < 90; ++i) {
    frame.count = 0;
    assert(
        bk_ending_ui_dispatch_sprite(&base, &stage, 72, 1.f / 60, &frame, e));
    assert(frame.count == 1 && frame.draws[0].slot == 72);
  }
  float last_angle = p->transform.radians, last_degrees = p->transform.degrees;
  BkEndingUiDraw snapshot = frame.draws[0];
  assert(bk_ending_stage_ui_release(&base, &stage, BK_ENDING_UI_AUXILIARY, e));
  frame.count = BK_ENDING_UI_DRAWS;
  assert(bk_ending_ui_dispatch_sprite(&base, &stage, 72, .1f, &frame, e));
  assert(p->transform.degrees != last_degrees &&
         p->transform.radians == last_angle);
  assert(frame.count == BK_ENDING_UI_DRAWS &&
         !memcmp(&snapshot, &frame.draws[0], sizeof(snapshot)));
  assert(bk_ending_stage_ui_initialize(&base, &stage, BK_ENDING_UI_THIRD, 3, 0,
                                       1280, e));
  BkEndingUi before = base;
  BkEndingStageUi before_stage = stage;
  BkEndingUiFrame before_frame = frame;
  assert(!bk_ending_ui_dispatch_sprite(&base, &stage, 8, .1f, &frame, e));
  assert(!memcmp(&base, &before, sizeof(base)) &&
         !memcmp(&stage, &before_stage, sizeof(stage)) &&
         !memcmp(&frame, &before_frame, sizeof(frame)));
  frame.count = 0;
  assert(bk_ending_ui_dispatch_sprite(&base, &stage, 8, .1f, &frame, e));
  assert(frame.count == 1 && frame.draws[0].slot == 8);
  for (unsigned bad = 0; bad < 7; ++bad) {
    BkEndingUi b = base;
    BkEndingStageUi s = stage;
    BkEndingUiFrame f = frame;
    unsigned slot = 8;
    float dt = .1f;
    if (bad == 0)
      b.loaded &= ~(UINT64_C(1) << 8);
    if (bad == 1)
      s.loaded &= ~1u;
    if (bad == 2)
      s.images[0] = NULL;
    if (bad == 3)
      dt = NAN;
    if (bad == 4)
      slot = 75;
    if (bad == 5)
      f.count = BK_ENDING_UI_DRAWS + 1;
    if (bad == 6) {
      slot = 72; /* invalid detached update is also atomic */
      dt = -1;
    }
    before = b;
    before_stage = s;
    before_frame = f;
    assert(!bk_ending_ui_dispatch_sprite(&b, &s, slot, dt, &f, e));
    assert(!memcmp(&b, &before, sizeof(b)) &&
           !memcmp(&s, &before_stage, sizeof(s)) &&
           !memcmp(&f, &before_frame, sizeof(f)));
  }
}
int main(void) {
  cursor();
  dispatch();
  BkEndingUi base = {0};
  BkEndingStageUi stage = {0};
  char e[256];
  unsigned steps = 0;
  for (unsigned i = 0; i < 12; i++) {
    stage.sprites[i].timer = (BkTimer){123, 456, 1};
    stage.sprites[i].transform.direction = 17;
    stage.sprites[i].transform.motion[0] = .02f;
  }
  for (unsigned kind = 1; kind <= 5; kind++)
    for (unsigned g = 0; g < 5; g++)
      for (unsigned v = 0; v < (kind == 2 ? 2u : 1u); v++) {
        assert(bk_ending_stage_ui_initialize(
            &base, &stage, (BkEndingUiStageKind)kind, g, v, 961, e));
        for (unsigned i = 0; i < 12; i++)
          assert(stage.sprites[i].timer.duration == 123 &&
                 stage.sprites[i].timer.deadline == 456 &&
                 stage.sprites[i].timer.armed == 1 &&
                 stage.sprites[i].transform.direction == 17);
        for (unsigned t = 0; t < 120; t++)
          for (unsigned slot = 0; slot < 75; slot++) {
            if (!bk_ending_stage_ui_image(&stage, slot))
              continue;
            BkEndingUiSprite *p =
                slot == 8 ? &base.sprites[8] : &stage.sprites[slot - 63];
            assert(bk_fade_sprite_request(&p->transform.fade, t % 40 < 30));
            BkEndingUiDraw d;
            assert(bk_ending_stage_ui_sprite_step(&base, &stage, slot, 1.f / 60,
                                                  &d, e));
            assert(d.slot == slot);
            steps++;
          }
        BkEndingStageUi before = stage;
        BkEndingUiSprite cursor = base.sprites[8];
        assert(bk_ending_stage_ui_release(&base, &stage,
                                          (BkEndingUiStageKind)kind, e));
        assert(!stage.loaded);
        assert(!memcmp(stage.sprites, before.sprites, sizeof(stage.sprites)) &&
               !memcmp(&cursor, &base.sprites[8], sizeof(cursor)));
      }
  assert(bk_ending_stage_ui_initialize(&base, &stage, BK_ENDING_UI_THIRD, 2, 0,
                                       640, e));
  assert(base.loaded & (UINT64_C(1) << 8));
  assert(bk_ending_stage_ui_initialize(&base, &stage, BK_ENDING_UI_AUXILIARY, 1,
                                       1, 640, e));
  assert(bk_ending_stage_ui_release(&base, &stage, BK_ENDING_UI_AUXILIARY, e));
  assert(stage.loaded == 1 && (base.loaded & (UINT64_C(1) << 8)));
  assert(!strcmp(bk_ending_stage_ui_image(&stage, 8), "hi_05.tga"));
  BkEndingStageUi before = stage;
  BkEndingUi base_before = base;
  assert(!bk_ending_stage_ui_initialize(&base, &stage, BK_ENDING_UI_AUXILIARY,
                                        0, 2, 640, e));
  assert(!bk_ending_stage_ui_initialize(&base, &stage, BK_ENDING_UI_FINAL, 5, 0,
                                        640, e));
  assert(!bk_ending_stage_ui_initialize(&base, &stage, (BkEndingUiStageKind)-1,
                                        0, 0, 640, e));
  assert(!bk_ending_stage_ui_initialize(&base, &stage, BK_ENDING_UI_THIRD, 0, 0,
                                        0, e));
  assert(!bk_ending_stage_ui_release(&base, &stage, (BkEndingUiStageKind)6, e));
  assert(!memcmp(&before, &stage, sizeof(stage)) &&
         !memcmp(&base_before, &base, sizeof(base)));
  BkEndingUiDraw draw = {.slot = 99};
  assert(!bk_ending_stage_ui_sprite_step(&base, &stage, 63, .1f, &draw, e));
  assert(!bk_ending_stage_ui_sprite_step(&base, &stage, 8, NAN, &draw, e));
  assert(draw.slot == 99 && !memcmp(&base_before, &base, sizeof(base)));
  assert(!bk_ending_stage_ui_image(&stage, 75) &&
         !bk_ending_stage_ui_image(&stage, UINT32_MAX));
  printf("PASS ending stage UI: 30 profiles/%u steps, retained scalar "
         "caches/shared cursor, selective release, detached dispatch and "
         "atomic invalid inputs\n",
         steps);
  return 0;
}
