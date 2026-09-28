#ifndef BK_UI_EFFECT_SPRITE_H
#define BK_UI_EFFECT_SPRITE_H
#include "ui/fade_sprite.h"
/* Original50e6ba subset used by the ending UI: enter/exit0..3, idle0/3/5,
 * one primary sprite. Unsupported modes reject rather than approximate.
 * The animation angle and last submitted render angle are separate caches. */
typedef struct {
  BkFadeSprite fade;
  float scale[2], pivot[2], degrees, radians, motion[2];
  int32_t half_extent[2];
  uint8_t enter, exit, idle, direction;
} BkEffectSprite;
/*50de40 transform initialization, retaining direction and motion. rect is
 * x/y/width/height; enter2 adds TRUNCATED half sizes to x/y. Atomic rejection.
 */
int bk_effect_sprite_initialize(BkEffectSprite *, float rect[4], uint8_t enter,
                                uint8_t exit, uint8_t idle);
/* Request separately through fade_sprite_request; no implicit request here.
 * Negative intermediate scales are legal original exit2 states. */
int bk_effect_sprite_advance(BkEffectSprite *, const float rect[4],
                             float seconds);
/* Handle-absent50e6ba still advances the CPU state. Its cached render angle
 * stays unchanged. Native null alpha/rotation device calls are suppressed
 * explicitly (they otherwise dereference null); no draw or fake image. */
int bk_effect_sprite_advance_detached(BkEffectSprite *, const float rect[4],
                                      float seconds);
/*43ed45: TL/TR/BR/BL XY corners, rotation before translation; no GPU mutation.
 */
int bk_effect_sprite_quad(const BkEffectSprite *, const float rect[4],
                          float xy[8]);
#endif
