#ifndef BK_UI_TEXT_DRAW_H
#define BK_UI_TEXT_DRAW_H
#include <stdint.h>
typedef struct {
  int32_t x, y, width, height, step_x, step_y, shadow;
  float opacity, color[3], shadow_distance;
} BkTextStyle;
typedef enum { BK_TEXT_DARKEN, BK_TEXT_ADD } BkTextBlend;
typedef struct {
  BkTextBlend blend;
  float x, y, width, height;
  uint32_t argb;
} BkTextPass;
typedef struct {
  unsigned count;
  BkTextPass passes[4];
} BkTextDraw;
/*4758f7 draw policy and4761af unrotated quad/color setup. Scale is target
 * width/640, all four rectangle integers truncate BEFORE shadow displacement.
 * Optional shadow darken, base darken, add, final color darken. Native vertex
 * alpha is always1; opacity is applied to RGB and truncated to8-bit channels.
 * Texture must be opaque grayscale (not a white/alpha mask). Passes preserve
 * the original nonlinear filtered-edge behavior; one alpha blend differs.
 * This does not update the text cache, viewport texture, flow or GPU state.
 * Valid colors/opacity0..1, positive rectangle; failures preserve output.
 */
int bk_text_draw(const BkTextStyle *, uint32_t target_width, BkTextDraw *);
#endif
