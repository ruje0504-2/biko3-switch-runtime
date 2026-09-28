#ifndef BK_SCENE_TEXT_RENDER_H
#define BK_SCENE_TEXT_RENDER_H
#include "render/renderer.h"
#include "resource/store.h"
#include "ui/text_canvas.h"
typedef struct BkTextRender BkTextRender;
/* Owns FTT, canvas, texture and four pass meshes. Borrow renderer/store only
 * for their documented operations. Font is read from mounted pack 'fonts'.
 * Create/prepare/destroy outside frames; draw during the active frame.
 */
BkTextRender *bk_text_render_create(BkRenderer *, BkResourceStore *,
                                    const char *font, uint32_t width,
                                    uint32_t height, char error[256]);
void bk_text_render_destroy(BkTextRender *);
/* CPU and earlier GPU updates remain applied on a GPU failure; terminate
 * that frame. Caller owns the global flow state and notice visibility policy.
 * Prepare does not decide when a notice is visible or poll its timer.
 * width/height describe the active drawing viewport, allowing a centered
 * 4:3 game view on a widescreen target. Caller sets that viewport for draw. */
int bk_text_render_prepare(BkTextRender *, const BkTextStyle *,
                           const uint8_t *text, size_t size, float seconds,
                           unsigned width, unsigned height, BkTextFlow *,
                           char error[256]);
int bk_text_render_draw(BkTextRender *, char error[256]);
const BkTextDraw *bk_text_render_snapshot(const BkTextRender *);
const BkImage *bk_text_render_image(const BkTextRender *);
#endif
