#include "scene/save_menu_control.h"
#include <math.h>
#include <stdio.h>
static int fail(char *e, const char *s) {
  snprintf(e, 256, "save menu: %s", s);
  return 0;
}
static int inside(const float r[4], const float p[2]) {
  return r[0] <= p[0] && (double)r[0] + r[2] >= p[0] && r[1] <= p[1] &&
         (double)r[1] + r[3] >= p[1];
}
/* These literal hit boxes use x87 products before comparison, with no float
 * store of the scaled endpoints. They are distinct from sprite rectangles. */
static int box(float scale, const float p[2], int x, int y, int w, int h) {
  return (double)x * scale <= p[0] && (double)(x + w) * scale >= p[0] &&
         (double)y * scale <= p[1] && (double)(y + h) * scale >= p[1];
}
static int warp(const BkSaveMenuOps *o, float scale, float x, float y,
                char *e) {
  return o->menu.warp(o->menu.context, (float)((double)x * scale),
                      (float)((double)y * scale), e);
}
static int sound(const BkSaveMenuOps *o, unsigned slot, char *e) {
  return o->menu.sound(o->menu.context, slot, e);
}
static int schedule(const BkSaveMenuOps *o, BkFlowTransition *f, uint8_t target,
                    uint8_t mode, char *e) {
  BkFlowTransitionOps ops = {o->menu.context, o->menu.release};
  return bk_flow_transition_schedule(f, &ops, target, mode, e);
}
static int row_warp(const BkSaveMenuOps *o, const BkSaveMenuControl *s,
                    float scale, char *e) {
  if (!s->row)
    return warp(o, scale, 514 + 168 * s->column, 170, e);
  return warp(o, scale, s->row == 11 ? 1070 : 750,
              s->row == 11 ? 912 : 210 + 52 * s->row, e);
}
static int slot_warp(const BkSaveMenuOps *o, int32_t hover, float scale,
                     char *e) {
  return warp(o, scale, 750, 262 + 52 * (hover - 21), e);
}
static int list(BkSaveMenuControl *s, const BkPauseBindings *b,
                const BkSaveMenuInput *in, const BkSaveMenuOps *o, int mode,
                char *e) {
  s->hover = 99;
  float motion[2];
  if (!o->menu.pointer(o->menu.context, s->cursor, motion, e))
    return 0;
  if (!isfinite(s->cursor[0]) || !isfinite(s->cursor[1]))
    return fail(e, "nonfinite pointer");
  if (mode)
    s->tab = (int32_t)in->current_group + 3;
  float scale = in->ui.scale;
  int confirm = (in->ui.buttons & BK_PAUSE_CONFIRM) != 0;
  for (unsigned i = 0; i < 5; ++i) {
    if (!box(scale, s->cursor, 430 + 168 * (int)i, 142, 168, 56) || !confirm ||
        mode)
      continue;
    if (s->tab != (int32_t)i + 3) {
      if (!o->clear_details(o->menu.context, e) ||
          !o->load_details(o->menu.context, i, e))
        return 0;
    }
    if (!sound(o, 0, e))
      return 0;
    s->tab = (int32_t)i + 3;
  }
  if (inside(s->back, s->cursor)) {
    s->hover = 1;
    if (confirm) {
      if (!sound(o, 2, e))
        return 0;
      b->common->action = 1;
      b->common->blocked = 1;
    }
  }
  unsigned group = (unsigned)(s->tab - 3);
  /*507653 resets its local previous-group to0 on EVERY invocation. */
  if (group && !o->reset_text(o->menu.context, e))
    return 0;
  for (unsigned slot = 0; slot < 10; ++slot) {
    if (!box(scale, s->cursor, 438, 228 + 54 * (int)slot, 624, 56))
      continue;
    s->hover = (int32_t)slot + 21;
    if (confirm) {
      if (mode || in->occupied[group][slot]) {
        if (!warp(o, scale, 788, 548, e))
          return 0;
        s->page = 1;
      } else if (!sound(o, 5, e))
        return 0;
    }
  }
  if (s->hover != s->last_hover) {
    if (!*b->hover_latched) {
      if (!sound(o, 3, e))
        return 0;
      *b->hover_latched = 1;
    }
  } else
    *b->hover_latched = 0;
  s->last_hover = s->hover;
  if (in->ui.buttons & (BK_PAUSE_UP | BK_PAUSE_DOWN)) {
    s->row += (in->ui.buttons & BK_PAUSE_UP) ? -1 : 1;
    if (s->row < 0)
      s->row = 11;
    else if (s->row > 11)
      s->row = 0;
    if (!row_warp(o, s, scale, e))
      return 0;
  }
  if (!mode && (in->ui.buttons & (BK_PAUSE_LEFT | BK_PAUSE_RIGHT))) {
    s->column += (in->ui.buttons & BK_PAUSE_LEFT) ? -1 : 1;
    if (s->column < 0)
      s->column = 4;
    else if (s->column > 4)
      s->column = 0;
    if (!warp(o, scale, 514 + 168 * s->column, 170, e))
      return 0;
  }
  if (b->common->blocked == 1 && b->common->curtain.stage == 3) {
    b->common->blocked = 0;
    if (b->common->action == 1) {
      uint8_t previous = b->flow->previous;
      if (!mode && (previous == 1 || previous == 4)) {
        if (!schedule(o, b->flow, previous, 0, e))
          return 0;
        s->skip_draw = 1;
        if (previous == 4 && !o->resume_pause(o->menu.context, e))
          return 0;
      } else if (mode) {
        if (!schedule(o, b->flow, 2, 3, e))
          return 0;
        s->skip_draw = 1;
      }
      s->row = s->column = 0;
    }
    b->common->action = 0;
  }
  return 1;
}
static int confirmation(BkSaveMenuControl *s, const BkPauseBindings *b,
                        const BkSaveMenuInput *in, const BkSaveMenuOps *o,
                        int mode, char *e) {
  float motion[2];
  if (!o->menu.pointer(o->menu.context, s->cursor, motion, e))
    return 0;
  if (!isfinite(s->cursor[0]) || !isfinite(s->cursor[1]))
    return fail(e, "nonfinite pointer");
  s->confirm_hover = 99;
  unsigned group = (unsigned)(s->tab - 3), slot = (unsigned)(s->hover - 21);
  float scale = in->ui.scale;
  int confirm = (in->ui.buttons & BK_PAUSE_CONFIRM) != 0;
  if (inside(s->yes, s->cursor)) {
    s->confirm_hover = 10;
    if (confirm) {
      if (!sound(o, 0, e))
        return 0;
      if (!mode) {
        if (!o->load(o->menu.context, group, slot, e))
          return 0;
        b->common->action = 1;
        b->common->blocked = 1;
      } else {
        if (!slot_warp(o, s->hover, scale, e) ||
            !o->store(o->menu.context, group, slot, e))
          return 0;
        for (unsigned g = 0; g < 5; ++g)
          if (!o->refresh_bank(o->menu.context, g, e))
            return 0;
        s->page = 0;
      }
    }
  }
  if (inside(s->no, s->cursor)) {
    s->confirm_hover = 12;
    if (confirm) {
      if (!sound(o, 2, e))
        return 0;
      s->page = 0;
      if (!slot_warp(o, s->hover, scale, e))
        return 0;
    }
  }
  if (s->confirm_hover != 99) {
    if (!*b->hover_latched) {
      if (!sound(o, 3, e))
        return 0;
      *b->hover_latched = 1;
    }
  } else
    *b->hover_latched = 0;
  if (in->ui.buttons & (BK_PAUSE_LEFT | BK_PAUSE_RIGHT)) {
    if (in->ui.buttons & BK_PAUSE_LEFT) {
      if (--s->confirm_column < 0)
        s->confirm_column = 1;
    } else if (++s->confirm_column > 1)
      s->confirm_column = 0;
    if (!warp(o, scale, s->confirm_column ? 496 : 788, 548, e))
      return 0;
  }
  if (b->common->blocked == 1 && b->common->curtain.stage == 3) {
    b->common->blocked = 0;
    if (b->common->action == 1) {
      uint8_t previous = b->flow->previous;
      if (!mode && (previous == 1 || previous == 4)) {
        if (previous == 4 && !o->menu.release(o->menu.context, 2, e))
          return 0;
        if (!schedule(o, b->flow, 2, 1, e))
          return 0;
        s->skip_draw = 1;
      }
      s->row = s->column = s->page = 0;
    }
    b->common->action = 0;
  }
  return 1;
}
int bk_save_menu_control(BkSaveMenuControl *s, const BkPauseBindings *b,
                         const BkSaveMenuInput *in, const BkSaveMenuOps *o,
                         char e[256]) {
  if (!s || !b || !b->common || !b->flow || !b->hover_latched || !in || !o ||
      !o->menu.pointer || !o->menu.sound || !o->menu.warp || !o->menu.release ||
      !o->load || !o->store || !o->refresh_bank || !o->clear_details ||
      !o->load_details || !o->reset_text || !o->resume_pause ||
      b->flow->current != 0x28 || !isfinite(in->ui.scale) ||
      in->ui.scale <= 0 || in->ui.scale > 16 || in->current_group >= 5)
    return fail(e, "invalid services/input");
  if (s->tab < 3 || s->tab > 7 || s->row < 0 || s->row > 11 || s->column < 0 ||
      s->column > 4 || s->confirm_column < 0 || s->confirm_column > 1 ||
      (s->page == 1 && (s->hover < 21 || s->hover > 30)))
    return fail(e, "invalid selection index");
  const float *rects[] = {s->back, s->yes, s->no};
  for (unsigned r = 0; r < 3; ++r)
    for (unsigned i = 0; i < 4; ++i)
      if (!isfinite(rects[r][i]) || (i >= 2 && rects[r][i] < 0))
        return fail(e, "invalid loaded rectangle");
  int mode = b->flow->previous == 0x20;
  if (!s->page)
    return list(s, b, in, o, mode, e);
  if (s->page == 1)
    return confirmation(s, b, in, o, mode, e);
  return 1; /*Native507540 default dispatch performs no work.*/
}
