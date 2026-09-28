#include "game/ending_control.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
static int fail(char e[256], const char *why) {
  snprintf(e, 256, "ending control: %s", why);
  return 0;
}
static int pressed(const BkEndingControlOps *o, unsigned mode, int *yes,
                   char e[256]) {
  if (!o->key)
    return fail(e, "missing key service");
  static const unsigned codes[3] = {0, 0x5a, 0x33450};
  *yes = 0;
  for (unsigned i = 0; i < 3; ++i) {
    uint32_t result;
    if (!o->key(o->context, codes[i], mode, &result, e))
      return 0;
    if (result & 255) {
      *yes = 1;
      break;
    }
  }
  return 1;
}
static int sound(const BkEndingControlBindings *b, const BkEndingControlOps *o,
                 unsigned slot, char e[256]) {
  if (!o->sound)
    return fail(e, "missing sound service");
  return o->sound(o->context, slot, *b->effect_volume, e);
}
static int choice(const BkEndingControlBindings *b, char e[256]) {
  return (b->frame->camera_clip >= 0 && b->frame->camera_clip < 3) ||
         fail(e, "camera choice outside three presets");
}
static void store_preset(const BkEndingControlBindings *b) {
  unsigned k = (unsigned)b->frame->camera_clip;
  b->presets->active[0][k] = b->camera->yaw;
  b->presets->active[1][k] = b->camera->pitch;
  b->presets->active[2][k] = b->camera->radius;
  b->presets->active[3][k] = b->camera->height;
}
static void read_preset(const BkEndingControlBindings *b) {
  unsigned k = (unsigned)b->frame->camera_clip;
  b->camera->yaw = b->presets->active[0][k];
  b->camera->pitch = b->presets->active[1][k];
  b->camera->radius = b->presets->active[2][k];
  b->camera->height = b->presets->active[3][k];
}
static int targets(BkEndingControlState *s, const BkEndingControlBindings *b,
                   char e[256]) {
  unsigned ids[3] = {2, 3, 0};
  if (s->variant == 1) {
    ids[0] = 0;
    ids[1] = ids[2] = 1;
  }
  for (unsigned i = 0; i < 3; ++i) {
    const float *p = b->nodes[ids[i]];
    if (!p)
      return fail(e, "missing actual cached target node");
    for (unsigned j = 0; j < 3; ++j)
      if (!isfinite(p[j]))
        return fail(e, "invalid cached target node");
  }
  for (unsigned i = 0; i < 3; ++i)
    memcpy(s->targets[i], b->nodes[ids[i]], sizeof(s->targets[i]));
  if (s->variant == 1)
    for (unsigned j = 0; j < 3; ++j)
      s->targets[1][j] = (float)(((double)b->nodes[0][j] + b->nodes[1][j]) / 2);
  return 1;
}
static int select_target(BkEndingControlState *s,
                         const BkEndingControlBindings *b, char e[256]) {
  if (s->target_choice < 0 || s->target_choice >= 3)
    return fail(e, "target choice outside three positions");
  memcpy(b->frame->camera_values, s->targets[s->target_choice],
         sizeof(b->frame->camera_values));
  return 1;
}
/* Signed x86 add wraps before idiv; no C signed-overflow UB. */
static int32_t increment_mod(int32_t value, int32_t modulus) {
  uint32_t bits = (uint32_t)value + 1;
  int32_t next;
  memcpy(&next, &bits, sizeof(next));
  return next % modulus;
}
static void toggle(uint8_t *v) {
  if (*v == 0)
    *v = 1;
  else if (*v == 1)
    *v = 0;
}
static int warp(const BkEndingControlBindings *b, const BkEndingControlOps *o,
                char e[256]) {
  if (!o->warp || !isfinite(*b->scale))
    return fail(e, "missing warp service or invalid scale");
  float x = (float)(800.0 * *b->scale), y = (float)(520.0 * *b->scale);
  if (!isfinite(x) || !isfinite(y))
    return fail(e, "warp coordinates overflow");
  return o->warp(o->context, x, y, e);
}
static void pause_state(BkEndingControlState *s,
                        const BkEndingControlBindings *b, unsigned which) {
  if (b->frame->phase != 9)
    s->previous_phase = b->frame->phase;
  b->frame->phase = 9;
  s->pause_selection = which ? 47 : 45;
  s->pause_flags[which] = 1;
  for (unsigned i = 2; i < 6; ++i)
    s->pause_flags[i] = 1;
}
int bk_ending_control_step(BkEndingControlState *s,
                           const BkEndingControlBindings *b,
                           const BkEndingFrameInput *in,
                           const BkEndingControlOps *o, char e[256]) {
  if (!s || !b || !in || !o || !b->frame || !b->camera || !b->presets ||
      !b->rects || !b->scale || !b->effect_volume)
    return fail(e, "missing state/bindings/input");
  for (unsigned i = 0; i < BK_ENDING_CONTROL_RECTS; ++i) {
    const BkEndingControlRect *r = &b->rects[i];
    if (!isfinite(r->x) || !isfinite(r->y) || !isfinite(r->width) ||
        !isfinite(r->height))
      return fail(e, "invalid control bounds");
  }
  int32_t pointer[2];
  memcpy(pointer, in->words + 9, sizeof(pointer));
  BkEndingFrameState *f = b->frame;
  s->hover = 0;
  static const int hover[] = {12, 29, 38, 21, 17, 19, 0, 0, 0, 0, 0, 45, 47};
  for (unsigned i = 0; i < BK_ENDING_CONTROL_RECTS; ++i) {
    const BkEndingControlRect *r = &b->rects[i];
    /* Native x87 adds two float bounds without an intermediate float store. */
    if ((double)pointer[0] < r->x ||
        (double)pointer[0] > (double)r->x + r->width ||
        (double)pointer[1] < r->y ||
        (double)pointer[1] > (double)r->y + r->height)
      continue;
    if (hover[i])
      s->hover = hover[i];
    int yes;
    if (!pressed(o, 1, &yes, e))
      return 0;
    if (!yes) {
      /* First control also polls held keys even though result is unused. */
      if (i == 0 && !pressed(o, 2, &yes, e))
        return 0;
      continue;
    }
    switch (i) {
    case 0:
      if (f->camera_mode == 1) {
        if (!sound(b, o, 5, e))
          return 0;
        break;
      }
      if (!sound(b, o, 0, e))
        return 0;
      f->camera_mode = 2;
      if (!choice(b, e))
        return 0;
      store_preset(b);
      f->camera_clip = increment_mod(f->camera_clip, 3);
      read_preset(b);
      s->target_choice = 0;
      if (!targets(s, b, e) || !select_target(s, b, e))
        return 0;
      memcpy(s->saved_camera, b->camera->matrix, sizeof(s->saved_camera));
      break;
    case 1:
      if (!sound(b, o, 0, e))
        return 0;
      s->mode_721ec4 = increment_mod(s->mode_721ec4, 3);
      break;
    case 2:
      if (f->phase != 5 && f->phase != 6) {
        if (!sound(b, o, 5, e))
          return 0;
        break;
      }
      if (!sound(b, o, 0, e))
        return 0;
      if (!o->auxiliary)
        return fail(e, "missing actual auxiliary transition495d92");
      int32_t accepted;
      if (!o->auxiliary(o->context, increment_mod(f->auxiliary_mode, 4),
                        &accepted, e))
        return 0;
      if (accepted)
        f->auxiliary_mode = increment_mod(f->auxiliary_mode, 4);
      break;
    case 3:
      if (!sound(b, o, 0, e))
        return 0;
      s->target_choice = increment_mod(s->target_choice, 3);
      if (!targets(s, b, e) || !select_target(s, b, e))
        return 0;
      break;
    case 4:
      if (!sound(b, o, 0, e))
        return 0;
      if (f->camera_mode == 0 || f->camera_mode == 2) {
        f->camera_cached = -1;
        f->camera_mode = 1;
      } else
        f->camera_mode = 2;
      break;
    case 5:
      if (!sound(b, o, 0, e))
        return 0;
      f->camera_mode = 2;
      memcpy(b->presets->active, b->presets->authored,
             sizeof(b->presets->active));
      if (!choice(b, e))
        return 0;
      read_preset(b);
      if (!targets(s, b, e))
        return 0;
      s->target_choice = 0;
      if (!select_target(s, b, e))
        return 0;
      break;
    case 6:
    case 7: {
      unsigned enabled = i == 6 ? 4 : 6, value = enabled - 1;
      if (!sound(b, o, s->toggles[enabled] ? 0 : 5, e))
        return 0;
      if (s->toggles[enabled])
        toggle(&s->toggles[value]);
      break;
    }
    case 8:
      if (f->phase == 1 || !s->toggles[2]) {
        if (!sound(b, o, 5, e))
          return 0;
      } else {
        if (!sound(b, o, 0, e))
          return 0;
        if (s->state_721eec == 7 || f->state_721ee4 == 7) {
          if (!sound(b, o, 5, e))
            return 0;
        } else
          toggle(&s->toggles[1]);
      }
      break;
    case 9:
    case 10:
      if (!sound(b, o, 0, e))
        return 0;
      toggle(&s->toggles[i == 9 ? 0 : 7]);
      break;
    case 11:
      pause_state(s, b, 0);
      if (!sound(b, o, 0, e) || !warp(b, o, e))
        return 0;
      break;
    case 12:
      if (!sound(b, o, 0, e))
        return 0;
      pause_state(s, b, 1);
      if (!warp(b, o, e))
        return 0;
      break;
    }
  }
  if (f->camera_mode == 0) {
    if (!choice(b, e))
      return 0;
    store_preset(b);
  }
  if (s->hover) {
    if (!s->hover_armed || s->previous_hover != s->hover) {
      if (!sound(b, o, 3, e))
        return 0;
      s->hover_armed = 1;
      s->previous_hover = s->hover;
    }
  } else
    s->hover_armed = s->previous_hover = 0;
  return 1;
}
