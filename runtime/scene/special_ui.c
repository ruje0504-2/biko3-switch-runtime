#include "scene/special_ui.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
static int fail(char e[256], const char *why) {
  if (e) snprintf(e, 256, "special UI: %s", why);
  return 0;
}
typedef struct { const char *name; float rect[4]; } Layout;
static const Layout normal[] = {
  {"ma_00.tga", {320, 240, 64, 64}},
  {"hs_09.tga", {320, 240, 96, 96}},
  {"hs_08.tga", {320, 240, 96, 96}},
  {"hs_31.tga", {1308, 200, 160, 40}},
  {"hs_30.tga", {1308, 200, 160, 40}},
  {"ih_02.tga", {1308, 372, 160, 40}},
  {"ih_01.tga", {1308, 372, 160, 40}},
  {"hs_13.tga", {1308, 36, 160, 48}},
  {"hs_14.tga", {1308, 36, 160, 48}},
  {"hs_15.tga", {1308, 36, 160, 48}},
  {"hs_33.tga", {1308, 164, 160, 40}},
  {"hs_32.tga", {1308, 164, 160, 40}},
  {"hs_41.tga", {1308, 288, 160, 40}},
  {"hs_40.tga", {1308, 288, 160, 40}},
  {"ih_00.tga", {1300, 0, 184, 432}},
  {"hs_12.tga", {1308, 80, 160, 40}},
  {"hs_11.tga", {1308, 80, 160, 40}},
};
static const Layout special[] = {
  {"ma_00.tga", {320, 240, 64, 64}},
  {"hs_09.tga", {320, 240, 96, 96}},
  {"hs_08.tga", {320, 240, 96, 96}},
  {"hs_15.tga", {1308, 64, 160, 40}},
  {"hs_14.tga", {1308, 64, 160, 40}},
  {"hs_41.tga", {1308, 192, 160, 40}},
  {"hs_40.tga", {1308, 192, 160, 40}},
  {"hs_11.tga", {1308, 22, 160, 40}},
  {"hs_12.tga", {1308, 22, 160, 40}},
  {"hs_13.tga", {1308, 22, 160, 40}},
  {"hs_17.tga", {1308, 108, 160, 40}},
  {"hs_16.tga", {1308, 108, 160, 40}},
  {"hs_33.tga", {1308, 152, 160, 40}},
  {"hs_32.tga", {1308, 152, 160, 40}},
  {"ih_00.tga", {1300, 0, 184, 248}},
};
static const Layout counter[] = {
  {"ge_43.tga", {0, 0, 32, 32}},
  {"ge_44.tga", {0, 0, 32, 32}},
  {"ge_45.tga", {0, 0, 32, 32}},
  {"ge_46.tga", {0, 0, 32, 32}},
  {"ge_47.tga", {0, 0, 32, 32}},
  {"ge_48.tga", {0, 0, 32, 32}},
  {"ge_49.tga", {0, 0, 32, 32}},
  {"ge_50.tga", {0, 0, 32, 32}},
  {"ge_51.tga", {0, 0, 32, 32}},
  {"ge_52.tga", {0, 0, 32, 32}},
  {"ge_53.tga", {0, 0, 32, 32}},
  {"ge_54.tga", {0, 0, 32, 32}},
  {"ge_16.tga", {1170, 12, 96, 64}},
};
const char *bk_special_ui_image(unsigned slot, int8_t special_mode) {
  if (slot >= BK_SPECIAL_UI_SPRITES) return NULL;
  if (slot >= 17) return counter[slot - 17].name;
  return special_mode == 1 ? (slot < 15 ? special[slot].name : NULL) : normal[slot].name;
}
int bk_special_ui_initialize(BkSpecialUi *s, BkSpecialUiControl *control,
    BkSpecialEventState *event, unsigned width, int8_t special_mode,
    const BkSpecialUiOps *ops, char e[256]) {
  if (!s || !control || !event || !ops || !ops->image || !width || width > 16384)
    return fail(e, "invalid initializer");
  float scale = (float)((double)width / 1280);
  for (unsigned n = 0; n < 30; ++n) {
    unsigned i = n < 13 ? n + 17 : n - 13;
    const char *name = bk_special_ui_image(i, special_mode);
    if (!name) continue;
    const Layout *l = i >= 17 ? &counter[i - 17] : special_mode == 1 ? &special[i] : &normal[i];
    if (!ops->image(ops->context, i, name, e)) return 0;
    BkEndingUiSprite *p = &s->sprites[i];
    for (unsigned k = 0; k < 4; ++k) p->rect[k] = (float)((double)l->rect[k] * scale);
    if (!bk_effect_sprite_initialize(&p->transform, p->rect, i < 3, i < 3, 0))
      return fail(e, "invalid sprite constructor");
    memcpy(p->uv, (float[4]){0, 0, 1, 1}, sizeof p->uv); p->rgb = 0xffffff;
    if (!bk_fade_sprite_request(&p->transform.fade, 1)) return fail(e, "invalid initial request");
    s->loaded |= UINT32_C(1) << i;
    if (i == 29) {
      /*50de40 clears/copies its filename at71ad90. Native flow48 fields
       * occupy this prefix, rather than separate retained globals.*/
      event->sequence_timer.duration = UINT32_C(0x315f6567); /*ge_1*/
      event->sequence_timer.deadline = UINT32_C(0x67742e36); /*6.tg*/
      event->sequence_timer.armed = 'a';
      event->face_target = 0; event->face_timer = (BkTimer){0}; event->sequence = 0;
      control->open = 0; control->row = 0;
    }
  }
  return 1;
}
static int draw(BkSpecialUi *s, unsigned i, float dt, BkSpecialUiFrame *out, char e[256]) {
  if (!(s->loaded & (UINT32_C(1) << i)))
    return bk_effect_sprite_advance_detached(&s->sprites[i].transform, s->sprites[i].rect, dt) || fail(e, "invalid detached sprite");
  if (out->sprites.count >= BK_ENDING_UI_DRAWS) return fail(e, "draw capacity exceeded");
  if (!bk_ending_ui_element_step(&s->sprites[i], i, dt, &out->sprites.draws[out->sprites.count], e)) return 0;
  ++out->sprites.count; return 1;
}
static int request(BkSpecialUi *s, unsigned i, uint8_t wanted, char e[256]) {
  return bk_fade_sprite_request(&s->sprites[i].transform.fade, wanted) || fail(e, "invalid visibility request");
}
static int key(const BkSpecialUiOps *ops, uint32_t code, unsigned mode, int *out, char e[256]) {
  uint32_t value;
  if (!ops->key || !ops->key(ops->context, code, mode, &value, e)) return fail(e, "key service failed/missing");
  *out = (uint8_t)value != 0; return 1;
}
static int keys(const BkSpecialUiOps *ops, const uint32_t *codes, unsigned count, unsigned mode, int *out, char e[256]) {
  *out = 0;
  for (unsigned i = 0; i < count; ++i) { if (!key(ops, codes[i], mode, out, e)) return 0; if (*out) break; }
  return 1;
}
static int confirm(const BkSpecialUiOps *ops, unsigned mode, int *out, char e[256]) {
  return keys(ops, (uint32_t[3]){0, 0x5a, 0x33450}, 3, mode, out, e);
}
static int sound(const BkSpecialUiOps *ops, unsigned slot, char e[256]) {
  return (ops->sound && ops->sound(ops->context, slot, e)) || fail(e, "sound service failed/missing");
}
static int capture(const BkSpecialUiOps *ops, BkSpecialCapture op, char e[256]) {
  return (ops->capture && ops->capture(ops->context, op, e)) || fail(e, "capture service failed/missing");
}
static int hit(const float first[4], const float last[4], const float p[2]) {
  return first[0] <= p[0] && (double)last[0] + last[2] >= p[0] &&
         first[1] <= p[1] && (double)last[1] + last[3] >= p[1];
}
static int save_camera(const BkSpecialUiBindings *b, char e[256]) {
  int32_t c = *b->camera_clip;
  if (c < 0 || c >= 3) return fail(e, "camera choice outside three presets");
  b->presets->active[0][c] = b->camera->yaw; b->presets->active[1][c] = b->camera->pitch;
  b->presets->active[2][c] = b->camera->radius; b->presets->active[3][c] = b->camera->height;
  return 1;
}
static int restore_camera(const BkSpecialUiBindings *b, char e[256]) {
  int32_t c = *b->camera_clip;
  if (c < 0 || c >= 3) return fail(e, "camera choice outside three presets");
  b->camera->yaw = b->presets->active[0][c]; b->camera->pitch = b->presets->active[1][c];
  b->camera->radius = b->presets->active[2][c]; b->camera->height = b->presets->active[3][c];
  return 1;
}
static int move(BkEndingUiSprite *p, float target, float scale, float dt, char e[256]) {
  float delta = (float)((double)target * scale - p->rect[0]);
  delta = (float)((double)dt * delta);
  p->rect[0] = (float)((double)p->rect[0] + delta);
  return isfinite(p->rect[0]) || fail(e, "nonfinite sidebar position");
}
static int slide(BkSpecialUi *s, const BkSpecialUiBindings *b, const float pointer[2],
    int forced, float scale, float dt, uint8_t *visible, char e[256]) {
  if (!forced) {
    const float *panel = s->sprites[14].rect;
    *visible = (double)1096 * scale <= pointer[0] && (double)1280 * scale >= pointer[0] &&
      panel[1] <= pointer[1] && (double)panel[1] + panel[3] >= pointer[1];
    b->control->open = (int8_t)*visible;
  }
  int open = !forced && *visible == 1;
  if (!move(&s->sprites[14], open ? 1096 : 1300, scale, dt, e)) return 0;
  for (unsigned i = 3; i <= 13; ++i)
    if (!move(&s->sprites[i], open ? 1104 : 1308, scale, dt, e)) return 0;
  if (*b->special == 0)
    for (unsigned i = 15; i <= 16; ++i)
      if (!move(&s->sprites[i], open ? 1104 : 1308, scale, dt, e)) return 0;
  return 1;
}
static int counter_draw(BkSpecialUi *s, int32_t value, float scale, float dt, BkSpecialUiFrame *out, char e[256]) {
  s->sprites[29].rect[0] = (float)(8.0 * scale); s->sprites[29].rect[1] = (float)(12.0 * scale);
  if (!draw(s, 29, dt, out, e)) return 0;
  int32_t digits[3] = {11,10,10};
  if (value < 100) {
    int32_t h = value / 100, t = value / 10 - h * 10;
    digits[0] = h; digits[1] = t; digits[2] = value - (h * 100 + t * 10);
  }
  for (unsigned n = 0; n < 3; ++n) {
    if (digits[n] < 0 || digits[n] >= 13) return fail(e, "counter index outside resources");
    unsigned slot = 17 + (unsigned)digits[n];
    s->sprites[slot].rect[0] = (float)((10.0 + 24 * n) * scale);
    s->sprites[slot].rect[1] = (float)(76.0 * scale);
    if (!draw(s, slot, dt, out, e)) return 0;
  }
  return 1;
}
static int curtain(BkCommonHudState *c, float dt, BkSpecialUiFrame *out, char e[256]) {
  if (!bk_fade_sprite_advance(&c->curtain, dt)) return fail(e, "invalid curtain");
  out->curtain.curtain_alpha = c->curtain.alpha;
  return bk_fade_sprite_request(&c->curtain, c->blocked) || fail(e, "invalid curtain request");
}
int bk_special_ui_step(BkSpecialUi *s, const BkSpecialUiBindings *b,
    const BkSpecialUiOps *ops, float scale, float dt, BkSpecialUiFrame *out, char e[256]) {
  if (out) memset(out, 0, sizeof *out);
  if (!s || !b || !b->common || !b->phase || !ops || !out || !isfinite(scale) ||
      scale <= 0 || scale > 16 || !isfinite(dt) || dt < 0) return fail(e, "invalid frame/services");
  if (*b->phase == 0 || *b->phase == 1) {
    if (!curtain(b->common, dt, out, e)) return 0;
    out->complete = 1; return 1;
  }
  if (*b->phase != 2) { out->complete = 1; return 1; } /*original dispatch skips*/
  if (!b->control || !b->camera || !b->presets || !b->camera_clip || !b->camera_mode ||
      !b->photo_count || !b->special || !b->previous_flow || !b->visibility || !b->hover_latched)
    return fail(e, "missing shared owners");
  if (*b->special != 1 && !capture(ops, BK_SPECIAL_CAPTURE_FLUSH, e)) return 0;
  if (!counter_draw(s, *b->photo_count, scale, dt, out, e)) return 0;
  float point[2], motion[2]; int pressed, hover = 0, selected = -1; uint8_t visible = 0;
  if (!ops->position || !ops->position(ops->context, point, e) || !isfinite(point[0]) || !isfinite(point[1]))
    return fail(e, "invalid position service");
  if (hit(s->sprites[3].rect, s->sprites[3].rect, point)) {
    if (!confirm(ops, 1, &pressed, e)) return 0;
    if (pressed) {
      if (!sound(ops, 0, e)) return 0;
      if (*b->camera_mode == 0 || *b->camera_mode == 2 || *b->camera_mode == 3) *b->camera_mode = 1;
      else if (*b->camera_mode == 1) *b->camera_mode = 2;
    }
  }
  if (hit(s->sprites[5].rect, s->sprites[5].rect, point)) {
    hover = 5;
    if (!confirm(ops, 1, &pressed, e)) return 0;
    if (pressed) { if (!sound(ops, 2, e)) return 0; b->common->action = 5; b->common->blocked = 1; }
  }
  if (hit(s->sprites[7].rect, s->sprites[*b->special == 1 ? 7 : 15].rect, point)) {
    if (*b->special != 1) hover = 15;
    if (!confirm(ops, 1, &pressed, e)) return 0;
    if (pressed) {
      if (!sound(ops, 0, e)) return 0;
      *b->camera_mode = 2;
      if (!save_camera(b, e)) return 0;
      *b->camera_clip = (*b->camera_clip + 1) % 3;
      if (!restore_camera(b, e)) return 0;
    } else if (!confirm(ops, 2, &pressed, e)) return 0; /*queries still have side effects*/
  }
  if (hit(s->sprites[10].rect, s->sprites[10].rect, point)) {
    hover = 10;
    if (!confirm(ops, 1, &pressed, e)) return 0;
    if (pressed) {
      if (!sound(ops, 0, e)) return 0;
      *b->camera_mode = 2;
      memcpy(b->presets->active, b->presets->authored, sizeof b->presets->active);
      if (!restore_camera(b, e)) return 0;
    }
  }
  if (hit(s->sprites[12].rect, s->sprites[12].rect, point)) {
    hover = 12;
    if (!confirm(ops, 1, &pressed, e)) return 0;
    if (pressed) {
      if (!sound(ops, 0, e)) return 0;
      if (*b->visibility == 0) *b->visibility = 1;
      else if (*b->visibility == 1) *b->visibility = 0;
    }
  }
  if (*b->camera_mode == 0 && !save_camera(b, e)) return 0;
  if (hover) {
    if ((int8_t)*b->hover_latched != hover) {
      if (!sound(ops, 3, e)) return 0;
      *b->hover_latched = (uint8_t)hover;
    }
  } else *b->hover_latched = 0;
  if (*b->special != 1) {
    if (!keys(ops, (uint32_t[2]){0x43, 0x33455}, 2, 1, &pressed, e)) return 0;
    if (pressed) {
      if (*b->photo_count < 100) {
        if (!sound(ops, 7, e) || !capture(ops, BK_SPECIAL_CAPTURE_REQUEST, e) ||
            !capture(ops, BK_SPECIAL_CAPTURE_CONFIGURE, e)) return 0;
      } else if (!sound(ops, 5, e)) return 0;
    }
  }
  if (!key(ops, 0, 2, &pressed, e)) return 0;
  unsigned held = 1;
  if (!pressed) { if (!key(ops, 1, 2, &pressed, e)) return 0; held = 2; }
  if (pressed && b->control->open == 0) {
    selected = (int)held;
    if (!slide(s, b, point, 1, scale, dt, &visible, e)) return 0;
  } else if (!pressed || b->control->open == 1) {
    selected = 0;
    if (!slide(s, b, point, 0, scale, dt, &visible, e)) return 0;
  } else return fail(e, "native cursor selection is uninitialized");
  if (b->control->open == 0 && *b->camera_mode == 3) *b->camera_mode = 0;
  else if (b->control->open == 1 && *b->camera_mode == 0) *b->camera_mode = 3;
  if (!draw(s, 14, dt, out, e)) return 0;
  if (*b->camera_clip >= 0 && *b->camera_clip < 3)
    if (!draw(s, 7 + (unsigned)*b->camera_clip, dt, out, e)) return 0;
  if (!draw(s, *b->camera_mode == 1 ? 4 : 3, dt, out, e) ||
      !draw(s, hover == 15 ? 16 : 15, dt, out, e) ||
      !draw(s, hover == 5 ? 6 : 5, dt, out, e) ||
      !draw(s, hover == 10 ? 11 : 10, dt, out, e) ||
      !draw(s, hover == 12 ? 13 : 12, dt, out, e)) return 0;
  if (visible == 1) selected = 0;
  for (unsigned i = 0; i < 3; ++i) {
    memcpy(s->sprites[i].rect, point, sizeof point);
    if (!draw(s, i, dt, out, e) || !request(s, i, selected == (int)i, e)) return 0;
  }
  if (!ops->motion || !ops->motion(ops->context, motion, e) || !isfinite(motion[0]) || !isfinite(motion[1]))
    return fail(e, "invalid motion service");
  if (motion[0] == 0 && motion[1] == 0) {
    uint32_t now;
    if (!ops->clock || !ops->clock(ops->context, &now, e)) return fail(e, "clock service failed/missing");
    if (bk_timer_poll(&s->sprites[0].timer, now)) s->cursor_wanted = 0;
  } else { s->cursor_wanted = 1; s->sprites[0].timer.armed = 0; }
  if (!curtain(b->common, dt, out, e)) return 0;
  if (!keys(ops, (uint32_t[2]){0x26, 0x30d40}, 2, 1, &pressed, e)) return 0;
  int direction = -1;
  if (!pressed) { if (!keys(ops, (uint32_t[2]){0x28, 0x30d41}, 2, 1, &pressed, e)) return 0; direction = 1; }
  if (pressed) {
    uint32_t bits = (uint32_t)b->control->row + (uint32_t)direction;
    memcpy(&b->control->row, &bits, sizeof bits);
    if (b->control->row < 0) b->control->row = 4;
    else if (b->control->row > 4) b->control->row = 0;
    static const float y[5] = {90, 174, 210, 298, 382};
    if (!ops->warp || !ops->warp(ops->context, (float)(1184.0 * scale),
                                  (float)((double)y[b->control->row] * scale), e))
      return fail(e, "warp service failed/missing");
  }
  if (b->common->blocked == 1 && b->common->curtain.stage == 3) {
    b->common->blocked = 0;
    if (b->common->action == 5) {
      if (*b->previous_flow == 2 || *b->previous_flow == 0x18)
        if (!ops->schedule || !ops->schedule(ops->context, *b->previous_flow, 1, e))
          return fail(e, "schedule service failed/missing");
      *b->phase = 0;
    }
    b->common->action = 0;
  }
  out->complete = 1; return 1;
}
