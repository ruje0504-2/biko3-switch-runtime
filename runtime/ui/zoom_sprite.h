#ifndef BK_UI_ZOOM_SPRITE_H
#define BK_UI_ZOOM_SPRITE_H
#include "ui/fade_sprite.h"
/* 50e6ba: enter2, exit0, idle0, no secondary sprite. Position/size and
 * constructor's truncated hit-test half extents belong to the menu layout.
 * A hidden step installs a center pivot and zero scale; requesting before
 * that first step deliberately retains the constructor's unit scale. */
typedef struct {
  BkFadeSprite fade;
  float scale[2], pivot[2];
} BkZoomSprite;
void bk_zoom_sprite_initialize(BkZoomSprite *);
int bk_zoom_sprite_advance(BkZoomSprite *, float seconds);
/* Output left/top/right/bottom from position/size, with original43ed45
 * float stores. Zero size at the start of a zoom is legal. */
int bk_zoom_sprite_rect(const BkZoomSprite *, const float position_size[4],
                        float corners[4]);
#endif
