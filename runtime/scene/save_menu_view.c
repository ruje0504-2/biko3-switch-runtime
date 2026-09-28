#include "scene/save_menu_view.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
/*4e97fd+50ace3 fixed geometry, verified against original constructors.*/
static const struct {
  const char *name;
  float rect[4];
  uint8_t requested, instant;
} layout[56] = {
    {"lo_00.bmp", {0, 0, 1280, 960}, 1, 1},
    {"lo_12.tga", {976, 880, 256, 56}, 1, 1},
    {"lo_13.tga", {976, 880, 256, 56}, 1, 1},
    {"lo_01.tga", {432, 144, 840, 656}, 1, 1},
    {"lo_02.tga", {432, 144, 840, 656}, 1, 1},
    {"lo_03.tga", {432, 144, 840, 656}, 1, 1},
    {"lo_04.tga", {432, 144, 840, 656}, 1, 1},
    {"lo_05.tga", {432, 144, 840, 656}, 1, 1},
    {"lo_11.tga", {438, 234, 624, 56}, 1, 1},
    {"lo_14.tga", {320, 344, 640, 280}, 0, 0},
    {"lo_15.tga", {368, 520, 256, 56}, 0, 0},
    {"lo_16.tga", {368, 520, 256, 56}, 0, 0},
    {"lo_17.tga", {656, 520, 256, 56}, 0, 0},
    {"lo_18.tga", {656, 520, 256, 56}, 0, 0},
    {"lo_06.tga", {0, 96, 800, 864}, 0, 0},
    {"lo_07.tga", {0, 96, 800, 864}, 0, 0},
    {"lo_08.tga", {0, 96, 800, 864}, 0, 0},
    {"lo_09.tga", {0, 96, 800, 864}, 0, 0},
    {"lo_10.tga", {0, 96, 800, 864}, 0, 0},
    {"sa_00.bmp", {0, 0, 1280, 960}, 1, 1},
    {"sa_14.tga", {320, 344, 640, 280}, 0, 0},
    {"lo_83.tga", {344, 440, 600, 56}, 0, 0},
    {"lo_84.tga", {344, 440, 600, 56}, 0, 0},
    {"lo_85.tga", {344, 440, 600, 56}, 0, 0},
    {"lo_86.tga", {344, 440, 600, 56}, 0, 0},
    {"lo_87.tga", {344, 440, 600, 56}, 0, 0},
    {"lo_88.tga", {344, 440, 600, 56}, 0, 0},
    {"lo_89.tga", {344, 440, 600, 56}, 0, 0},
    {"lo_90.tga", {344, 440, 600, 56}, 0, 0},
    {"lo_91.tga", {344, 440, 600, 56}, 0, 0},
    {"lo_92.tga", {344, 440, 600, 64}, 0, 0},
    {"lo_74.tga", {1064, 224, 200, 80}, 0, 0},
    {"lo_75.tga", {1064, 224, 200, 80}, 0, 0},
    {"lo_76.tga", {1064, 224, 200, 80}, 0, 0},
    {"lo_77.tga", {1064, 224, 200, 80}, 0, 0},
    {"lo_78.tga", {1064, 224, 200, 80}, 0, 0},
    {"lo_79.tga", {1064, 224, 200, 80}, 0, 0},
    {"lo_80.tga", {1064, 224, 200, 80}, 0, 0},
    {"lo_81.tga", {1064, 224, 200, 80}, 0, 0},
    {"lo_82.tga", {1064, 224, 200, 80}, 0, 0},
    {"lo_19.tga", {1132, 707, 64, 64}, 0, 1},
    {"lo_93.tga", {432, 144, 840, 656}, 1, 1},
    {"lo_22.tga", {1196, 636, 64, 64}, 0, 1},
    {"lo_20.tga", {1072, 640, 64, 64}, 0, 1},
    {"lo_21.tga", {1134, 640, 64, 64}, 0, 1},
    {"lo_23.tga", {1096, 568, 64, 64}, 0, 1},
    {"lo_24.tga", {1160, 568, 64, 64}, 0, 1},
    {"lo_45.bmp", {1064, 328, 200, 144}, 0, 1},
    {"lo_46.bmp", {1064, 328, 200, 144}, 0, 1},
    {"lo_48.bmp", {1064, 328, 200, 144}, 0, 1},
    {"lo_47.bmp", {1064, 328, 200, 144}, 0, 1},
    {"lo_49.bmp", {1064, 328, 200, 144}, 0, 1},
    {"lo_50.bmp", {1064, 328, 200, 144}, 0, 1},
    {"lo_51.bmp", {1064, 328, 200, 144}, 0, 1},
    {"lo_52.bmp", {1064, 328, 200, 144}, 0, 1},
    {"lo_53.bmp", {1064, 328, 200, 144}, 0, 1},
};
static const char *const details[5][14] = {
    {"lo_22.tga", "lo_20.tga", "lo_21.tga", "lo_23.tga", "lo_24.tga",
     "lo_45.bmp", "lo_46.bmp", "lo_48.bmp", "lo_47.bmp", "lo_49.bmp",
     "lo_50.bmp", "lo_51.bmp", "lo_52.bmp", "lo_53.bmp"},
    {"lo_27.tga", "lo_20.tga", "lo_26.tga", "lo_28.tga", "lo_29.tga",
     "lo_47.bmp", "lo_45.bmp", "lo_46.bmp", "lo_48.bmp", "lo_54.bmp",
     "lo_55.bmp", "lo_56.bmp", "lo_57.bmp", "lo_58.bmp"},
    {"lo_32.tga", "lo_20.tga", "lo_31.tga", "lo_33.tga", "lo_34.tga",
     "lo_46.bmp", "lo_45.bmp", "lo_47.bmp", "lo_48.bmp", "lo_59.bmp",
     "lo_60.bmp", "lo_61.bmp", "lo_62.bmp", "lo_63.bmp"},
    {"lo_37.tga", "lo_20.tga", "lo_21.tga", "lo_38.tga", "lo_39.tga",
     "lo_48.bmp", "lo_46.bmp", "lo_45.bmp", "lo_47.bmp", "lo_64.bmp",
     "lo_65.bmp", "lo_66.bmp", "lo_67.bmp", "lo_68.bmp"},
    {"lo_42.tga", "lo_20.tga", "lo_41.tga", "lo_43.tga", "lo_44.tga",
     "lo_45.bmp", "lo_47.bmp", "lo_48.bmp", "lo_46.bmp", "lo_69.bmp",
     "lo_70.bmp", "lo_71.bmp", "lo_72.bmp", "lo_73.bmp"},
};
const char *bk_save_menu_image(unsigned slot, unsigned group, unsigned mode) {
  if (group >= 5 || mode > 1)
    return NULL;
  static const char *const saves[] = {"sa_92.tga", "sa_93.tga", "sa_94.tga",
                                      "sa_95.tga", "sa_96.tga"};
  if (mode && slot >= 14 && slot <= 18)
    return saves[slot - 14];
  if (slot == BK_SAVE_MENU_CURSOR)
    return "ma_00.tga";
  if (slot == BK_SAVE_MENU_CURTAIN)
    return "ma_01.tga";
  return slot < 42   ? layout[slot].name
         : slot < 56 ? details[group][slot - 42]
                     : NULL;
}
static void initialize_slot(BkSaveMenuView *v, unsigned slot, float scale) {
  BkPauseSprite *p = &v->sprites[slot];
  bk_fade_sprite_initialize(&p->fade);
  for (unsigned j = 0; j < 4; ++j)
    p->rect[j] = (float)((double)layout[slot].rect[j] * scale);
  bk_fade_sprite_request(&p->fade, layout[slot].requested);
  v->loaded |= UINT64_C(1) << slot;
}
int bk_save_menu_view_details(BkSaveMenuView *v, unsigned group,
                              unsigned width) {
  if (!v || group >= 5 || !width || width > 16384)
    return 0;
  float scale = (float)((double)width / 1280);
  for (unsigned i = 42; i < 56; ++i)
    initialize_slot(v, i, scale);
  return 1;
}
int bk_save_menu_view_initialize(BkSaveMenuView *v, unsigned mode,
                                 unsigned group, unsigned width) {
  if (!v || mode > 1 || group >= 5 || !width || width > 16384)
    return 0;
  v->loaded = 0;
  float scale = (float)((double)width / 1280);
  for (unsigned i = 0; i < 42; ++i) {
    if ((mode && (i == 0 || i == 9)) || (!mode && (i == 19 || i == 20)))
      continue;
    initialize_slot(v, i, scale);
  }
  return bk_save_menu_view_details(v, group, width);
}
static int fail(char *e, const char *why) {
  snprintf(e, 256, "save menu view: %s", why);
  return 0;
}
static int append(BkSaveMenuFrame *f, unsigned slot, const BkPauseSprite *p) {
  if (f->count == BK_SAVE_MENU_DRAWS)
    return 0;
  BkPauseDraw *d = &f->draws[f->count++];
  d->slot = slot;
  d->alpha = p->fade.alpha;
  memcpy(d->rect, p->rect, 16);
  return 1;
}
static int advance(BkPauseSprite *p, int instant, float dt) {
  BkFadeSprite check = p->fade;
  if (!bk_fade_sprite_advance(&check, dt))
    return 0;
  if (!instant) {
    p->fade = check;
    return 1;
  }
  switch (p->fade.stage) {
  case 0:
    p->fade.alpha = 0;
    break;
  case 1:
    p->fade.alpha = 1;
    p->fade.stage = 2;
    break;
  case 2:
    p->fade.alpha = 1;
    p->fade.stage = 3;
    break;
  case 3:
    p->fade.alpha = 1;
    break;
  default:
    p->fade.alpha = 0;
    p->fade.stage = 0;
    break;
  }
  return 1;
}
static int draw(BkSaveMenuView *v, unsigned slot, float dt,
                BkSaveMenuFrame *f) {
  return advance(&v->sprites[slot], layout[slot].instant, dt) &&
         (!(v->loaded & (UINT64_C(1) << slot)) ||
          append(f, slot, &v->sprites[slot]));
}
static void request(BkSaveMenuView *v, unsigned slot, uint8_t wanted) {
  /*Native hover99 requests an unbound global sprite; no drawn image or live
   * resource is associated with it. Keep that stray write out of owned state.*/
  if (slot < 56)
    bk_fade_sprite_request(&v->sprites[slot].fade, wanted);
}
int bk_save_menu_view_step(BkSaveMenuView *v, BkSaveMenuControl *s,
                           const BkPauseBindings *b, const BkPauseInput *in,
                           unsigned mode, const BkSaveMenuRecord records[5][10],
                           const BkSaveMenuViewOps *ops, BkSaveMenuFrame *f,
                           char e[256]) {
  if (!v || !s || !b || !b->common || !b->cursor || !in || mode > 1 ||
      !records || !ops || !ops->text || !ops->motion || !f ||
      !isfinite(in->seconds) || in->seconds < 0 || !isfinite(in->scale) ||
      in->scale <= 0 || in->scale > 16)
    return fail(e, "invalid services/time");
  *f = (BkSaveMenuFrame){.text_after = UINT32_MAX};
  if (s->skip_draw == 1) {
    s->skip_draw = 0;
    return 1;
  }
  if (s->tab < 3 || s->tab > 7 || (s->page != 0 && s->page != 1) ||
      v->detail_slot < 0 || v->detail_slot > 9)
    return fail(e, "invalid page/index");
  for (unsigned i = 0; i < 58; ++i) {
    BkFadeSprite check = i < 56    ? v->sprites[i].fade
                         : i == 56 ? b->cursor->sprite.fade
                                   : b->common->curtain;
    if (!bk_fade_sprite_advance(&check, in->seconds))
      return fail(e, "invalid sprite state");
  }
  unsigned group = (unsigned)(s->tab - 3);
  int detail =
      s->hover >= 21 && s->hover <= 30 ? s->hover - 21 : v->detail_slot;
  const BkSaveMenuRecord *r = &records[group][detail];
  if (r->area > 8)
    return fail(e, "invalid record area");
  float dt = in->seconds;
  uint8_t confirm = (uint8_t)s->page;
#define DRAW(i)                                                                \
  do {                                                                         \
    if (!draw(v, (i), dt, f))                                                  \
      return fail(e, "sprite advance/snapshot failed");                        \
  } while (0)
  DRAW(mode ? 19 : 0);
  for (unsigned i = 3; i <= 7; ++i)
    request(v, i + 11, s->tab == (int)i);
  for (unsigned i = 14; i <= 18; ++i)
    DRAW(i);
  DRAW(41);
  if (s->hover >= 21 && s->hover <= 30) {
    v->sprites[8].rect[0] = (float)(438.0 * in->scale);
    v->sprites[8].rect[1] = (float)((228.0 + 54 * (s->hover - 21)) * in->scale);
    DRAW(8);
  } else if (s->hover == 1) {
    DRAW(2);
    request(v, 1, 0);
    request(v, 2, 1);
  } else {
    request(v, 1, 1);
    request(v, 2, 0);
  }
  v->detail_slot = detail;
  for (unsigned i = 0; i < 5; ++i)
    request(v, 42 + i, r->inventory[i] == 1);
  request(v, 40, r->occupied != 0);
  for (unsigned i = 42; i <= 46; ++i)
    DRAW(i);
  DRAW(40);
  if (r->occupied)
    request(v, 47 + r->area, 1);
  DRAW(47 + r->area);
  if (r->occupied)
    request(v, 31 + r->area, 1);
  DRAW(31 + r->area);
  DRAW((unsigned)s->tab);
  DRAW(1);
  if (!s->skip_draw) {
    f->text_after = f->count;
    if (!ops->text(ops->context, group, dt, e))
      return 0;
  }
  unsigned panel = mode ? 20 : 9;
  request(v, panel, confirm);
  DRAW(panel);
  for (unsigned i = 10; i <= 13; ++i)
    request(v, i, confirm);
  const unsigned yes_order[] = {10, 13, 11, 12}, no_order[] = {11, 12, 10, 13},
                 none_order[] = {11, 13, 10, 12};
  const unsigned *order = s->confirm_hover == 10   ? yes_order
                          : s->confirm_hover == 12 ? no_order
                                                   : none_order;
  for (unsigned i = 0; i < 4; ++i)
    DRAW(order[i]);
  request(v, (unsigned)s->hover, confirm);
  for (unsigned i = 21; i <= 30; ++i)
    DRAW(i);
  BkMenuCursor *c = b->cursor;
  memcpy(c->sprite.rect, s->cursor, 8);
  if (!advance(&c->sprite, 0, dt) ||
      !append(f, BK_SAVE_MENU_CURSOR, &c->sprite) ||
      !bk_fade_sprite_request(&c->sprite.fade, c->wanted))
    return fail(e, "invalid cursor");
  float motion[2];
  if (!ops->motion(ops->context, motion, e))
    return 0;
  if (!isfinite(motion[0]) || !isfinite(motion[1]))
    return fail(e, "invalid motion");
  if (motion[0] == 0 && motion[1] == 0) {
    if (bk_timer_poll(&c->idle, in->now_ms))
      c->wanted = 0;
  } else {
    c->wanted = 1;
    c->idle.armed = 0;
  }
  BkPauseSprite curtain = {
      .fade = b->common->curtain,
      .rect = {0, 0, (float)(1280.0 * in->scale), (float)(960.0 * in->scale)}};
  if (!advance(&curtain, 0, dt) || !append(f, BK_SAVE_MENU_CURTAIN, &curtain))
    return fail(e, "invalid curtain");
  b->common->curtain = curtain.fade;
  bk_fade_sprite_request(&b->common->curtain, b->common->blocked);
  if (s->skip_draw == 1)
    s->skip_draw = 0;
  return 1;
#undef DRAW
}
