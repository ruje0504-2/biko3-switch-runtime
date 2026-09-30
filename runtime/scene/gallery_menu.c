#include "scene/gallery_menu.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
static int fail(char e[256], const char *why) {
  snprintf(e, 256, "gallery menu: %s", why);
  return 0;
}
/*4C59C0 construction order, not numeric slot order.*/
static const unsigned order[48] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 32, 33, 34, 35, 36, 37, 31, 39, 40, 38, 41, 42, 43, 44, 45, 46, 47};
static const struct { const char *name; float rect[4]; } layout[48] = {
  {"ex_00.bmp", {0, 0, 1280, 960}},
  {"al_01.tga", {20, 134, 144, 144}},
  {"al_02.tga", {20, 134, 144, 144}},
  {"al_03.tga", {20, 284, 144, 144}},
  {"al_04.tga", {20, 284, 144, 144}},
  {"al_05.tga", {20, 434, 144, 144}},
  {"al_06.tga", {20, 434, 144, 144}},
  {"al_07.tga", {20, 584, 144, 144}},
  {"al_08.tga", {20, 584, 144, 144}},
  {"al_09.tga", {20, 734, 144, 144}},
  {"al_10.tga", {20, 734, 144, 144}},
  {"al_26.tga", {976, 880, 256, 64}},
  {"al_27.tga", {976, 880, 256, 64}},
  {"ex_01.tga", {528, 208, 168, 64}},
  {"ex_02.tga", {528, 208, 168, 64}},
  {"ex_03.tga", {528, 208, 168, 64}},
  {"ex_04.tga", {528, 272, 168, 64}},
  {"ex_05.tga", {528, 272, 168, 64}},
  {"ex_06.tga", {528, 272, 168, 64}},
  {"ex_07.tga", {528, 336, 168, 64}},
  {"ex_08.tga", {528, 336, 168, 64}},
  {"ex_09.tga", {528, 336, 168, 64}},
  {"ex_01.tga", {528, 515, 168, 64}},
  {"ex_02.tga", {528, 515, 168, 64}},
  {"ex_03.tga", {528, 515, 168, 64}},
  {"ex_10.tga", {528, 580, 168, 64}},
  {"ex_11.tga", {528, 580, 168, 64}},
  {"ex_12.tga", {528, 580, 168, 64}},
  {"ex_07.tga", {528, 640, 168, 64}},
  {"ex_08.tga", {528, 640, 168, 64}},
  {"ex_09.tga", {528, 640, 168, 64}},
  {"ex_23.tga", {224, 232, 320, 240}},
  {"ex_57.tga", {528, 400, 168, 64}},
  {"ex_58.tga", {528, 400, 168, 64}},
  {"ex_59.tga", {528, 400, 168, 64}},
  {"ex_57.tga", {528, 706, 168, 64}},
  {"ex_58.tga", {528, 706, 168, 64}},
  {"ex_59.tga", {528, 706, 168, 64}},
  {"ex_25.tga", {736, 208, 208, 160}},
  {"ex_13.tga", {224, 220, 320, 240}},
  {"ex_14.tga", {224, 520, 320, 240}},
  {"ex_30.tga", {976, 208, 208, 160}},
  {"ex_35.tga", {736, 408, 208, 160}},
  {"ex_40.tga", {976, 408, 208, 160}},
  {"ex_45.tga", {736, 608, 208, 160}},
  {"ex_50.tga", {976, 608, 208, 160}},
  {"ex_55.tga", {0, 0, 208, 160}},
  {"ex_56.tga", {0, 0, 208, 160}},
};
int bk_gallery_menu_name(char out[32], unsigned group, unsigned index,
                          unsigned mode) {
  static const unsigned numbers[8] = {30, 35, 40, 45, 50, 13, 14, 25};
  static const unsigned pictures[5] = {20, 10, 21, 30, 30};
  if (!out || group >= 5 || mode > 3 || (mode == 0 && index >= 8) ||
      (mode == 1 && index >= 5))
    return 0;
  if (mode == 0)
    snprintf(out, 32, "ex_%u.tga", numbers[index] + group * (index == 5 || index == 6 ? 2 : 1));
  else if (mode == 1)
    snprintf(out, 32, "g0%u_%02u.bmp", group + 1, pictures[index]);
  else
    snprintf(out, 32, "%s_%02u.bmp", mode == 2 ? "gl" : "gs", group + 1);
  return 1;
}
int bk_gallery_menu_layout(unsigned slot, unsigned group, float scale,
                            unsigned height, unsigned picture,
                            char name[32], float rect[4]) {
  if (!name || !rect || slot >= BK_GALLERY_SPRITES || group >= 5 ||
      !isfinite(scale) || scale <= 0 || scale > 16 || !height || height > 16384)
    return 0;
  if (slot == 48) {
    if (picture > 4 || !bk_gallery_menu_name(name, group, picture, picture == 4 ? 3 : 1))
      return 0;
    rect[0] = (picture == 4 ? 160.f : 0.f) * scale;
    rect[1] = 0;
    rect[2] = picture == 4 ? (float)height : 1280.f * scale;
    rect[3] = picture == 4 ? (float)height : 960.f * scale;
    return 1;
  }
  for (unsigned j = 0; j < 4; ++j)
    rect[j] = layout[slot].rect[j] * scale;
  if (slot >= 38 && slot <= 45) {
    unsigned index = slot == 38 ? 7 : slot == 39 ? 5 : slot == 40 ? 6 : slot - 41;
    return bk_gallery_menu_name(name, group, index, 0);
  }
  snprintf(name, 32, "%s", layout[slot].name);
  return 1;
}
static int load(BkGalleryMenu *s, unsigned slot, float scale, unsigned height,
                const BkGalleryMenuOps *o, char e[256]) {
  char name[32];
  BkPauseSprite p = {0};
  if (!bk_gallery_menu_layout(slot, (unsigned)s->group, scale, height,
                               (unsigned)s->picture, name, p.rect))
    return fail(e, "invalid image selection/geometry");
  if (!o->image(o->context, slot, name, e))
    return 0;
  bk_fade_sprite_initialize(&p.fade);
  s->sprites[slot] = p;
  s->loaded |= UINT64_C(1) << slot;
  return 1;
}
static int unload(BkGalleryMenu *s, unsigned slot,
                  const BkGalleryMenuOps *o, char e[256]) {
  if ((s->loaded & (UINT64_C(1) << slot)) &&
      !o->image(o->context, slot, NULL, e))
    return 0;
  s->loaded &= ~(UINT64_C(1) << slot);
  return 1;
}
int bk_gallery_menu_initialize(BkGalleryMenu *s, unsigned width,
                               uint8_t previous, unsigned ending_group,
                               unsigned game_group, const uint8_t u[5][8],
                               int32_t master, const BkGalleryMenuOps *o,
                               char e[256]) {
  unsigned group = previous == 0x10 ? ending_group : previous == 0x48 ? game_group : 0;
  if (!s || !u || !width || width > 16384 || group >= 5 ||
      master < -10000 || master > 0 || !o || !o->image)
    return fail(e, "invalid initializer/services");
  s->group = (int16_t)group;
  s->transition = -1;
  const unsigned pic[] = {2, 3, 5, 6, 7};
  for (unsigned g = 0; g < 5; ++g) {
    for (unsigned i = 0; i < 9; ++i)
      s->actions[g][i] = u[g][i == 8 ? 6 : i];
    for (unsigned i = 0; i < 5; ++i)
      s->pictures[g][i] = u[g][pic[i]];
  }
  float scale = (float)((double)width / 1280);
  for (unsigned i = 0; i < 48; ++i)
    if (!load(s, order[i], scale, width * 3 / 4, o, e))
      return 0;
  s->music_volume = master;
  memset(s->hover, 0, sizeof(s->hover)); /*46D9B4 allocates zeroed81 bytes*/
  return 1;
}
static int replace_group(BkGalleryMenu *s, unsigned group,
                         const BkGalleryMenuInput *in,
                         const BkGalleryMenuOps *o, char e[256]) {
  static const unsigned release_order[] = {48, 45, 44, 43, 42, 41, 38, 40, 39};
  static const unsigned load_order[] = {39, 40, 38, 41, 42, 43, 44, 45};
  s->group = (int16_t)group;
  for (unsigned i = 0; i < 9; ++i)
    if (!unload(s, release_order[i], o, e))
      return 0;
  for (unsigned i = 0; i < 8; ++i)
    if (!load(s, load_order[i], in->scale, in->height, o, e))
      return 0;
    else if (load_order[i] == 39)
      s->sprites[39].rect[1] = 232.f * in->scale; /*4C7916 differs from4C59C0*/
  return 1;
}
static int inside(const float r[4], const float p[2]) {
  return r[0] <= p[0] && (double)r[0] + r[2] >= p[0] &&
         r[1] <= p[1] && (double)r[1] + r[3] >= p[1];
}
static int hover(BkGalleryMenu *s, unsigned slot, unsigned latch, unsigned kind,
                  unsigned value, const BkGalleryMenuBindings *b,
                  const BkGalleryMenuInput *in, const BkGalleryMenuOps *o,
                  int *out, char e[256]) {
  *out = -1;
  if (!inside(s->sprites[slot].rect, s->pointer)) {
    s->hover[latch] = 0;
    return 1;
  }
  if ((in->buttons & BK_PAUSE_CONFIRM) &&
      (kind != 0 || s->group != (int16_t)value)) {
    if (!o->sound(o->context, 0, e))
      return 0;
    if (kind == 0) {
      if (!replace_group(s, value, in, o, e))
        return 0;
    } else {
      b->common->blocked = 1;
      if (kind == 1) {
        s->requested_action = (int16_t)value;
        s->transition = 1;
      } else if (kind == 2) {
        s->picture = (int16_t)value;
        s->transition = 2;
      } else {
        s->requested_action = -1;
        s->transition = 0;
      }
    }
  }
  if (s->hover[latch] == 0) {
    if (!o->sound(o->context, kind == 0 ? 4 : 3, e))
      return 0;
    s->hover[latch] = 1;
  }
  *out = (int)slot;
  return 1;
}
static int control(BkGalleryMenu *s, const BkGalleryMenuBindings *b,
                    const BkGalleryMenuInput *in, const BkGalleryMenuOps *o,
                    int *out, char e[256]) {
  for (unsigned g = 0; g < 5; ++g) {
    if (!hover(s, 2 + g * 2, g, 0, g, b, in, o, out, e))
      return 0;
    if (*out != -1)
      return 1;
  }
  if (!hover(s, 12, 19, 3, 0, b, in, o, out, e))
    return 0;
  if (*out != -1)
    return 1;
  static const unsigned slots[] = {14, 17, 20, 23, 26, 29, 33, 36, 38};
  for (unsigned i = 0; i < 9; ++i) {
    if (!s->actions[s->group][i])
      continue; /*Native skips the latch reset for disabled entries.*/
    unsigned action = (slots[i] - 14) / 3;
    if (!hover(s, slots[i], action + 5, 1, action, b, in, o, out, e))
      return 0;
    if (*out != -1)
      return 1;
  }
  for (unsigned i = 0; i < 5; ++i) {
    if (!s->pictures[s->group][i])
      continue;
    if (!hover(s, 41 + i, 14 + i, 2, i, b, in, o, out, e))
      return 0;
    if (*out != -1)
      return 1;
  }
  *out = -1;
  return 1;
}
static int flat(BkGalleryMenuFrame *f, unsigned slot, BkPauseSprite *p,
                 float dt, int loaded, char e[256]) {
  if (!bk_fade_sprite_advance(&p->fade, dt))
    return fail(e, "invalid sprite transition");
  if (!loaded)
    return 1;
  if (f->count >= BK_GALLERY_DRAWS)
    return fail(e, "draw capacity exceeded");
  BkGalleryMenuDraw *d = &f->draws[f->count++];
  *d = (BkGalleryMenuDraw){slot, {p->rect[0], p->rect[1],
      p->rect[0] + p->rect[2], p->rect[1] + p->rect[3]}, p->fade.alpha};
  return 1;
}
static int draw(BkGalleryMenu *s, BkGalleryMenuFrame *f, unsigned slot,
                float dt, char e[256]) {
  return flat(f, slot, &s->sprites[slot], dt,
              !!(s->loaded & (UINT64_C(1) << slot)), e);
}
static int move_draw(BkGalleryMenu *s, BkGalleryMenuFrame *f, unsigned slot,
                     float x, float y, float dt, char e[256]) {
  s->sprites[slot].rect[0] = x;
  s->sprites[slot].rect[1] = y;
  return draw(s, f, slot, dt, e);
}
int bk_gallery_menu_step(BkGalleryMenu *s, const BkGalleryMenuBindings *b,
                         const BkGalleryMenuInput *in, const BkGalleryMenuOps *o,
                         BkGalleryMenuFrame *f, char e[256]) {
  if (!s || !b || !b->common || !b->cursor || !in || !o || !f ||
      !o->sound || !o->image || !o->position || s->group < 0 || s->group >= 5 ||
      !isfinite(in->seconds) || in->seconds < 0 || !isfinite(in->scale) ||
      in->scale <= 0 || in->scale > 16 || !in->height || in->height > 16384 ||
      !isfinite(s->pointer[0]) || !isfinite(s->pointer[1]))
    return fail(e, "invalid state/input/services");
  *f = (BkGalleryMenuFrame){.action = 99};
  if (b->common->blocked == 1 && b->common->curtain.stage == 3) {
    if (s->transition == 0 || s->transition == 1) {
      b->common->blocked = 0;
      f->action = s->transition == 0 ? 10 : s->requested_action;
      return 1;
    }
    if (s->transition == 2) {
      if (!load(s, 48, in->scale, in->height, o, e) ||
          !bk_fade_sprite_request(&s->sprites[48].fade, 1))
        return 0;
      s->image_view = 1;
      b->common->blocked = 0;
    } else {
      b->common->blocked = 0;
      s->image_view = s->view_gate = 0;
    }
  }
  float dt = in->seconds, scale = in->scale;
  if (s->image_view == 0) {
    int h;
    if (!control(s, b, in, o, &h, e))
      return 0;
    for (unsigned i = 0; i < 48; ++i)
      if (!bk_fade_sprite_request(&s->sprites[i].fade, 1))
        return fail(e, "invalid sprite request");
    if (!draw(s, f, 0, dt, e))
      return 0;
    for (unsigned i = 1; i < 12; i += 2)
      if (!draw(s, f, i, dt, e))
        return 0;
    for (unsigned i = 0; i < 5; ++i)
      if ((h == (int)(2 + i * 2) || s->group == (int)i) &&
          !draw(s, f, 2 + i * 2, dt, e))
        return 0;
    if (h == 12 && !draw(s, f, 12, dt, e))
      return 0;
    static const unsigned rows[] = {13, 16, 19, 22, 25, 28, 32, 35};
    const int32_t *a = s->actions[s->group], *p = s->pictures[s->group];
    for (unsigned i = 0; i < 8; ++i)
      if (!draw(s, f, rows[i] + (!a[i] ? 2 : h == (int)(rows[i] + 1) ? 1 : 0), dt, e))
        return 0;
    int locked[2] = {!(a[0] || a[1] || a[2]), !(a[3] || a[4] || a[5])};
    for (unsigned i = 0; i < 2; ++i)
      if (!locked[i] && !draw(s, f, 39 + i, dt, e))
        return 0;
    for (unsigned i = 0; i < 2; ++i)
      if (locked[i] && !move_draw(s, f, 31, 224.f * scale, (232.f + i * 288.f) * scale, dt, e))
        return 0;
    if (a[8]) {
      if (!draw(s, f, 38, dt, e))
        return 0;
    } else if (!move_draw(s, f, 46, 736.f * scale, 208.f * scale, dt, e))
      return 0;
    for (unsigned i = 0; i < 5; ++i)
      if (p[i] && !draw(s, f, 41 + i, dt, e))
        return 0;
    for (unsigned i = 1; i < 6; ++i)
      if (!p[i - 1] && !move_draw(s, f, 46, (736.f + (i % 2) * 240.f) * scale,
                                  (208.f + (i / 2) * 200.f) * scale, dt, e))
        return 0;
    for (unsigned i = 0; i < 6; ++i) {
      /*Original hit expressions retain x87 precision across base*scale+size.*/
      double x = (736.0 + (i % 2) * 240.0) * scale;
      double y = (208.0 + (i / 2) * 200.0) * scale;
      if ((i ? p[i - 1] : a[8]) && x <= s->pointer[0] && y <= s->pointer[1] &&
          x + s->sprites[46].rect[2] >= s->pointer[0] &&
          y + s->sprites[46].rect[3] >= s->pointer[1] &&
          !move_draw(s, f, 47, (float)x, (float)y, dt, e))
        return 0;
    }
  } else {
    if (!bk_fade_sprite_request(&s->sprites[48].fade, 1) || !draw(s, f, 48, dt, e))
      return 0;
    if (in->buttons & BK_GALLERY_BACK) {
      s->view_gate = 0;
      if (!o->sound(o->context, 2, e) || !unload(s, 48, o, e))
        return 0;
      b->common->blocked = 1;
      s->transition = 3;
    }
  }
  if (!o->position(o->context, s->pointer, e))
    return 0;
  if (!isfinite(s->pointer[0]) || !isfinite(s->pointer[1]))
    return fail(e, "invalid pointer sensor");
  b->cursor->sprite.rect[0] = s->pointer[0];
  b->cursor->sprite.rect[1] = s->pointer[1];
  if (!flat(f, BK_GALLERY_CURSOR, &b->cursor->sprite, dt, 1, e) ||
      !bk_fade_sprite_request(&b->cursor->sprite.fade, b->cursor->wanted))
    return 0;
  BkPauseSprite curtain = {b->common->curtain, {0, 0, 1280.f * scale, 960.f * scale}};
  if (!flat(f, BK_GALLERY_CURTAIN, &curtain, dt, 1, e))
    return 0;
  b->common->curtain = curtain.fade;
  return bk_fade_sprite_request(&b->common->curtain, b->common->blocked);
}
int bk_gallery_menu_dispatch(const BkGalleryMenu *s, int32_t action,
                             BkGalleryMenuSelection *selection,
                             BkFlowTransition *flow,
                             const BkGalleryDispatchOps *o, char e[256]) {
  if (!s || !selection || !flow || !o || !o->release || !o->story ||
      s->group < 0 || s->group >= 5)
    return fail(e, "invalid dispatch state/services");
  unsigned target, mode;
  if (action >= 0 && action <= 7) {
    *selection = (BkGalleryMenuSelection){s->group,
      (action >= 3 && action <= 5) || action == 7, action >= 6 ? 6 : action};
    target = 0x10;
    mode = 1;
  } else if (action == 8) {
    if (!o->story(o->context, (unsigned)s->group, e))
      return 0;
    target = 0x48;
    mode = 1;
  } else if (action == 10) {
    target = 1;
    mode = 0;
  } else
    return 1;
  BkFlowTransitionOps ops = {o->context, o->release};
  return bk_flow_transition_schedule(flow, &ops, (uint8_t)target, (uint8_t)mode, e);
}
