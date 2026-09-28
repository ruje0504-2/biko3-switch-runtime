#include "scene/pause.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
static const struct {
  const char *name;
  float rect[4];
} layouts[20] = {
    {"ma_12.tga", {0, 0, 1280, 960}},    {"sy_00.tga", {416, 185, 464, 512}},
    {"sy_01.tga", {512, 308, 256, 56}},  {"sy_03.tga", {512, 380, 256, 56}},
    {"sy_05.tga", {512, 452, 256, 56}},  {"sy_07.tga", {512, 524, 256, 56}},
    {"sy_09.tga", {512, 595, 256, 56}},  {"sy_02.tga", {512, 308, 256, 56}},
    {"sy_04.tga", {512, 380, 256, 56}},  {"sy_06.tga", {512, 452, 256, 56}},
    {"sy_08.tga", {512, 524, 256, 56}},  {"sy_10.tga", {512, 595, 256, 56}},
    {"sy_11.tga", {320, 344, 640, 280}}, {"sy_12.tga", {320, 344, 640, 280}},
    {"lo_15.tga", {368, 502, 256, 56}},  {"lo_16.tga", {368, 502, 256, 56}},
    {"lo_17.tga", {656, 502, 256, 56}},  {"lo_18.tga", {656, 502, 256, 56}},
    {"ma_12.tga", {0, 0, 1280, 960}},    {"sy_99.bmp", {0, 0, 1280, 960}}};
