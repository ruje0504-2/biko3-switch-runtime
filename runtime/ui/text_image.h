#ifndef BK_UI_TEXT_IMAGE_H
#define BK_UI_TEXT_IMAGE_H
#include "resource/assets.h"
#include "ui/text_layout.h"
/* Full, unscrolled white glyph mask from original FTT/text layout. Each glyph
 * copies its whole storage rectangle, including zero bits, as450794's plain
 * BltFast does; overlapping rectangles do not accumulate coverage. Alpha is
 * 0/255, RGB white. Clipping to the canvas is an explicit portable policy.
 * Owns RGBA on success; failure preserves out. Input/layout/output must not
 * alias. Limits:4096x4096 canvas and10240 text bytes. No original scroll,
 * shadow, outline, color blending, or notice timers are implied here.
 */
int bk_text_image(const BkFont *, const uint8_t *text, size_t size,
                  const BkTextLayout *, uint32_t width, uint32_t height,
                  BkImage *out, char error[256]);
#endif
