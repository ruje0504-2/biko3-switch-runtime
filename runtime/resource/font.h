#ifndef BK_RESOURCE_FONT_H
#define BK_RESOURCE_FONT_H
#include <stddef.h>
#include <stdint.h>
typedef struct BkFont BkFont;
typedef struct {
  /* Borrowed MSB-first one-bit rows. width includes the authored row padding,
   * unlike advance_width. Zero-area glyphs have NULL bits. */
  const uint8_t *bits;
  uint32_t row_bytes, width, height, advance_width;
  int32_t offset_x, offset_y, advance;
} BkFontGlyph;
/*475406 FTT: LE cell size,32768 twelve-byte records for codes8000..ffff,
 * then bitmaps. Owns a copy; lower codes use the native zero-initialized
 * table. Offsets and advances follow474a58/474cdb/47501b. No transcoding,
 * native surfaces, or UI state. Invalid data fails without an instance.
 * Bounded policy: cell/dimensions <=4096; original assets use32-pixel cells.
 */
BkFont *bk_font_decode(const void *, size_t, char error[256]);
void bk_font_destroy(BkFont *);
uint32_t bk_font_cell(const BkFont *);
/* code is first byte<<8|second byte, independent of host endianness. */
int bk_font_glyph(const BkFont *, uint16_t code, BkFontGlyph *);
#endif
