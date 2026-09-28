#include "scene/title_menu.h"
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static int fail(char e[256], const char *why) {
  snprintf(e, 256, "title menu: %s", why);
  return 0;
}
static int present(unsigned slot, uint8_t special) {
  return special != 1 || slot <= 2 || slot == 9 || slot == 10;
}
const char *bk_title_menu_image(unsigned slot, uint8_t special) {
  static const char *const names[] = {
      "op_00.bmp", "op_06.tga", "op_01.tga", "op_07.tga", "op_02.tga",
      "op_08.tga", "op_03.tga", "op_09.tga", "op_04.tga", "op_10.tga",
      "op_05.tga", "op_12.tga", "op_11.tga", "ma_00.tga", "ma_01.tga"};
  if (slot >= 15 || (slot < 13 && !present(slot, special)))
    return NULL;
  return slot == 0 && special == 1 ? "te_10.bmp" : names[slot];
}
int bk_title_menu_initialize(BkTitleMenuState *s, unsigned width,
                             uint8_t special, int32_t master,
                             const BkTitleMenuOps *ops, char e[256]) {
  if (!s || !width || width > 16384 || master < -10000 || master > 0 || !ops ||
      !ops->warp)
    return fail(e, "invalid initializer/services");
  static const float ys[] = {25, 95, 164, 234, 375, 305};
  float scale = (float)((double)width / 1280);
  for (unsigned i = 0; i < BK_TITLE_SPRITES; ++i) {
    if (!present(i, special))
      continue;
    BkTitleSprite *p = &s->sprites[i];
    *p = (BkTitleSprite){0};
    bk_zoom_sprite_initialize(&p->zoom);
    if (i == 0) {
      p->rect[2] = 1280.f * scale;
      p->rect[3] = 960.f * scale;
      p->zoom.pivot[0] = p->zoom.pivot[1] = 0;
      p->zoom.fade.stage = 1;
    } else {
      p->rect[0] = (special == 1 ? 784.f : 956.f) * scale;
      p->rect[1] =
          (special == 1 ? (i <= 2 ? 560.f : 632.f) : ys[(i - 1) / 2]) * scale;
      p->rect[2] = 256.f * scale;
      p->rect[3] = 56.f * scale;
    }
    p->half[0] = (int32_t)((double)p->rect[2] / 2);
    p->half[1] = (int32_t)((double)p->rect[3] / 2);
    if (i) {
      p->rect[0] = (float)((double)p->rect[0] + p->half[0]);
      p->rect[1] = (float)((double)p->rect[1] + p->half[1]);
    }
    s->loaded_mask |= (uint16_t)(1u << i);
  }
  s->music_volume = master;
  s->music_mode = 1;
  return ops->warp(ops->context, 320, 240, e);
}
static int inside(const BkTitleSprite *s, const float p[2]) {
  return (double)s->rect[0] - s->half[0] <= p[0] &&
         (double)s->rect[0] + s->half[0] >= p[0] &&
         (double)s->rect[1] - s->half[1] <= p[1] &&
         (double)s->rect[1] + s->half[1] >= p[1];
}
static int draw(BkTitleMenuState *s, unsigned slot, float seconds,
                BkTitleMenuFrame *f) {
  BkTitleSprite *p = &s->sprites[slot];
  if (slot) {
    if (!bk_zoom_sprite_advance(&p->zoom, seconds))
      return 0;
  } else {
    BkFadeSprite *b = &p->zoom.fade;
    switch (b->stage) {
    case 0:
      b->alpha = 0;
      break;
    case 1:
      b->alpha = 1;
      b->stage = 2;
      break;
    case 2:
      b->stage = 3; /* fall through */
    case 3:
      b->alpha = 1;
      break;
    case 4:
      p->zoom.scale[0] = p->zoom.scale[1] = 1; /* fall through */
    case 5:
      b->alpha = 0;
      b->stage = 0;
      break;
    default:
      return 0;
    }
  }
  if (!(s->loaded_mask & (1u << slot)))
    return 1;
  if (f->count >= BK_TITLE_DRAWS)
    return 0;
  BkTitleMenuDraw *d = &f->draws[f->count++];
  d->slot = slot;
  d->alpha = p->zoom.fade.alpha;
  return bk_zoom_sprite_rect(&p->zoom, p->rect, d->corners);
}
static int flat(BkTitleMenuFrame *f, unsigned slot, const float rect[4],
                float alpha) {
  if (f->count >= BK_TITLE_DRAWS)
    return 0;
  BkTitleMenuDraw *d = &f->draws[f->count++];
  *d = (BkTitleMenuDraw){
      slot, {rect[0], rect[1], rect[0] + rect[2], rect[1] + rect[3]}, alpha};
  return 1;
}
typedef struct {
  BkTitleMenuState *s;
  const BkTitleMenuOps *ops;
} Release;
static int release(void *p, uint8_t flow, char e[256]) {
  Release *c = p;
  if (!c->ops->release(c->ops->context, flow, e))
    return 0;
  if (flow == 1)
    c->s->loaded_mask = 0;
  return 1;
}
static void request_pair(BkTitleMenuState *s, unsigned slot) {
  bk_fade_sprite_request(&s->sprites[slot].zoom.fade, 1);
  bk_fade_sprite_request(&s->sprites[slot + 1].zoom.fade, 1);
}
int bk_title_menu_step(BkTitleMenuState *s, const BkTitleMenuBindings *b,
                       const BkTitleMenuInput *in, const BkTitleMenuOps *ops,
                       BkTitleMenuFrame *out, char e[256]) {
  if (!s || !b || !b->common || !b->flow || !b->cursor || !b->hover_latched ||
      !in || !out || !ops || !ops->sound || !ops->music_gain || !ops->warp ||
      !ops->position || !ops->motion || !ops->release ||
      b->flow->current != 1 || !isfinite(in->seconds) || in->seconds < 0 ||
      (double)in->seconds * 400 > INT32_MAX - 10000 || !isfinite(in->scale) ||
      in->scale <= 0 || in->scale > 16 || in->music_master < -10000 ||
      in->music_master > 0 || s->music_volume < -10000 || s->music_volume > 0)
    return fail(e, "invalid state/input/services");
  const unsigned rows[] = {1, 3, 5, 7, 11, 9};
  float ys[6], point[2], motion[2];
  for (unsigned i = 0; i < 6; ++i)
    ys[i] = s->sprites[rows[i]].rect[1];
  if (!ops->position(ops->context, point, e))
    return 0;
  if (!isfinite(point[0]) || !isfinite(point[1]))
    return fail(e, "invalid pointer");
  int32_t delta = (int32_t)(float)((double)in->seconds * 400);
  if (delta <= 1)
    delta = 1;
  if (s->music_mode == 1) {
    s->music_volume += delta;
    if (s->music_volume >= in->music_master)
      s->music_volume = in->music_master;
  } else if (s->music_mode == 0) {
    s->music_volume -= delta;
    if (s->music_volume <= -6000)
      s->music_volume = -6000;
  }
  if (!ops->music_gain(ops->context, s->music_volume, e))
    return 0;
  unsigned hover = 0;
  for (unsigned slot = 1; slot < 13; slot += 2) {
    if (!present(slot, in->special) || !inside(&s->sprites[slot], point))
      continue;
    hover = slot;
    if (in->buttons & BK_PAUSE_CONFIRM) {
      if (!ops->sound(ops->context, 1, e))
        return 0;
      b->common->action = (uint8_t)slot;
      b->common->blocked = 1;
      s->music_mode = 0;
    }
  }
  if (hover) {
    if (*b->hover_latched == 0) {
      if (!ops->sound(ops->context, 3, e))
        return 0;
      *b->hover_latched = 1;
    }
  } else
    *b->hover_latched = 0;
  if (in->buttons & (BK_PAUSE_UP | BK_PAUSE_DOWN)) {
    if (in->special == 1) {
      if (s->row == 0)
        s->row = 5;
      else if (s->row == 5)
        s->row = 0;
    } else {
      uint32_t v = (uint32_t)s->row;
      v += in->buttons & BK_PAUSE_UP ? UINT32_MAX : 1u;
      int64_t signed_v = v <= INT32_MAX ? v : (int64_t)v - INT64_C(4294967296);
      s->row = (int32_t)signed_v;
      if ((in->buttons & BK_PAUSE_UP) && s->row < 0)
        s->row = 5;
      else if (!(in->buttons & BK_PAUSE_UP) && s->row > 5)
        s->row = 0;
    }
    if (s->row < 0 || s->row > 5)
      return fail(e, "keyboard row outside original initialized array");
    if (!ops->warp(ops->context,
                   (float)((in->special == 1 ? 912.0 : 1116.0) * in->scale),
                   ys[s->row], e))
      return 0;
  }
  BkTitleMenuFrame f = {0};
  if (!draw(s, 0, in->seconds, &f))
    return fail(e, "invalid backdrop");
  for (unsigned slot = 1; slot < 13; slot += 2)
    if (present(slot, in->special) &&
        !draw(s, slot + (hover == slot), in->seconds, &f))
      return fail(e, "invalid button animation");
  BkMenuCursor *c = b->cursor;
  c->sprite.rect[0] = point[0];
  c->sprite.rect[1] = point[1];
  if (!bk_fade_sprite_advance(&c->sprite.fade, in->seconds) ||
      !flat(&f, BK_TITLE_CURSOR, c->sprite.rect, c->sprite.fade.alpha) ||
      !bk_fade_sprite_request(&c->sprite.fade, c->wanted))
    return fail(e, "invalid shared cursor");
  if (!ops->motion(ops->context, motion, e))
    return 0;
  if (!isfinite(motion[0]) || !isfinite(motion[1]))
    return fail(e, "invalid relative pointer");
  if (motion[0] == 0 && motion[1] == 0) {
    if (bk_timer_poll(&c->idle, in->now_ms))
      c->wanted = 0;
  } else {
    c->wanted = 1;
    c->idle.armed = 0;
  }
  BkCommonHudState *common = b->common;
  const float full[4] = {0, 0, 1280.f * in->scale, 960.f * in->scale};
  if (!bk_fade_sprite_advance(&common->curtain, in->seconds) ||
      !flat(&f, BK_TITLE_CURTAIN, full, common->curtain.alpha) ||
      !bk_fade_sprite_request(&common->curtain, common->blocked))
    return fail(e, "invalid shared curtain");
  if (common->blocked == 1 && common->curtain.stage == 3) {
    common->blocked = 0;
    const uint8_t targets[] = {0x38, 0x28, 0x30, 0x18, 0x58, 0x60};
    if (common->action >= 1 && common->action <= 11 && common->action % 2) {
      Release r = {s, ops};
      BkFlowTransitionOps fo = {&r, release};
      if (!bk_flow_transition_schedule(b->flow, &fo,
                                       targets[(common->action - 1) / 2],
                                       common->action <= 3 ? 1 : 0, e))
        return 0;
    }
    common->action = 0;
    s->row = 0;
  }
  if (common->blocked == 0 && common->curtain.stage == 0) {
    request_pair(s, 1);
    if (in->special == 1) {
      if (s->sprites[1].zoom.fade.stage == 3 ||
          s->sprites[2].zoom.fade.stage == 3)
        request_pair(s, 9);
      if (in->buttons & BK_PAUSE_CONFIRM) {
        request_pair(s, 1);
        request_pair(s, 9);
      }
    } else {
      for (unsigned i = 1; i < 6; ++i) {
        unsigned prior = rows[i - 1];
        if (s->sprites[prior].zoom.fade.stage != 3 &&
            s->sprites[prior + 1].zoom.fade.stage != 3)
          break;
        request_pair(s, rows[i]);
      }
      if (in->buttons & BK_PAUSE_CONFIRM)
        for (unsigned i = 1; i < 13; ++i)
          bk_fade_sprite_request(&s->sprites[i].zoom.fade, 1);
    }
  }
  *out = f;
  return 1;
}
