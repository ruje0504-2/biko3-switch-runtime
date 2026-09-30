#ifndef BK_SCENE_SPECIAL_UI_RENDER_H
#define BK_SCENE_SPECIAL_UI_RENDER_H
#include "scene/special_ui.h"
#include "scene/curtain_render.h"
typedef struct BkSpecialUiRender BkSpecialUiRender;
/* Owns actual bk3_00 images; borrows renderer/store/process curtain. No
 * resource fallback. Constructors/image replacement/destruction are outside
 * active frames. A skipped constructor retains its actual previous image. */
BkSpecialUiRender *bk_special_ui_render_create(BkRenderer *, BkResourceStore *,
                                               BkCurtainRender *, char[256]);
void bk_special_ui_render_destroy(BkSpecialUiRender *);
int bk_special_ui_render_image(BkSpecialUiRender *, unsigned slot,
                                const char *name, char[256]);
/* Before each REAL tick, outside active frames: retire the preceding snapshot
 * and pin current image identities. Images released/replaced afterward do
 * not invalidate this frame. Never call begin for a pure redraw. */
int bk_special_ui_render_begin(BkSpecialUiRender *, char[256]);
/* Capture geometry AFTER native control, possibly inside an active frame
 * after49d0eb. No allocations, uploads or GPU waits. prepare once per begin;
 * incomplete CPU frames reject. Caller sets the actual4:3 viewport. */
int bk_special_ui_render_prepare(BkSpecialUiRender *, const BkSpecialUiFrame *,
                                  unsigned width, unsigned height, char[256]);
/* Pure snapshot draw: sprites then process curtain. No input/time/capture or
 * audio side effects. Transient vertices preserve ALPHA/depth-write0. */
int bk_special_ui_render_draw(BkSpecialUiRender *, char[256]);
#endif
