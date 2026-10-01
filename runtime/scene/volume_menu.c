#include "scene/volume_menu.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
static int fail(char e[256], const char *why) {
  snprintf(e, 256, "volume menu: %s", why); return 0;
}
static int valid(const BkVolumeMenu *s) {
  if (!s || !isfinite(s->minimum) || !isfinite(s->maximum) ||
      s->maximum <= s->minimum || s->row < 0 || s->row >= 6 ||
      s->selected < 0 || s->selected >= 6) return 0;
  for (unsigned i = 0; i < 3; ++i)
    if (!isfinite(s->slider[i]) || s->slider[i] < s->minimum ||
        s->slider[i] > s->maximum || s->samples[i] < -1 || s->samples[i] >= 16)
      return 0;
  return 1;
}
const char *bk_volume_menu_image(unsigned slot) {
  static const char *const names[] = {
    "vc_00.bmp", "vc_03.tga", "vc_03.tga", "vc_03.tga",
    "vc_01.tga", "vc_01.tga", "vc_01.tga", "vc_02.tga", "vc_02.tga", "vc_02.tga",
    "vc_04.tga", "vc_05.tga", "vc_04.tga", "vc_05.tga", "vc_04.tga", "vc_05.tga",
    "vc_06.tga", "vc_07.tga", "vc_08.tga", "vc_09.tga", "vc_99.bmp", "vc_99.bmp",
    "ma_00.tga"};
  return slot < BK_VOLUME_SPRITES ? names[slot] : NULL;
}
const char *bk_volume_menu_sound(unsigned slot) {
  switch (slot) {
  case 0: return "se123.wav";
  case 3: return "bg000.wav";
  case 6: return "se128.wav";
  case 9: return "se003.wav";
  case 10: return "se002.wav";
  case 11: return "se000.wav";
  default: return NULL;
  }
}
static void positions(BkVolumeMenu *s) {
  float span = (float)((double)s->maximum - s->minimum);
  for (unsigned i = 0; i < 3; ++i)
    s->slider[i] = (float)((double)s->maximum - (double)s->values[i] / -6000 * span);
}
int bk_volume_menu_initialize(BkVolumeMenu *s, unsigned width,
                               const int32_t values[3], char e[256]) {
  if (!s || !values || width < 4 || width > 16384) return fail(e, "invalid extent/values");
  for (unsigned i = 0; i < 3; ++i)
    if (values[i] < -6000 || values[i] > 0) return fail(e, "volume outside original slider range");
  static const int rects[BK_VOLUME_SPRITES][4] = {
    {0,0,1280,960}, {324,333,625,15}, {324,477,625,15}, {324,624,625,15},
    {0,0,40,40}, {0,0,40,40}, {0,0,40,40}, {0,0,40,40}, {0,0,40,40}, {0,0,40,40},
    {928,259,128,38}, {928,259,128,38}, {928,405,128,38}, {928,405,128,38},
    {928,552,128,38}, {928,552,128,38}, {696,880,256,56}, {696,880,256,56},
    {976,880,256,56}, {976,880,256,56}, {792,620,148,40}, {792,620,148,40}, {0,0,64,64}};
  BkVolumeMenu next = {0};
  float scale = (float)((double)width / 1280);
  for (unsigned i = 0; i < BK_VOLUME_SPRITES; ++i)
    for (unsigned j = 0; j < 4; ++j)
      next.rect[i][j] = (float)(int32_t)((double)rects[i][j] * scale);
  for (unsigned i = 0; i < 3; ++i) {
    next.bar_rect[i][0] = (int32_t)(325. * scale);
    next.bar_rect[i][1] = (int32_t)((308. + 145 * i) * scale);
    next.bar_rect[i][2] = (int32_t)(629. * scale);
    next.bar_rect[i][3] = (int32_t)(47. * scale);
    next.samples[i] = -1;
  }
  next.minimum = next.rect[1][0];
  next.maximum = (float)((double)next.minimum + next.rect[1][2]);
  memcpy(next.values, values, sizeof(next.values));
  next.previewing = -1; next.disabled[5] = 1;
  next.sound_mask = BK_VOLUME_SOUND_MASK;
  positions(&next); *s = next; return 1;
}
static void draw(BkVolumeMenuFrame *f, unsigned slot, const float q[4], float px, float py) {
  BkVolumeMenuDraw *d = &f->draws[f->count++];
  d->slot = slot;
  d->corners[0] = (float)((double)q[0] - (double)q[2] * px);
  d->corners[1] = (float)((double)q[1] - (double)q[3] * py);
  d->corners[2] = (float)((double)d->corners[0] + q[2]);
  d->corners[3] = (float)((double)d->corners[1] + q[3]);
}
int bk_volume_menu_prepare(const BkVolumeMenu *s, BkVolumeMenuFrame *f, char e[256]) {
  if (!valid(s) || !f) return fail(e, "invalid draw state");
  *f = (BkVolumeMenuFrame){0};
  for (unsigned i = 0; i < BK_VOLUME_SPRITES; ++i) {
    float q[4]; memcpy(q, s->rect[i], sizeof(q));
    float px = 0, py = 0;
    if (i >= 1 && i <= 3) {
      float fraction = (float)(((double)s->slider[i-1] - s->minimum) /
                                ((double)s->maximum - s->minimum));
      q[2] = (float)((double)q[2] * fraction); py = .5f;
    } else if (i >= 4 && i <= 9) {
      unsigned row = (i - 4) % 3;
      if ((i >= 7) != (s->previewing == (int32_t)row)) continue;
      q[0] = s->slider[row]; q[1] = s->rect[row+1][1]; px = py = .5f;
    } else if (i >= 10 && i <= 21) {
      unsigned row = (i-10)/2;
      if (s->disabled[row] || ((i & 1) != (s->row == (int32_t)row))) continue;
    } else if (i == 22) { q[0] = (float)s->cursor[0]; q[1] = (float)s->cursor[1]; }
    draw(f, i, q, px, py);
  }
  f->draws[f->count] = f->draws[f->count-1]; ++f->count;
  return 1;
}
static int contains(const int32_t q[4], int32_t x, int32_t y) {
  return x >= q[0] && (int64_t)x < (int64_t)q[0]+q[2] &&
         y >= q[1] && (int64_t)y < (int64_t)q[1]+q[3];
}
static int play(BkVolumeMenu *s, const BkVolumeMenuOps *o, unsigned slot,
                 int32_t volume, char e[256]) {
  return !(s->sound_mask & (1u << slot)) || o->play(o->context, slot, volume, e);
}
static int gain(BkVolumeMenu *s, const BkVolumeMenuOps *o, char e[256]) {
  for (unsigned i = 0; i < 3; ++i)
    s->values[i] = -(int32_t)((1. - ((double)s->slider[i] - s->minimum) /
                                    ((double)s->maximum - s->minimum)) * 6000.);
  for (unsigned i = 0; i < 3; ++i) {
    int active;
    if (s->samples[i] >= 0) {
      if (!o->gain(o->context, (unsigned)s->samples[i], s->values[i], e) ||
          !o->playing(o->context, (unsigned)s->samples[i], &active, e)) return 0;
      if (active) s->previewing = (int32_t)i;
    }
  }
  return 1;
}
int bk_volume_menu_step(BkVolumeMenu *s, const BkVolumeMenuInput *in,
                         const BkVolumeMenuOps *o, int *action, char e[256]) {
  if (!valid(s) || !in || !o || !o->play || !o->stop || !o->gain ||
      !o->playing || !o->random || !o->warp || !action) return fail(e, "invalid input/services");
  *action = -1; s->previewing = -1;
  s->cursor[0] = in->x; s->cursor[1] = in->y;
  if (!(in->left || in->right || in->up || in->down)) {
    if (!(s->dragging[0] || s->dragging[1] || s->dragging[2])) {
      s->mouse_result = -1;
      for (unsigned i = 0; i < 6; ++i) {
        if (s->disabled[i] == 1) continue;
        int32_t q[4];
        for (unsigned j = 0; j < 4; ++j) q[j] = (int32_t)s->rect[10+2*i][j];
        if (!contains(q, in->x, in->y)) continue;
        if (in->mouse == 3) {
          s->selected = (int32_t)i;
          s->mouse_result = s->row == (int32_t)i ? 2 : 3;
        } else s->mouse_result = s->row == (int32_t)i ? 0 : 1;
        s->row = (int32_t)i; break;
      }
      if (s->mouse_result == 1 && !play(s, o, 9, s->values[2], e)) return 0;
    }
    if (!(in->mouse & 1)) memset(s->dragging, 0, sizeof(s->dragging));
    for (unsigned i = 0; i < 3; ++i) {
      float dx = (float)((double)in->x - (int32_t)s->slider[i]);
      float dy = (float)((double)in->y - (int32_t)s->rect[i+1][1]);
      float distance = (float)sqrt((double)dx*dx + (double)dy*dy);
      int hit = (int32_t)distance < (int32_t)(s->rect[i+4][2]/2);
      if (!hit) hit = contains(s->bar_rect[i], in->x, in->y);
      if (hit && in->mouse == 3) { s->dragging[i] = 1; break; }
    }
    for (unsigned i = 0; i < 3; ++i)
      if (s->dragging[i] && in->mouse == 1) {
        s->slider[i] = (float)in->x;
        if (s->slider[i] > s->maximum) s->slider[i] = s->maximum;
        else if (s->slider[i] < s->minimum) s->slider[i] = s->minimum;
        break;
      }
  } else {
    int changed = 0;
    if (s->row < 3) {
      if (in->up == 3) { if (--s->row < 0) s->row = 0; changed = 1; }
      else if (in->down == 3) { if (++s->row > 2) s->row = 4; changed = 1; }
      else if (in->left & 1) {
        s->slider[s->row] = (float)((double)s->slider[s->row] - ((in->fast & 1) ? 5 : 2));
        if (s->slider[s->row] < s->minimum) s->slider[s->row] = s->minimum;
      } else if (in->right & 1) {
        s->slider[s->row] = (float)((double)s->slider[s->row] + ((in->fast & 1) ? 5 : 2));
        if (s->slider[s->row] > s->maximum) s->slider[s->row] = s->maximum;
      }
    } else if (in->up == 3) { s->row = 2; changed = 1; }
    else if (in->left == 3) { if (--s->row < 3) s->row = 3; changed = 1; }
    else if (in->right == 3) { if (++s->row > 5) s->row = 5; changed = 1; }
    if (changed && !play(s, o, 9, s->values[2], e)) return 0;
    const float *q = s->rect[10+2*s->row];
    if (!o->warp(o->context, (int32_t)((double)q[0] + (double)q[2]/2),
                              (int32_t)((double)q[1] + (double)q[3]/2), e)) return 0;
  }
  if (!gain(s, o, e)) return 0;
  if (s->mouse_result != 2 && s->mouse_result != 3 && in->confirm != 3) return 1;
  if (in->confirm == 3) s->selected = s->row;
  for (unsigned i = 0; i < 16; ++i)
    if ((s->sound_mask & (1u << i)) && !o->stop(o->context, i, e)) return 0;
  unsigned slot;
  if (s->selected < 3) {
    uint32_t random;
    if (!o->random(o->context, &random, e)) return 0;
    slot = 3*(unsigned)s->selected + random % (s->selected == 2 ? 2u : 1u);
    s->samples[s->selected] = (s->sound_mask & (1u << slot)) ? (int32_t)slot : -1;
  } else slot = s->selected == 3 ? 11 : 10;
  if (!play(s, o, slot, s->values[s->selected < 3 ? s->selected : 2], e)) return 0;
  if (s->selected == 3) {
    s->values[0] = -1500; s->values[1] = -2500; s->values[2] = -2000;
    positions(s);
  }
  *action = s->selected; return 1;
}
