#include "scene/dialogue_backdrop.h"
#include <stdio.h>
#include <string.h>
int bk_dialogue_backdrop_extent(float out[2], unsigned width, int replacement) {
  if (!out || !width || width > 16384 || (replacement != 0 && replacement != 1))
    return 0;
  double base = replacement ? 1024 : 1280;
  float scale = (float)((double)width / base);
  out[0] = (float)(base * scale);
  out[1] = (float)((replacement ? 768.0 : 960.0) * scale);
  return 1;
}
int bk_dialogue_backdrop_step(BkDialogueBackdrop *s,
                              const BkDialogueBackdropBindings *b,
                              const BkDialogueBackdropOps *ops, float seconds,
                              BkDialogueBackdropFrame *out, char e[256]) {
  if (!s || !b || !ops || !ops->replace || !out || !b->phase ||
      !b->expression || !b->curtain || !b->curtain_wanted || !b->image ||
      !memchr(b->image, 0, 256))
    goto invalid;
  BkFadeSprite checked_image = s->image, checked_curtain = *b->curtain;
  if (!bk_fade_sprite_advance(&checked_image, seconds) ||
      !bk_fade_sprite_advance(&checked_curtain, seconds))
    goto invalid;
  *out = (BkDialogueBackdropFrame){0};
  switch (*b->phase) {
  case 1:
    *b->expression = -1;
    break;
  case 2:
    if (s->curtain_cycle == 0) {
      /* Original case-sensitive seven-byte prefix, not a full name match. */
      if (!strncmp(b->image, "SG00001", 7)) {
        s->curtain_cycle = 1;
        *b->phase = 10;
        *b->curtain_wanted = 1;
      } else
        s->image_wanted = 0;
    } else if (s->curtain_cycle == 2) {
      *b->phase = 10;
      *b->curtain_wanted = 1;
    }
    break;
  case 3:
    if (!ops->replace(ops->context, b->image, e))
      return 0;
    bk_fade_sprite_initialize(&s->image);
    out->replaced = 1;
    *b->phase = 4;
    if (s->image_kind == 1)
      s->image_kind = 2;
    break;
  case 4:
    s->image_wanted = 1;
    break;
  case 5:
    *b->expression = s->saved_expression;
    *b->phase = 0;
    if (s->curtain_cycle == 1)
      s->curtain_cycle = 2;
    break;
  case 10:
    if (b->curtain->stage == 3)
      s->image_wanted = 0;
    break;
  case 11:
    if (s->curtain_cycle == 1 || s->curtain_cycle == 2) {
      *b->curtain_wanted = 0;
      if (b->curtain->stage == 0) {
        if (s->curtain_cycle == 2)
          s->curtain_cycle = 0;
        *b->phase = 5;
      }
    }
    break;
  }
  bk_fade_sprite_step(&s->image, s->image_wanted, seconds);
  out->image_alpha = s->image.alpha;
  if (s->image.stage == 0) {
    if ((!s->curtain_cycle && *b->phase == 2) ||
        (s->curtain_cycle && *b->phase == 10))
      *b->phase = 3;
  } else if (s->image.stage == 3 && *b->phase == 4)
    *b->phase = s->curtain_cycle ? 11 : 5;
  bk_fade_sprite_advance(b->curtain, seconds);
  out->curtain_alpha = b->curtain->alpha;
  bk_fade_sprite_request(b->curtain, *b->curtain_wanted);
  return 1;
invalid:
  snprintf(e, 256, "dialogue backdrop: invalid state/services/time");
  return 0;
}
