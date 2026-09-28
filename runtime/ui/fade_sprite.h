#ifndef BK_UI_FADE_SPRITE_H
#define BK_UI_FADE_SPRITE_H
#include <stdint.h>
/* 50e633/50e6ba specialization: enter1, exit1, idle0, no secondary sprite.
 * Constructor alpha is1; the first hidden step resets it to0. Requests during
 * transitions are ignored, including the frame reaching stage2 or0. */
typedef struct {
  float alpha, speed;
  uint8_t stage;
} BkFadeSprite;
void bk_fade_sprite_initialize(BkFadeSprite *);
/*50e633 only: does not advance alpha/time. */
int bk_fade_sprite_request(BkFadeSprite *, uint8_t wanted);
/*50e6ba only for enter1/exit1/idle0. Used where native draws/steps BEFORE
 * issuing this frame's request (e.g. the common transition curtain). */
int bk_fade_sprite_advance(BkFadeSprite *, float seconds);
/* Request then step, in native order. Nonfinite/out-of-range alpha, negative
 * speed/time and stages outside0..5 reject atomically. */
int bk_fade_sprite_step(BkFadeSprite *, uint8_t wanted, float seconds);
/* Same enter1/exit1 with idle11 alpha pulse. Constructor/request1 retains
 * the native+160 direction byte across resource reload. */
typedef struct {
  BkFadeSprite fade;
  uint8_t direction;
} BkPulseSprite;
void bk_pulse_sprite_initialize(BkPulseSprite *);
/*50e6ba only: advance an idle11 sprite without another visibility request. */
int bk_pulse_sprite_advance(BkPulseSprite *, float seconds);
int bk_pulse_sprite_step(BkPulseSprite *, uint8_t wanted, float seconds);
#endif