static int fail(char e[256], const char *why) {
  snprintf(e, 256, "pause: %s", why);
  return 0;
}
const char *bk_pause_image(unsigned i) {
  return i < 20                  ? layouts[i].name
         : i == BK_PAUSE_CURSOR  ? "ma_00.tga"
         : i == BK_PAUSE_CURTAIN ? "ma_01.tga"
                                 : NULL;
}
int bk_pause_initialize(BkPauseState *s, unsigned width, const BkPauseOps *ops,
                        char e[256]) {
  if (!s || !width || width > 16384 || !ops || !ops->warp)
    return fail(e, "invalid initializer/services");
  float scale = (float)((double)width / 1280);
  for (unsigned i = 0; i < 20; ++i) {
    bk_fade_sprite_initialize(&s->sprites[i].fade);
    for (unsigned j = 0; j < 4; ++j)
      s->sprites[i].rect[j] = (float)((double)layouts[i].rect[j] * scale);
  }
  bk_fade_sprite_request(&s->sprites[19].fade, 1);
  s->background_show = 1;
  s->loaded = 1;
  return ops->warp(ops->context, 320, 240, e);
}
int bk_menu_cursor_initialize(BkMenuCursor *s, unsigned width,
                              unsigned height) {
  if (!s || !width || !height || width > 16384 || height > 16384)
    return 0;
  float x = (float)((double)width / 1024), y = (float)((double)height / 768);
  s->sprite.rect[0] = (float)(320.0 * x);
  s->sprite.rect[1] = (float)(240.0 * y);
  s->sprite.rect[2] = (float)(64.0 * x);
  s->sprite.rect[3] = (float)(64.0 * y);
  bk_fade_sprite_initialize(&s->sprite.fade);
  bk_fade_sprite_request(&s->sprite.fade, 1);
  s->idle.duration = 10000;
  s->wanted = 1;
  return 1;
}
static int append(BkPauseFrame *f, unsigned slot, const BkPauseSprite *p) {
  if (f->count >= BK_PAUSE_DRAWS)
    return 0;
  BkPauseDraw *d = &f->draws[f->count++];
  d->slot = slot;
  d->alpha = p->fade.alpha;
  memcpy(d->rect, p->rect, sizeof(d->rect));
  return 1;
}
int bk_pause_backdrop(BkPauseState *s, BkPauseFrame *f, char e[256]) {
  if (!s || !f || s->sprites[19].fade.stage > 5)
    return fail(e, "invalid backdrop");
  BkFadeSprite *p = &s->sprites[19].fade;
  switch (p->stage) {
  case 0:
    p->alpha = 0;
    break;
  case 1:
    p->alpha = 1;
    p->stage = 2;
    break;
  case 2:
    p->stage = 3;
    p->alpha = 1;
    break;
  case 3:
    p->alpha = 1;
    break;
  case 4:
  case 5:
    p->alpha = 0;
    p->stage = 0;
    break;
  }
  *f = (BkPauseFrame){0};
  return !s->loaded || append(f, 19, &s->sprites[19]);
}
typedef struct {
  BkPauseState *s;
  const BkPauseBindings *b;
  const BkPauseInput *in;
  const BkPauseOps *ops;
} Context;
static int release(void *p, uint8_t flow, char e[256]) {
  Context *c = p;
  if (!c->ops->release(c->ops->context, flow, e))
    return 0;
  if (flow == 4)
    c->s->loaded = 0;
  return 1;
}
static int schedule(Context *c, uint8_t target, char e[256]) {
  BkFlowTransitionOps o = {c, release};
  return bk_flow_transition_schedule(c->b->flow, &o, target, 0, e);
}
static int sound(Context *c, unsigned slot, char e[256]) {
  return c->ops->sound(c->ops->context, slot, e);
}
static int warp(Context *c, float x, float y, char e[256]) {
  return c->ops->warp(c->ops->context, (float)((double)x * c->in->scale),
                      (float)((double)y * c->in->scale), e);
}
static int inside(const BkPauseState *s, unsigned slot) {
  const float *r = s->sprites[slot].rect;
  return r[0] <= s->cursor[0] && (double)r[0] + r[2] >= s->cursor[0] &&
         r[1] <= s->cursor[1] && (double)r[1] + r[3] >= s->cursor[1];
}
static int hover_sound(Context *c, int32_t hover, char e[256]) {
  if (hover == 99)
    *c->b->hover_latched = 0;
  else if (*c->b->hover_latched == 0) {
    if (!sound(c, 3, e))
      return 0;
    *c->b->hover_latched = 1;
  }
  return 1;
}
static int32_t add_wrap(int32_t value, int delta) {
  int64_t n = (int64_t)value + delta;
  if (n > INT32_MAX)
    n -= INT64_C(4294967296);
  if (n < INT32_MIN)
    n += INT64_C(4294967296);
  return (int32_t)n;
}
static int main_page(Context *c, char e[256]) {
  BkPauseState *s = c->s;
  BkCommonHudState *common = c->b->common;
  s->hover = 99;
  s->page = 0;
  for (unsigned i = 2; i <= 6; ++i) {
    if (!inside(s, i))
      continue;
    s->hover = (int32_t)i;
    if (!(c->in->buttons & BK_PAUSE_CONFIRM) || s->action != 0)
      continue;
    if (!sound(c, (i == 2 || i == 3) && c->in->special == 1 ? 5 : 0, e))
      return 0;
    if (i == 2 || i == 3) {
      common->blocked = 1;
      s->action = (int32_t)i;
    } else if (i == 4) {
      s->background_show = 0;
      s->action = 4;
      s->page = 0;
      *c->b->pause_overlay = 1;
    } else {
      s->page = (int32_t)i - 3;
      s->column = 0;
      if (!warp(c, 784, 530, e))
        return 0;
    }
  }
  if (!hover_sound(c, s->hover, e))
    return 0;
  if (c->in->buttons & (BK_PAUSE_UP | BK_PAUSE_DOWN)) {
    if (c->in->buttons & BK_PAUSE_UP) {
      s->row = add_wrap(s->row, -1);
      if (s->row < 0)
        s->row = 4;
    } else {
      s->row = add_wrap(s->row, 1);
      if (s->row >= 5)
        s->row = 0;
    }
    if (s->row < 0 || s->row >= 5)
      return fail(e, "main selection would read native undefined stack data");
    const float rows[] = {336, 408, 480, 552, 623};
    if (!warp(c, 640, rows[s->row], e))
      return 0;
  }
  if (common->blocked == 1 && common->curtain.stage == 3) {
    common->blocked = 0;
    if (s->action == 2 || s->action == 3) {
      if (!schedule(c, s->action == 2 ? 0x28 : 0x30, e))
        return 0;
      s->action = 0;
    }
  }
  if (s->background_show == 0) {
    if (s->sprites[0].fade.stage == 0 && s->action == 4) {
      if (!release(c, 4, e) || !c->ops->remove_capture(c->ops->context, e))
        return 0;
      c->b->flow->current = 2;
      s->action = 0;
    }
  } else if (s->background_show == 1 && s->sprites[0].fade.stage == 3 &&
             s->page == 0)
    s->page = 1;
  return 1;
}
static int confirmation(Context *c, int page, char e[256]) {
  BkPauseState *s = c->s;
  BkCommonHudState *common = c->b->common;
  s->confirm_hover = 99;
  for (unsigned i = 14; i <= 16; i += 2) {
    if (!inside(s, i))
      continue;
    s->confirm_hover = (int32_t)i;
    if (!(c->in->buttons & BK_PAUSE_CONFIRM) || s->action != 0)
      continue;
    if (!sound(c, i == 14 ? 1 : 2, e))
      return 0;
    if (i == 14) {
      s->action = 14;
      common->blocked = 1;
    } else {
      s->action = 0;
      s->page = 1;
      if (!warp(c, 640, page == 2 ? 552 : 623, e))
        return 0;
    }
  }
  if (!hover_sound(c, s->confirm_hover, e))
    return 0;
  if (c->in->buttons & (BK_PAUSE_LEFT | BK_PAUSE_RIGHT)) {
    if (c->in->buttons & BK_PAUSE_LEFT) {
      s->column = add_wrap(s->column, -1);
      if (s->column < 0)
        s->column = 1;
    } else {
      s->column = add_wrap(s->column, 1);
      if (s->column >= 2)
        s->column = 0;
    }
    if (s->column < 0 || s->column >= 2)
      return fail(
          e, "confirmation selection would read native undefined stack data");
    if (!warp(c, s->column ? 496 : 784, 530, e))
      return 0;
  }
  if (common->blocked == 1 && common->curtain.stage == 3 && s->action == 14) {
    if (!release(c, 4, e) || !release(c, 2, e) ||
        !schedule(c, page == 2 ? 1 : 0x58, e) ||
        !c->ops->remove_capture(c->ops->context, e))
      return 0;
    s->action = 0;
    s->page = 0;
  }
  return 1;
}
static int draw(Context *c, BkPauseFrame *f, unsigned slot, char e[256]) {
  BkPauseSprite *p = &c->s->sprites[slot];
  if (!bk_fade_sprite_advance(&p->fade, c->in->seconds))
    return fail(e, "invalid sprite state");
  return !c->s->loaded || append(f, slot, p);
}
int bk_pause_step(BkPauseState *s, const BkPauseBindings *b,
                  const BkPauseInput *in, const BkPauseOps *ops,
                  BkPauseFrame *f, char e[256]) {
  if (!s || !b || !b->common || !b->flow || b->flow->current != 4 ||
      !b->cursor || !b->pause_overlay || !b->hover_latched || !in ||
      !isfinite(in->seconds) || in->seconds < 0 || !isfinite(in->scale) ||
      in->scale <= 0 || (in->buttons & ~31u) || !f || !ops || !ops->sound ||
      !ops->warp || !ops->pointer || !ops->release || !ops->remove_capture ||
      s->page < 0 || s->page > 3)
    return fail(e, "invalid state/input/services");
  for (unsigned i = 0; i < 20; ++i)
    for (unsigned j = 0; j < 4; ++j)
      if (!isfinite(s->sprites[i].rect[j]))
        return fail(e, "invalid retained rectangle");
  if (!isfinite(s->cursor[0]) || !isfinite(s->cursor[1]))
    return fail(e, "invalid retained pointer");
  Context c = {s, b, in, ops};
  *f = (BkPauseFrame){0};
  int page = s->page;
  if (!(page < 2 ? main_page(&c, e) : confirmation(&c, page, e)))
    return 0;
  if (s->page < 0 || s->page > 3)
    return fail(e, "loader left undefined pause page");
  if (!bk_fade_sprite_request(&s->sprites[0].fade, s->background_show))
    return fail(e, "invalid dim sprite");
  for (unsigned i = 1; i <= 11; ++i)
    if (!bk_fade_sprite_request(&s->sprites[i].fade, s->page != 0))
      return fail(e, "invalid menu sprite");
  for (unsigned i = 12; i <= 17; ++i) {
    uint8_t wanted = i == 12   ? s->page == 2
                     : i == 13 ? s->page == 3
                               : s->page >= 2;
    if (!bk_fade_sprite_request(&s->sprites[i].fade, wanted))
      return fail(e, "invalid confirm sprite");
  }
  for (unsigned i = 0; i <= 6; ++i)
    if (!draw(&c, f, i, e))
      return 0;
  if (s->hover >= 2 && s->hover <= 6 && !draw(&c, f, (unsigned)s->hover + 5, e))
    return 0;
  if (!bk_fade_sprite_request(&s->sprites[18].fade, s->page >= 2))
    return fail(e, "invalid secondary dim sprite");
  if (!draw(&c, f, 18, e) || !draw(&c, f, 12, e) || !draw(&c, f, 13, e))
    return 0;
  if (!draw(&c, f, s->confirm_hover == 14 ? 15 : 14, e) ||
      !draw(&c, f, s->confirm_hover == 16 ? 17 : 16, e))
    return 0;
  float point[2], motion[2];
  if (!ops->pointer(ops->context, point, motion, e))
    return 0;
  for (unsigned i = 0; i < 2; ++i)
    if (!isfinite(point[i]) || !isfinite(motion[i]))
      return fail(e, "invalid live pointer");
  memcpy(s->cursor, point, sizeof(point));
  memcpy(s->motion, motion, sizeof(motion));
  BkMenuCursor *cursor = b->cursor;
  memcpy(cursor->sprite.rect, point, sizeof(point));
  if (!bk_fade_sprite_advance(&cursor->sprite.fade, in->seconds) ||
      !append(f, BK_PAUSE_CURSOR, &cursor->sprite) ||
      !bk_fade_sprite_request(&cursor->sprite.fade, cursor->wanted))
    return fail(e, "invalid shared cursor");
  if (motion[0] == 0 && motion[1] == 0) {
    if (bk_timer_poll(&cursor->idle, in->now_ms))
      cursor->wanted = 0;
  } else {
    cursor->wanted = 1;
    cursor->idle.armed = 0;
  }
  if (!bk_fade_sprite_advance(&b->common->curtain, in->seconds))
    return fail(e, "invalid shared curtain");
  BkPauseSprite curtain = {
      .fade = b->common->curtain,
      .rect = {0, 0, (float)(1280.0 * in->scale), (float)(960.0 * in->scale)}};
  if (!append(f, BK_PAUSE_CURTAIN, &curtain) ||
      !bk_fade_sprite_request(&b->common->curtain, b->common->blocked))
    return fail(e, "invalid curtain request");
  return 1;
}
