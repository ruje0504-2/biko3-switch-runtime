#include "scene/ending_ui.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
static int fail(char e[256], const char *why) {
  snprintf(e, 256, "ending UI: %s", why);
  return 0;
}
typedef struct {
  const char *name;
  float rect[4], speed, pivot;
  uint8_t enter, exit, idle, stage;
} Layout;
/* Original4ccabd..4ce7a9 constructor arguments and explicit setters. */
static const Layout layout[BK_ENDING_UI_SPRITES] = {
    [0] = {"ma_00.tga", {320, 240, 64, 64}, 5, 0, 1, 1, 0, 1},
    [1] = {"hs_09.tga", {320, 240, 96, 96}, 5, 0, 1, 1, 0, 0},
    [2] = {"hs_08.tga", {320, 240, 96, 96}, 5, 0, 1, 1, 0, 0},
    [3] = {"hs_06.tga", {320, 240, 96, 96}, 5, 0.5, 1, 1, 0, 0},
    [4] = {"hs_03.tga", {320, 240, 96, 96}, 5, 0.5, 1, 1, 0, 0},
    [5] = {"hs_04.tga", {320, 240, 96, 96}, 5, 0.5, 1, 1, 0, 0},
    [6] = {"hs_07.tga", {320, 240, 96, 96}, 5, 0.5, 1, 1, 0, 0},
    [7] = {"hs_05.tga", {320, 240, 96, 96}, 5, 0.5, 1, 1, 0, 0},
    [9] = {"hs_00.bmp", {26, 251, 35, 200}, 2, 0, 1, 1, 0, 3},
    [10] = {"hs_01.tga", {15, 15, 56, 248}, 2, 0, 1, 1, 0, 1},
    [11] = {"hs_02.tga", {15, 15, 56, 248}, 2, 0, 1, 1, 0, 1},
    [12] = {"hs_12.tga", {1308, 80, 160, 40}, 2, 0, 1, 1, 0, 1},
    [13] = {"hs_11.tga", {1308, 80, 160, 40}, 2, 0, 1, 1, 0, 1},
    [14] = {"hs_13.tga", {1308, 36, 160, 48}, 2, 0, 1, 1, 0, 1},
    [15] = {"hs_14.tga", {1308, 36, 160, 48}, 2, 0, 1, 1, 0, 1},
    [16] = {"hs_15.tga", {1308, 36, 160, 48}, 2, 0, 1, 1, 0, 1},
    [17] = {"hs_31.tga", {1308, 362, 160, 40}, 2, 0, 1, 1, 0, 1},
    [18] = {"hs_30.tga", {1308, 362, 160, 40}, 2, 0, 1, 1, 0, 1},
    [19] = {"hs_33.tga", {1308, 400, 160, 40}, 2, 0, 1, 1, 0, 1},
    [20] = {"hs_32.tga", {1308, 400, 160, 40}, 2, 0, 1, 1, 0, 1},
    [21] = {"hs_29.tga", {1308, 324, 160, 40}, 2, 0, 1, 1, 0, 1},
    [22] = {"hs_28.tga", {1308, 324, 160, 40}, 2, 0, 1, 1, 0, 1},
    [23] = {"hs_35.tga", {1308, 485, 160, 40}, 2, 0, 1, 1, 0, 1},
    [24] = {"hs_34.tga", {1308, 485, 160, 40}, 2, 0, 1, 1, 0, 1},
    [25] = {"hs_37.tga", {1308, 520, 160, 40}, 2, 0, 1, 1, 0, 1},
    [26] = {"hs_36.tga", {1308, 520, 160, 40}, 2, 0, 1, 1, 0, 1},
    [27] = {"hs_39.tga", {1308, 558, 160, 40}, 2, 0, 1, 1, 0, 1},
    [28] = {"hs_38.tga", {1308, 558, 160, 40}, 2, 0, 1, 1, 0, 1},
    [29] = {"hs_17.tga", {1308, 158, 160, 40}, 2, 0, 1, 1, 0, 1},
    [30] = {"hs_16.tga", {1308, 158, 160, 40}, 2, 0, 1, 1, 0, 1},
    [31] = {"hs_18.tga", {1308, 118, 160, 48}, 2, 0, 1, 1, 0, 1},
    [32] = {"hs_19.tga", {1308, 118, 160, 48}, 2, 0, 1, 1, 0, 1},
    [33] = {"hs_20.tga", {1308, 118, 160, 48}, 2, 0, 1, 1, 0, 1},
    [34] = {"hs_43.tga", {1308, 632, 160, 40}, 2, 0, 1, 1, 0, 1},
    [35] = {"hs_42.tga", {1308, 632, 160, 40}, 2, 0, 1, 1, 0, 1},
    [36] = {"hs_41.tga", {1308, 596, 160, 40}, 2, 0, 1, 1, 0, 1},
    [37] = {"hs_40.tga", {1308, 596, 160, 40}, 2, 0, 1, 1, 0, 1},
    [38] = {"hs_22.tga", {1308, 240, 160, 40}, 2, 0, 1, 1, 0, 1},
    [39] = {"hs_21.tga", {1308, 240, 160, 40}, 2, 0, 1, 1, 0, 1},
    [40] = {"hs_23.tga", {1308, 240, 160, 40}, 2, 0, 1, 1, 0, 1},
    [41] = {"hs_24.tga", {1308, 197, 160, 48}, 2, 0, 1, 1, 0, 1},
    [42] = {"hs_25.tga", {1308, 197, 160, 48}, 2, 0, 1, 1, 0, 1},
    [43] = {"hs_26.tga", {1308, 197, 160, 48}, 2, 0, 1, 1, 0, 1},
    [44] = {"hs_27.tga", {1308, 197, 160, 48}, 2, 0, 1, 1, 0, 1},
    [45] = {"hs_45.tga", {1308, 720, 160, 40}, 2, 0, 1, 1, 0, 1},
    [46] = {"hs_44.tga", {1308, 720, 160, 40}, 2, 0, 1, 1, 0, 1},
    [47] = {"hs_47.tga", {1308, 756, 160, 40}, 2, 0, 1, 1, 0, 1},
    [48] = {"hs_46.tga", {1308, 756, 160, 40}, 2, 0, 1, 1, 0, 1},
    [49] = {"hs_10.tga", {1300, 0, 184, 816}, 2, 0, 1, 1, 0, 1},
    [50] = {"hs_48.tga", {320, 240, 120, 120}, 2, 0.5, 1, 1, 0, 1},
    [51] = {"hs_49.tga", {320, 240, 120, 120}, 2, 0.5, 1, 1, 0, 1},
    [52] = {"hs_100.bmp", {0, 0, 1280, 960}, 2, 0, 1, 1, 0, 1},
    [53] = {"hs_101.tga", {500, 320, 256, 256}, 2, 0.5, 2, 3, 5, 0},
    [54] = {"hs_102.tga", {500, 320, 256, 256}, 2, 0.5, 2, 3, 5, 0},
    [55] = {"hs_103.tga", {640, 480, 256, 256}, 2, 0.5, 3, 2, 5, 0},
    [56] = {"hs_104.tga", {640, 480, 256, 256}, 2, 0.5, 3, 2, 3, 0},
    [57] = {"sy_11.tga", {320, 344, 640, 280}, 2, 0, 1, 1, 0, 0},
    [58] = {"sy_12.tga", {320, 344, 640, 280}, 2, 0, 1, 1, 0, 0},
    [59] = {"lo_15.tga", {368, 502, 256, 56}, 2, 0, 1, 1, 0, 0},
    [60] = {"lo_16.tga", {368, 502, 256, 56}, 2, 0, 1, 1, 0, 0},
    [61] = {"lo_17.tga", {656, 502, 256, 56}, 2, 0, 1, 1, 0, 0},
    [62] = {"lo_18.tga", {656, 502, 256, 56}, 2, 0, 1, 1, 0, 0},
};
const char *bk_ending_ui_image(unsigned slot) {
  return slot < BK_ENDING_UI_SPRITES ? layout[slot].name : NULL;
}
int bk_ending_ui_sprite_step(BkEndingUi *s, unsigned slot, float seconds,
                             BkEndingUiDraw *out, char e[256]) {
  if (!s || !out || slot >= BK_ENDING_UI_SPRITES ||
      !(s->loaded & (UINT64_C(1) << slot)))
    return fail(e, "invalid or unconstructed sprite");
  return bk_ending_ui_element_step(&s->sprites[slot], slot, seconds, out, e);
}
int bk_ending_ui_element_step(BkEndingUiSprite *sprite, unsigned slot,
                              float seconds, BkEndingUiDraw *out, char e[256]) {
  if (!sprite || !out)
    return fail(e, "invalid sprite output");
  BkEndingUiSprite p = *sprite;
  BkEndingUiDraw d = {.slot = slot, .rgb = p.rgb};
  for (unsigned i = 0; i < 4; ++i)
    if (!isfinite(p.uv[i]))
      return fail(e, "invalid sprite UV");
  if (!bk_effect_sprite_advance(&p.transform, p.rect, seconds) ||
      !bk_effect_sprite_quad(&p.transform, p.rect, d.xy))
    return fail(e, "invalid sprite animation");
  d.alpha = p.transform.fade.alpha;
  memcpy(d.uv, p.uv, sizeof(d.uv));
  *sprite = p;
  *out = d;
  return 1;
}
int bk_ending_ui_initialize(BkEndingUi *s, unsigned width, uint8_t flags[6],
                            float *gauge, char e[256]) {
  if (!s || !flags || !gauge || !width || width > 16384)
    return fail(e, "invalid layout initializer");
  float scale = (float)((double)width / 1280);
  BkEndingUi next = *s;
  for (unsigned i = 0; i < BK_ENDING_UI_SPRITES; ++i) {
    const Layout *l = &layout[i];
    if (!l->name)
      continue;
    BkEndingUiSprite *p = &next.sprites[i];
    for (unsigned j = 0; j < 4; ++j)
      p->rect[j] = l->rect[j] * scale;
    if (!bk_effect_sprite_initialize(&p->transform, p->rect, l->enter, l->exit,
                                     l->idle))
      return fail(e, "invalid sprite constructor");
    p->transform.fade.speed = l->speed;
    p->transform.fade.stage = l->stage;
    p->transform.pivot[0] = p->transform.pivot[1] = l->pivot;
    /*4ce447/4ce4e3 explicit post-constructor scale. */
    if (i == 55 || i == 56)
      p->transform.scale[0] = p->transform.scale[1] = 5;
    memcpy(p->uv, (float[4]){0, 0, 1, 1}, sizeof(p->uv));
    p->rgb = 0xffffff;
    next.loaded |= UINT64_C(1) << i;
  }
  *s = next;
  memset(flags, 0, 6);
  *gauge = next.sprites[9].rect[1];
  return 1;
}
int bk_ending_ui_control_rects(const BkEndingUi *s, BkEndingControlRect r[13]) {
  static const unsigned slots[13] = {12, 29, 38, 21, 17, 19, 23,
                                     25, 27, 36, 34, 45, 47};
  if (!s || !r)
    return 0;
  for (unsigned i = 0; i < 13; ++i) {
    if (!(s->loaded & (UINT64_C(1) << slots[i])))
      return 0;
    for (unsigned j = 0; j < 4; ++j)
      if (!isfinite(s->sprites[slots[i]].rect[j]))
        return 0;
  }
  for (unsigned i = 0; i < 13; ++i) {
    const float *p = s->sprites[slots[i]].rect;
    r[i] = (BkEndingControlRect){p[0], p[1], p[2], p[3]};
  }
  return 1;
}
static float slide(float x, double target, float dt) {
  float delta = (float)(target - x);
  delta = (float)(3.0 * dt * delta);
  return (float)((double)x + delta);
}
int bk_ending_ui_hover(BkEndingUi *s, int32_t *open, const int32_t *request,
                       const float pointer[2], float scale, float dt,
                       const BkEndingUiHoverOps *ops, uint8_t *visible,
                       char e[256]) {
  if (!s || !open || !request || !pointer || !visible || !isfinite(scale) ||
      scale <= 0 || scale > 16 || !isfinite(dt) || dt < 0 ||
      !isfinite(pointer[0]) || !isfinite(pointer[1]))
    return fail(e, "invalid hover input");
  for (unsigned i = 12; i <= 49; ++i) {
    if (!(s->loaded & (UINT64_C(1) << i)))
      return fail(e, "toolbar sprite not constructed");
    for (unsigned j = 0; j < 4; ++j)
      if (!isfinite(s->sprites[i].rect[j]))
        return fail(e, "invalid toolbar rectangle");
  }
  const float *p = s->sprites[49].rect;
  uint8_t show = 0;
  if (pointer[0] >= 1096.0 * scale && pointer[0] <= 1280.0 * scale &&
      pointer[1] >= p[1] && pointer[1] <= (double)p[1] + p[3]) {
    if (!ops || !ops->key)
      return fail(e, "missing hover key service");
    uint32_t key = 0;
    if (!ops->key(ops->context, 0, 2, &key, e))
      return 0;
    if (!(key & 255) && !ops->key(ops->context, 1, 2, &key, e))
      return 0;
    show = (key & 255) || !*request ? *open != 0 : 1;
  }
  float xs[38];
  for (unsigned i = 12; i <= 49; ++i) {
    double target = i == 49 ? (show ? 1096 : 1300) : (show ? 1104 : 1308);
    xs[i - 12] = slide(s->sprites[i].rect[0], target * scale, dt);
    if (!isfinite(xs[i - 12]))
      return fail(e, "toolbar motion overflow");
  }
  for (unsigned i = 12; i <= 49; ++i)
    s->sprites[i].rect[0] = xs[i - 12];
  if (show && xs[37] <= 1280.0 * scale)
    *open = 1;
  else if (!show && xs[37] >= 1280.0 * scale)
    *open = 0;
  *visible = show;
  return 1;
}
