#ifndef BK_UI_TEXT_CANVAS_H
#define BK_UI_TEXT_CANVAS_H
#include "ui/text_draw.h"
#include "ui/text_flow.h"
#include "ui/text_image.h"
typedef struct BkTextCanvas BkTextCanvas;
/* Borrow font; own full1280x960 glyph mask and a doubled viewport. Logical
 * width<=640,height<=480. Window dimensions are fixed for this instance;
 * dimension changes require recreation (explicit portable restriction).
 * Uninitialized DirectDraw memory is replaced with a defined black canvas.
 */
BkTextCanvas *bk_text_canvas_create(const BkFont *, uint32_t width,
                                    uint32_t height, char error[256]);
void bk_text_canvas_destroy(BkTextCanvas *);
/*4758f7 has distinct full-raster and last-displayed text caches. Always
 * remember the latest input, even when flow suppresses refresh. Re-rasterize
 * only on changed full text; unchanged text retains its old glyph layout.
 * Moving screen origin requests reupload of the retained viewport.
 * Crop outside the original full canvas is rejected, not silently clipped.
 * All output/flow/canvas state is unchanged on failure. No aliasing inputs.
 */
int bk_text_canvas_prepare(BkTextCanvas *, const BkTextStyle *,
                           const uint8_t *text, size_t size, float seconds,
                           uint32_t target_width, BkTextFlow *, BkTextDraw *,
                           int *upload, char error[256]);
/* Borrowed opaque grayscale viewport, matching RGB font surfaces. */
const BkImage *bk_text_canvas_image(const BkTextCanvas *);
#endif
