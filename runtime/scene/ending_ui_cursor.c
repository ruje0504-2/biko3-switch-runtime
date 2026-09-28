#include "scene/ending_ui_cursor.h"
#include <math.h>
#include <stdio.h>
static int fail(char e[256], const char *why) {
  if (e)
    snprintf(e, 256, "ending UI cursor: %s", why);
  return 0;
}
static int request(BkEndingUiSprite *p, uint8_t wanted, char e[256]) {
  return bk_fade_sprite_request(&p->transform.fade, wanted) ||
         fail(e, "invalid visibility state");
}
int bk_ending_ui_cursor(BkEndingUi *ui, BkEndingStageUi *stage,
                        const BkEndingUiCursorBindings *b, int32_t selected,
                        uint8_t visible, const float pointer[2], float dt,
                        BkEndingUiFrame *out, char e[256]) {
  if (!ui || !stage || !b || !b->frame || !b->auxiliary || !b->notices ||
      !pointer || !out || out->count > BK_ENDING_UI_DRAWS ||
      !isfinite(pointer[0]) || !isfinite(pointer[1]) || !isfinite(dt) || dt < 0)
    return fail(e, "invalid input/bindings");
  if (((ui->loaded >> 8) & 1) != (stage->loaded & 1))
    return fail(e, "inconsistent cursor8 owner");
  const BkEndingFrameState *f = b->frame;
  if (visible == 1 || f->phase == 9)
    selected = 0;
  for (unsigned i = 0; i < 9; i++) {
    BkEndingUiSprite *p = &ui->sprites[i];
    p->rect[0] = pointer[0];
    p->rect[1] = pointer[1];
    if ((ui->loaded >> i) & 1)
      if (!bk_ending_ui_dispatch_sprite(ui, stage, i, dt, out, e))
        return 0;
    if (selected != (int32_t)i) {
      if (!request(p, 0, e))
        return 0;
      continue;
    }
    if (f->state_721ee0 != 3 && f->phase == 1) {
      if (!request(p, 1, e))
        return 0;
    } else if (f->state_721ee0 == 3) {
      if (f->camera_cached == 11 || f->camera_cached == 12) {
        if (!request(p, 0, e))
          return 0;
      } else {
        if (f->phase == 1 && !b->normal_ready)
          return fail(e, "missing normal readiness");
        if (!request(p, f->phase == 1 && !*b->normal_ready ? 0 : 1, e))
          return 0;
      }
    }
    if (f->phase == 2 && !request(p, 1, e))
      return 0;
    if (f->phase == 5 || f->phase == 6) {
      if (b->auxiliary->gate == 3) {
        if (!b->active_clip)
          return fail(e, "missing primary clip");
        if (!request(p,
                     *b->active_clip == 1 || *b->active_clip == 2 ||
                         f->camera_event == 1,
                     e))
          return 0;
      } else if (b->auxiliary->gate == 1 || b->auxiliary->gate == 2) {
        if (!request(p, 1, e))
          return 0;
      }
    }
    if (f->phase == 3 || f->phase == 4 || f->phase == 8 || f->phase == 9)
      if (!request(p, 1, e))
        return 0;
  }
  for (unsigned i = 0; i < 2; i++) {
    if (!request(&stage->sprites[9 + i], b->notices->popups[i], e) ||
        !bk_ending_ui_dispatch_sprite(ui, stage, 72 + i, dt, out, e))
      return 0;
  }
  static const unsigned notice_order[] = {1, 0, 3, 2};
  for (unsigned i = 0; i < 4; i++) {
    unsigned n = notice_order[i], slot = 53 + n;
    if (!request(&ui->sprites[slot], b->notices->notices[n], e) ||
        !bk_ending_ui_dispatch_sprite(ui, stage, slot, dt, out, e))
      return 0;
  }
  return 1;
}
