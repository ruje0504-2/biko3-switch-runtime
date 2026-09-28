#include "scene/ending_ui_toolbar.h"
#include <math.h>
#include <stdio.h>
static int fail(char e[256], const char *why) {
  if (e)
    snprintf(e, 256, "ending UI toolbar: %s", why);
  return 0;
}
int bk_ending_ui_toolbar_shared(BkEndingUi *ui, BkEndingStageUi *stage,
                                const BkEndingUiToolbarBindings *b,
                                const uint8_t *curtain_wanted,
                                const float pointer[2], float scale, float dt,
                                const BkEndingUiHoverOps *ops, uint8_t *visible,
                                BkEndingUiFrame *out, char e[256]) {
  if (!ui || !stage || !b || !b->frame || !b->control || !b->auxiliary ||
      !b->open || !curtain_wanted || !pointer || !visible || !out ||
      out->count > BK_ENDING_UI_DRAWS || !isfinite(scale) || scale <= 0 ||
      scale > 16 || !isfinite(dt) || dt < 0 || !isfinite(pointer[0]) ||
      !isfinite(pointer[1]))
    return fail(e, "invalid state/input");
  const BkEndingFrameState *f = b->frame;
  const BkEndingControlState *c = b->control;
  *visible = 0;
  int hover = f->camera_manual || f->phase == 8 ||
              (f->phase != 9 && f->state_721ee0 != 3 &&
               b->auxiliary->gate != 3 && f->phase != 7 && !*curtain_wanted);
  if (hover && !bk_ending_ui_hover(ui, b->open, &f->camera_request, pointer,
                                   scale, dt, ops, visible, e))
    return 0;
#define DRAW(slot)                                                             \
  do {                                                                         \
    if (!bk_ending_ui_dispatch_sprite(ui, stage, (slot), dt, out, e))          \
      return 0;                                                                \
  } while (0)
#define REQUEST(slot, wanted)                                                  \
  do {                                                                         \
    if (!bk_fade_sprite_request(&ui->sprites[(slot)].transform.fade,           \
                                (wanted)))                                     \
      return fail(e, "invalid visibility state");                              \
  } while (0)
  DRAW(49);
  if ((unsigned)f->camera_clip < 3)
    DRAW(14 + (unsigned)f->camera_clip);
  if ((unsigned)c->mode_721ec4 < 3)
    DRAW(31 + (unsigned)c->mode_721ec4);
  if ((unsigned)f->auxiliary_mode < 4)
    DRAW(41 + (unsigned)f->auxiliary_mode);
  DRAW(f->camera_mode == 1 ? 18 : 17);
  static const unsigned fixed[] = {12, 29, 38, 21, 17, 19, 23,
                                   25, 27, 36, 34, 45, 47};
  for (unsigned i = 0; i < sizeof(fixed) / sizeof(*fixed); ++i)
    DRAW(fixed[i]);
  static const unsigned hovered[][2] = {{12, 13}, {29, 30}, {38, 39}, {21, 22},
                                        {17, 18}, {19, 20}, {45, 46}, {47, 48}};
  for (unsigned i = 0; i < sizeof(hovered) / sizeof(*hovered); ++i)
    if (c->hover == (int32_t)hovered[i][0]) {
      DRAW(hovered[i][1]);
      break;
    }
  static const unsigned toggles[][2] = {{3, 24}, {5, 26}, {1, 28}, {0, 37}};
  for (unsigned i = 0; i < sizeof(toggles) / sizeof(*toggles); ++i)
    if (!c->toggles[toggles[i][0]])
      DRAW(toggles[i][1]);
  if (c->toggles[7])
    DRAW(35);
  for (unsigned i = 0; i < 3; ++i) {
    REQUEST(57 + i, c->pause_flags[i]);
    DRAW(57 + i);
  }
  REQUEST(60, 1);
  REQUEST(61, c->pause_flags[4]);
  DRAW(61);
  REQUEST(62, 1);
  if (c->hover == 59)
    DRAW(60);
  else if (c->hover == 61)
    DRAW(62);
#undef REQUEST
#undef DRAW
  return 1;
}
int bk_ending_ui_toolbar(BkEndingUi *ui, BkEndingStageUi *stage,
                         const BkEndingUiToolbarBindings *b,
                         const float pointer[2], float scale, float dt,
                         const BkEndingUiHoverOps *ops, uint8_t *visible,
                         BkEndingUiFrame *out, char e[256]) {
  return bk_ending_ui_toolbar_shared(
      ui, stage, b, b && b->frame ? &b->frame->curtain_wanted : NULL, pointer,
      scale, dt, ops, visible, out, e);
}
