#include "ui/fade_sprite.h"
#include <math.h>
void bk_fade_sprite_initialize(BkFadeSprite *s) {
  if (s)
    *s = (BkFadeSprite){1, 2, 0};
}
int bk_fade_sprite_request(BkFadeSprite *s, uint8_t wanted) {
  if (!s || s->stage > 5)
    return 0;
  if (s->stage == 0 && wanted == 1)
    s->stage = 1;
  else if (s->stage == 3 && wanted == 0)
    s->stage = 4;
  return 1;
}
int bk_fade_sprite_advance(BkFadeSprite *s, float seconds) {
  if (!s || !isfinite(seconds) || seconds < 0 || !isfinite(s->speed) ||
      s->speed < 0 || !isfinite(s->alpha) || s->alpha < 0 || s->alpha > 1 ||
      s->stage > 5)
    return 0;
  BkFadeSprite n = *s;
  double delta = (double)n.speed * .5 * seconds;
  switch (n.stage) {
  case 0:
    n.alpha = 0;
    break;
  case 1:
    n.alpha = (float)(n.alpha + delta);
    if (n.alpha >= 1) {
      n.alpha = 1;
      n.stage = 2;
    }
    break;
  case 2:
    n.stage = 3;
    n.alpha = 1;
    break;
  case 3:
    n.alpha = 1;
    break;
  case 4:
    n.alpha = 1;
    n.stage = 5;
    /* fall through */
  case 5:
    n.alpha = (float)(n.alpha - delta);
    if (n.alpha <= 0) {
      n.alpha = 0;
      n.stage = 0;
    }
    break;
  }
  *s = n;
  return 1;
}
int bk_fade_sprite_step(BkFadeSprite *s, uint8_t wanted, float seconds) {
  if (!s)
    return 0;
  BkFadeSprite n = *s;
  if (!bk_fade_sprite_request(&n, wanted) ||
      !bk_fade_sprite_advance(&n, seconds))
    return 0;
  *s = n;
  return 1;
}
void bk_pulse_sprite_initialize(BkPulseSprite *s) {
  if (s) {
    bk_fade_sprite_initialize(&s->fade);
    bk_fade_sprite_request(&s->fade, 1);
  }
}
int bk_pulse_sprite_advance(BkPulseSprite *s, float seconds) {
  if (!s)
    return 0;
  BkPulseSprite n = *s;
  /* Validate using the same supported transition states. */
  BkFadeSprite checked = n.fade;
  if (!bk_fade_sprite_advance(&checked, seconds))
    return 0;
  if (n.fade.stage == 2 || n.fade.stage == 3) {
    n.fade.stage = 3;
    if (n.direction == 0) {
      n.fade.alpha = (float)((double)n.fade.alpha - seconds);
      if (n.fade.alpha <= 0) {
        n.fade.alpha = 0;
        n.direction = 1;
      }
    } else if (n.direction == 1) {
      n.fade.alpha = (float)((double)n.fade.alpha + seconds);
      if (n.fade.alpha >= 1) {
        n.fade.alpha = 1;
        n.direction = 0;
      }
    }
  } else
    n.fade = checked;
  *s = n;
  return 1;
}
int bk_pulse_sprite_step(BkPulseSprite *s, uint8_t wanted, float seconds) {
  if (!s)
    return 0;
  BkPulseSprite next = *s;
  if (!bk_fade_sprite_request(&next.fade, wanted) ||
      !bk_pulse_sprite_advance(&next, seconds))
    return 0;
  *s = next;
  return 1;
}
