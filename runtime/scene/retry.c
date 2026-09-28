#include "scene/retry.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
static const struct {
  const char *name;
  float rect[4];
} layout[6] = {
    {"ma_07.bmp", {184, 400, 880, 160}}, {"ma_08.bmp", {320, 344, 640, 280}},
    {"lo_15.tga", {368, 520, 256, 56}},  {"lo_16.tga", {368, 520, 256, 56}},
    {"lo_17.tga", {656, 520, 256, 56}},  {"lo_18.tga", {656, 520, 256, 56}}};
static int fail(char *e, const char *why) {
  snprintf(e, 256, "retry: %s", why);
  return 0;
}
const char *bk_retry_image(unsigned i) {
  return i < 6                   ? layout[i].name
         : i == BK_PAUSE_CURSOR  ? "ma_00.tga"
         : i == BK_PAUSE_CURTAIN ? "ma_01.tga"
                                 : NULL;
}
int bk_retry_initialize(BkRetryState *s, unsigned width, const BkPauseOps *ops,
                        char e[256]) {
  if (!s || !width || width > 16384 || !ops || !ops->warp)
    return fail(e, "invalid initializer");
  float scale = (float)((double)width / 1280);
  for (unsigned i = 0; i < 6; ++i) {
    bk_fade_sprite_initialize(&s->sprites[i].fade);
    for (unsigned j = 0; j < 4; ++j)
      s->sprites[i].rect[j] = (float)((double)layout[i].rect[j] * scale);
  }
  bk_fade_sprite_request(&s->sprites[0].fade, 1);
  s->loaded = 1;
  return ops->warp(ops->context, (float)(496.0 * scale), (float)(548.0 * scale),
                   e);
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
static int draw(BkRetryState *s, unsigned slot, float seconds,
                BkPauseFrame *f) {
  return bk_fade_sprite_advance(&s->sprites[slot].fade, seconds) &&
         (!s->loaded || append(f, slot, &s->sprites[slot]));
}
static int inside(const BkPauseSprite *s, const float point[2]) {
  const float *r = s->rect;
  return r[0] <= point[0] && (double)r[0] + r[2] >= point[0] &&
         r[1] <= point[1] && (double)r[1] + r[3] >= point[1];
}
typedef struct {
  BkRetryState *state;
  const BkPauseOps *ops;
} Release;
static int release(void *context, uint8_t flow, char e[256]) {
  Release *r = context;
  if (!r->ops->release(r->ops->context, flow, e))
    return 0;
  if (flow == 0x68)
    r->state->loaded = 0;
  return 1;
}
int bk_retry_step(BkRetryState *s, const BkPauseBindings *b,
                  const BkPauseInput *in, const BkPauseOps *ops,
                  BkPauseFrame *f, char e[256]) {
  if (!s || !b || !b->common || !b->flow || !b->cursor || !b->hover_latched ||
      !in || !ops || !ops->sound || !ops->warp || !ops->pointer ||
      !ops->release || !f || b->flow->current != 0x68 ||
      !isfinite(in->seconds) || in->seconds < 0 || !isfinite(in->scale) ||
      in->scale <= 0 || in->scale > 16)
    return fail(e, "invalid live bindings/time");
  for (unsigned i = 0; i < 8; ++i) {
    BkFadeSprite check = i < 6    ? s->sprites[i].fade
                         : i == 6 ? b->cursor->sprite.fade
                                  : b->common->curtain;
    if (!bk_fade_sprite_advance(&check, in->seconds))
      return fail(e, "invalid sprite");
  }
  float point[2], motion[2];
  if (!ops->pointer(ops->context, point, motion, e))
    return 0;
  for (unsigned i = 0; i < 2; ++i)
    if (!isfinite(point[i]) || !isfinite(motion[i]))
      return fail(e, "invalid pointer");
  int confirm = (in->buttons & BK_PAUSE_CONFIRM) != 0;
  if (s->phase == 0 && confirm)
    s->phase = 1;
  if (s->phase == 1) {
    bk_fade_sprite_request(&s->sprites[0].fade, 0);
    if (s->sprites[0].fade.stage == 0) {
      for (unsigned i = 1; i < 6; ++i)
        bk_fade_sprite_request(&s->sprites[i].fade, 1);
      s->phase = 2;
    }
  }
  unsigned hover = 0;
  if (s->phase == 2) {
    for (unsigned i = 2; i <= 4; i += 2) {
      if (!inside(&s->sprites[i], point))
        continue;
      hover = i;
      if (confirm) {
        if (!ops->sound(ops->context, i == 2 ? 0 : 2, e))
          return 0;
        b->common->action = (uint8_t)i;
        b->common->blocked = 1;
      }
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
  *f = (BkPauseFrame){0};
  if (!draw(s, 0, in->seconds, f) || !draw(s, 1, in->seconds, f) ||
      !draw(s, hover == 2 ? 3 : 2, in->seconds, f) ||
      !draw(s, hover == 4 ? 5 : 4, in->seconds, f))
    return fail(e, "invalid menu snapshot");
  BkMenuCursor *c = b->cursor;
  memcpy(c->sprite.rect, point, sizeof(point));
  if (!bk_fade_sprite_advance(&c->sprite.fade, in->seconds) ||
      !append(f, BK_PAUSE_CURSOR, &c->sprite) ||
      !bk_fade_sprite_request(&c->sprite.fade, c->wanted))
    return fail(e, "invalid cursor");
  /*51c029..071: x87 C3 equality bits keep the timer only while still. */
  if (motion[0] == 0 && motion[1] == 0) {
    if (bk_timer_poll(&c->idle, in->now_ms))
      c->wanted = 0;
  } else {
    c->wanted = 1;
    c->idle.armed = 0;
  }
  if (!bk_fade_sprite_advance(&b->common->curtain, in->seconds))
    return fail(e, "invalid curtain");
  BkPauseSprite curtain = {
      .fade = b->common->curtain,
      .rect = {0, 0, (float)(1280.0 * in->scale), (float)(960.0 * in->scale)}};
  if (!append(f, BK_PAUSE_CURTAIN, &curtain) ||
      !bk_fade_sprite_request(&b->common->curtain, b->common->blocked))
    return fail(e, "invalid curtain request");
  if (in->buttons & (BK_PAUSE_LEFT | BK_PAUSE_RIGHT)) {
    if (in->buttons & BK_PAUSE_LEFT) {
      s->selected = (int32_t)((uint32_t)s->selected - 1u);
      if (s->selected < 0)
        s->selected = 1;
    } else {
      s->selected = (int32_t)((uint32_t)s->selected + 1u);
      if (s->selected > 1)
        s->selected = 0;
    }
    if (s->selected < 0 || s->selected > 1)
      return fail(e, "invalid native selection index");
    if (!ops->warp(ops->context,
                   (float)((s->selected ? 788.0 : 496.0) * in->scale),
                   (float)(548.0 * in->scale), e))
      return 0;
  }
  if (s->phase == 2 && b->common->blocked == 1 &&
      b->common->curtain.stage == 3) {
    b->common->blocked = 0;
    Release context = {s, ops};
    BkFlowTransitionOps transition = {&context, release};
    if (b->common->action == 2) {
      if (!bk_flow_transition_schedule(b->flow, &transition, 2, 1, e))
        return 0;
    } else if (b->common->action == 4) {
      if (!bk_flow_transition_schedule(b->flow, &transition, 1, 0, e))
        return 0;
    }
    b->common->action = 0;
    s->phase = 0;
    s->selected = 0;
  }
  return 1;
}
