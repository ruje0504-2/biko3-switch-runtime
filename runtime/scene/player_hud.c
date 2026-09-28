#include "scene/player_hud.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
static int fail(char *e, const char *why) {
  snprintf(e, 256, "player HUD: %s", why);
  return 0;
}
static const BkPlayerHudLayout layouts[BK_PLAYER_HUD_SLOTS] = {
    [0] = {"ge_00.tga", 19, 8, 104, 88, 1},
    [1] = {"ge_05.tga", 94, 26, 336, 56, 1},
    [3] = {"ge_07.tga", 118, 37, 32, 32, 1},
    [4] = {"ge_08.tga", 394, 37, 32, 32, 1},
    [5] = {"ge_09.tga", 117, 26, 280, 56, 0},
    [6] = {"ge_10.bmp", 186, 32, 80, 16, 0},
    [7] = {"ge_11.tga", 18, 93, 104, 48, 1},
    [8] = {"ge_12.tga", 22, 93, 48, 48, 1},
    [9] = {"ge_13.tga", 70, 93, 48, 48, 1},
    [10] = {"ge_14.tga", 30, 140, 80, 16, 1},
    [11] = {"ge_15.tga", 1170, 12, 96, 64, 1},
    [12] = {"ge_16.tga", 1170, 12, 96, 64, 1},
    [13] = {"ge_17.tga", 1056, 864, 208, 80, 1},
    [14] = {"ge_18.tga", 1065, 873, 64, 64, 1},
    [15] = {"ge_19.tga", 1126, 873, 64, 64, 1},
    [16] = {"ge_20.tga", 1188, 862, 72, 72, 1},
    [17] = {"ge_21.tga", 1065, 873, 64, 64, 1},
    [18] = {"ge_22.tga", 1126, 873, 64, 64, 1},
    [19] = {"ge_23.tga", 1188, 862, 72, 72, 1},
    [20] = {"ge_24.tga", 1065, 873, 64, 64, 1},
    [21] = {"ge_28.tga", 1126, 873, 64, 64, 1},
    [22] = {"ge_30.tga", 1188, 862, 72, 72, 1},
    [23] = {"ge_34.tga", 1188, 925, 80, 24, 1},
    [24] = {"ge_35.tga", 0, 0, 168, 72, 1},
    [25] = {"ge_36.tga", 0, 0, 168, 72, 1},
    [26] = {"ge_37.tga", 0, 0, 168, 72, 1},
    [27] = {"ge_38.tga", 8, 880, 168, 72, 1},
    [28] = {"ge_39.tga", 8, 880, 168, 72, 1},
    [29] = {"ge_40.tga", 8, 880, 168, 72, 1},
    [30] = {"ge_41.tga", 0, 0, 1280, 960, 0},
    [31] = {"ge_42.tga", 0, 0, 1280, 960, 0},
    [32] = {"ge_43.tga", 0, 0, 32, 32, 1},
    [33] = {"ge_44.tga", 0, 0, 32, 32, 1},
    [34] = {"ge_45.tga", 0, 0, 32, 32, 1},
    [35] = {"ge_46.tga", 0, 0, 32, 32, 1},
    [36] = {"ge_47.tga", 0, 0, 32, 32, 1},
    [37] = {"ge_48.tga", 0, 0, 32, 32, 1},
    [38] = {"ge_49.tga", 0, 0, 32, 32, 1},
    [39] = {"ge_50.tga", 0, 0, 32, 32, 1},
    [40] = {"ge_51.tga", 0, 0, 32, 32, 1},
    [41] = {"ge_52.tga", 0, 0, 32, 32, 1},
    [42] = {"ge_53.tga", 0, 0, 32, 32, 1},
    [43] = {"ge_54.tga", 0, 0, 32, 32, 1},
    [44] = {"ge_35.tga", 8, 800, 168, 72, 1},
    [45] = {"ge_36.tga", 8, 800, 168, 72, 1},
    [46] = {"ge_37.tga", 8, 800, 168, 72, 1},
    [47] = {"ge_55.tga", 0, 0, 1280, 960, 0},
    [48] = {"ge_56.tga", 0, 0, 1280, 960, 0},
};
int bk_player_hud_layout(BkPlayerHudLayout out[BK_PLAYER_HUD_SLOTS],
                         unsigned group, uint8_t special, unsigned width) {
  if (!out || group >= 5 || !width || width > 16384)
    return 0;
  static const char *const group0[] = {"ge_00.tga", "ge_01.tga", "ge_02.tga",
                                       "ge_03.tga", "ge_04.tga"};
  static const char *const group21[] = {"ge_28.tga", "ge_25.tga", "ge_27.tga",
                                        "ge_28.tga", "ge_26.tga"};
  static const char *const group22[] = {"ge_30.tga", "ge_33.tga", "ge_29.tga",
                                        "ge_32.tga", "ge_31.tga"};
  memcpy(out, layouts, sizeof(layouts));
  out[0].name = group0[group];
  out[21].name = group21[group];
  out[22].name = group22[group];
  float scale = (float)((double)width / 1280);
  for (unsigned i = 0; i < BK_PLAYER_HUD_SLOTS; ++i) {
    if (special == 1 &&
        ((i >= 20 && i <= 22) || (i >= 31 && i <= 43) || i >= 47))
      out[i].name = NULL;
    out[i].x *= scale;
    out[i].y *= scale;
    out[i].width *= scale;
    out[i].height *= scale;
  }
  return 1;
}
int bk_player_hud_initialize(BkPlayerHudState *s, unsigned group,
                             uint8_t special, unsigned width) {
  BkPlayerHudLayout l[BK_PLAYER_HUD_SLOTS];
  if (!s || !bk_player_hud_layout(l, group, special, width))
    return 0;
  for (unsigned i = 0; i < BK_PLAYER_HUD_SLOTS; ++i) {
    if (!l[i].name)
      continue;
    BkPlayerHudSprite *p = &s->sprites[i];
    BkTimer timer = p->timer;
    uint8_t blink = p->blink;
    *p = (BkPlayerHudSprite){.x = l[i].x,
                             .y = l[i].y,
                             .width = l[i].width,
                             .height = l[i].height,
                             .sx = 1,
                             .sy = 1,
                             .uv = {0, 0, 1, 1},
                             .alpha = 1,
                             .rgb = 0xffffff,
                             .stage = l[i].requested,
                             .blink = blink,
                             .timer = timer};
  }
  s->extent = .18f;
  s->reserve = s->scroll = 1;
  s->indicator_x = 118;
  s->sprites[1].uv[0] = (float)(1.0 - s->extent);
  s->sprites[1].sx = s->extent;
  for (unsigned i = 24; i <= 26; ++i)
    s->sprites[i].px = s->sprites[i].py = .5f;
  s->sprites[25].timer.duration = s->sprites[28].timer.duration =
      s->sprites[45].timer.duration = 500;
  return 1;
}
static void request(BkPlayerHudSprite *s, int visible) {
  if (s->stage == 0 && visible == 1)
    s->stage = 1;
  else if (s->stage == 3 && visible == 0)
    s->stage = 4;
}
static int valid(const BkPlayerHudState *s) {
  if (!s || !isfinite(s->extent) || !isfinite(s->reserve) ||
      !isfinite(s->scroll) || !isfinite(s->indicator_x) ||
      !isfinite(s->depth) || s->reserve < 0 || s->reserve > 1 ||
      s->extent < 0 || s->scroll < 0 || s->scroll > 1)
    return 0;
  for (unsigned i = 0; i < BK_PLAYER_HUD_SLOTS; ++i) {
    const BkPlayerHudSprite *p = &s->sprites[i];
    if (!isfinite(p->x) || !isfinite(p->y) || !isfinite(p->width) ||
        !isfinite(p->height) || !isfinite(p->sx) || !isfinite(p->sy) ||
        !isfinite(p->px) || !isfinite(p->py) || !isfinite(p->alpha) ||
        p->alpha < 0 || p->alpha > 1 || p->stage > 5)
      return 0;
    for (unsigned j = 0; j < 4; ++j)
      if (!isfinite(p->uv[j]))
        return 0;
  }
  return 1;
}
static float smooth(float value, double target, float dt) {
  float d = (float)(target - value);
  d = (float)(2.0 * dt * d);
  return (float)((double)value + d);
}
int bk_player_hud_update(BkPlayerHudState *s, const BkPlayerHudInput *in,
                         uint8_t *outcome, const BkScreenPoint *projected,
                         float dt, uint32_t now, unsigned width, char e[256]) {
  if (!valid(s) || !in || !outcome || !projected ||
      !isfinite(projected->depth) || !isfinite(dt) || dt < 0 || !width ||
      width > 16384)
    return fail(e, "invalid state/update/projection");
  BkPlayerHudState n = *s;
  BkPlayerHudSprite *p = n.sprites;
  uint8_t result = *outcome;
  if (in->interface_mode == 2) {
    if (in->action == in->actions[11]) {
      if (in->trigger_kind == 10)
        request(&p[30], 1);
      else if (in->trigger_kind == 19)
        request(&p[48], 1);
      else if (in->trigger_kind == 18)
        request(&p[47], 1);
    } else if (in->action == in->actions[13] && in->special_mode != 1 &&
               in->trigger_kind == 11)
      request(&p[31], 1);
  } else {
    request(&p[30], 0);
    if (in->special_mode != 1) {
      request(&p[31], 0);
      request(&p[47], 0);
      request(&p[48], 0);
    }
  }
  if (in->npc_in_view == 0) {
    n.extent = smooth(n.extent, 1, dt);
    if (n.extent >= 1)
      n.extent = 1;
    n.indicator_x = smooth(n.indicator_x, 394, dt);
    if (n.indicator_x >= 394)
      n.indicator_x = 394;
  } else if (in->npc_in_view == 1 && n.reserve >= 1) {
    n.extent = smooth(n.extent, .18, dt);
    if (n.extent <= .18)
      n.extent = .18f;
    n.indicator_x = smooth(n.indicator_x, 118, dt);
    if (n.indicator_x <= 118)
      n.indicator_x = 118;
  }
  request(&p[6], n.extent >= .99);
  request(&p[5], n.extent >= .99);
  if (p[5].stage == 3 && in->menu_request == 0 && in->response == 0) {
    if (in->npc_in_view == 0) {
      n.reserve = (float)(n.reserve - (double)dt * .01);
      if (n.reserve <= 0) {
        n.reserve = 0;
        result = 4;
      }
      n.scroll = (float)(n.scroll - (double)dt * .5);
      if (n.scroll <= 0)
        n.scroll = 1;
    } else if (in->npc_in_view == 1) {
      n.reserve = (float)(n.reserve + (double)dt * .05);
      if (n.reserve >= 1)
        n.reserve = 1;
      n.scroll = (float)(n.scroll + (double)dt * .5);
      if (n.scroll >= 1)
        n.scroll = 0;
    }
  }
  p[1].uv[0] = (float)(1.0 - n.extent);
  p[1].uv[1] = 0;
  p[1].uv[2] = p[1].uv[3] = 1;
  p[1].sx = n.extent;
  p[1].sy = 1;
  p[5].uv[0] = p[5].uv[1] = 0;
  p[5].uv[2] = n.reserve;
  p[5].uv[3] = 1;
  p[5].sx = n.reserve;
  p[5].sy = 1;
  p[5].rgb = 0xff00ff | ((uint32_t)(255.0 * n.reserve) << 8);
  p[6].uv[0] = (float)(1.0 + n.scroll);
  p[6].uv[1] = 0;
  p[6].uv[2] = n.scroll;
  p[6].uv[3] = 1;
  request(&p[3], n.extent < .99);
  request(&p[4], n.extent >= .99);
  float scale = (float)((double)width / 1280);
  p[3].x = p[4].x = n.indicator_x * scale;
  n.depth = projected->depth;
  if (n.depth < 1 && in->prop_available == 1)
    for (unsigned i = 24; i <= 26; ++i) {
      p[i].x = (float)projected->position[0];
      p[i].y = (float)projected->position[1];
    }
  const unsigned timers[] = {25, 45, 28};
  for (unsigned i = 0; i < 3; ++i) {
    BkPlayerHudSprite *b = &p[timers[i]];
    if (bk_timer_poll(&b->timer, now) && b->blink <= 1)
      b->blink ^= 1;
  }
  if (!valid(&n))
    return fail(e, "update overflow");
  *s = n;
  *outcome = result;
  return 1;
}
static void draw(BkPlayerHudState *s, BkPlayerHudFrame *f, unsigned slot) {
  BkPlayerHudSprite *p = &s->sprites[slot];
  switch (p->stage) {
  case 0:
    p->alpha = 0;
    break;
  case 1:
    p->alpha = 1;
    p->stage = 2;
    break;
  case 2:
    p->alpha = 1;
    p->stage = 3;
    break;
  case 3:
    p->alpha = 1;
    break;
  case 4:
    p->sx = p->sy = 1; /* fall through */
  case 5:
    p->alpha = 0;
    p->stage = 0;
    break;
  }
  f->draws[f->count++] = (BkPlayerHudDraw){slot, *p};
}
int bk_player_hud_draws(BkPlayerHudState *s, const BkPlayerHudInput *in,
                        unsigned width, BkPlayerHudFrame *out, char e[256]) {
  if (!valid(s) || !in || !out || !width || width > 16384)
    return fail(e, "invalid draw state/extent");
  int32_t digits[3] = {43, 42, 42};
  if (in->counter < 100) {
    int h = in->counter / 100, t = in->counter / 10 - h * 10;
    digits[0] = 32 + h;
    digits[1] = 32 + t;
    digits[2] = 32 + in->counter - (h * 100 + t * 10);
    for (unsigned i = 0; i < 3; ++i)
      if (digits[i] < 0 || digits[i] >= BK_PLAYER_HUD_SLOTS)
        return fail(e, "counter addresses outside HUD slots");
  }
  BkPlayerHudState n = *s;
  BkPlayerHudFrame f = {0};
  draw(&n, &f, 30);
  if (in->special_mode != 1) {
    draw(&n, &f, 31);
    draw(&n, &f, 47);
    draw(&n, &f, 48);
    f.capture = 1;
    f.capture_after = f.count;
  }
  int suppressed = in->action == in->actions[11] ||
                   in->action == in->actions[13] ||
                   in->action == in->actions[16];
  if (n.depth < 1 && in->prop_available == 1 && !suppressed) {
    if (n.sprites[25].blink <= 1)
      draw(&n, &f, 25 + n.sprites[25].blink);
    draw(&n, &f, 24);
  }
  if (in->cover_available == 1) {
    if (n.sprites[45].blink <= 1)
      draw(&n, &f, 45 + n.sprites[45].blink);
    draw(&n, &f, 44);
  }
  if (in->npc_prompt == 1 && !suppressed && in->npc_behavior == 0) {
    if (n.sprites[28].blink <= 1)
      draw(&n, &f, 28 + n.sprites[28].blink);
    draw(&n, &f, 27);
  }
  const unsigned fixed[] = {6, 1, 5, 4, 3, 0, 7};
  for (unsigned i = 0; i < 7; ++i)
    draw(&n, &f, fixed[i]);
  if (in->inventory[3] == 1)
    draw(&n, &f, 8);
  if (in->inventory[4] == 1)
    draw(&n, &f, 9);
  draw(&n, &f, 10);
  draw(&n, &f, 12);
  float scale = (float)((double)width / 1280);
  for (unsigned i = 0; i < 3; ++i) {
    n.sprites[digits[i]].x = (1172.f + i * 24.f) * scale;
    n.sprites[digits[i]].y = 76 * scale;
    draw(&n, &f, (unsigned)digits[i]);
  }
  draw(&n, &f, 13);
  const unsigned inventory[] = {1, 2, 0};
  for (unsigned i = 0; i < 3; ++i) {
    uint8_t has = in->inventory[inventory[i]];
    if (has == 1 && in->special_mode != 1) {
      draw(&n, &f, 17 + i);
      draw(&n, &f, 20 + i);
    } else if (has == 0)
      draw(&n, &f, 14 + i);
  }
  draw(&n, &f, 23);
  *s = n;
  *out = f;
  return 1;
}
int bk_player_hud_rect(const BkPlayerHudSprite *p, float r[4]) {
  if (!p || !r)
    return 0;
  float x = p->width * p->px, y = p->height * p->py;
  float w = p->width, h = p->height;
  if (p->sx != 1 || p->sy != 1) {
    x *= p->sx;
    y *= p->sy;
    w *= p->sx;
    h *= p->sy;
  }
  float right = -x + w, bottom = -y + h;
  float rect[4] = {-x + p->x, -y + p->y, right + p->x, bottom + p->y};
  for (unsigned i = 0; i < 4; ++i)
    if (!isfinite(rect[i]))
      return 0;
  memcpy(r, rect, sizeof(rect));
  return 1;
}

void bk_player_hud_reset_reserve(BkPlayerHudState *s) {
  if (s)
    s->reserve = 1;
}
