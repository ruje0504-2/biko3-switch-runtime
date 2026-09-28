#ifndef BK_UI_TEXT_LAYOUT_H
#define BK_UI_TEXT_LAYOUT_H
#include "resource/font.h"
typedef struct {
  int32_t x, y, width, step_x, step_y;
} BkTextLayout;
typedef struct {
  uint16_t code;
  int32_t x, y;
  BkFontGlyph glyph;
} BkTextGlyph;
/*4747e0 full-text layout, before scroll/crop/surface scaling. Scans two bytes
 * at a time up to first NUL; CR advances by step_y and LF is skipped, each
 * still consumes two bytes. Odd final byte is paired with NUL. A glyph uses
 * explicit step_x or its advance+2; wrapping is strictly x>origin+width-cell.
 * Only the wrap branch falls back to cell when step_y==0. Text encoding is
 * not normalized. Every non-newline pair emits a record, including empty
 * glyphs. Count/output unchanged on failure. Font outlives all records.
 */
int bk_text_layout(const BkFont *, const uint8_t *text, size_t size,
                   const BkTextLayout *, BkTextGlyph *out, size_t capacity,
                   size_t *count, char error[256]);
#endif
