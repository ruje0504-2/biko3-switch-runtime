#ifndef BK_SCENE_SELECTION_UI_RENDER_H
#define BK_SCENE_SELECTION_UI_RENDER_H
#include "render/renderer.h"
#include "resource/store.h"
#include "scene/selection_ui.h"
typedef struct BkSelectionUiRender BkSelectionUiRender;
/* Real bk3_00 images. Creation/preparation/destruction are outside active
 * frames. Keep this owner alive through its final pre-release snapshot. */
BkSelectionUiRender *bk_selection_ui_render_create(BkRenderer *,
                                                   BkResourceStore *,
                                                   uint8_t special,
                                                   char error[256]);
void bk_selection_ui_render_destroy(BkSelectionUiRender *);
/* Already evaluated corners and UV cropping, including zero-width labels. No
 * menu stepping or pointer/flow mutation. Caller sets the logical viewport. */
int bk_selection_ui_render_prepare(BkSelectionUiRender *,
                                   const BkSelectionFrame *, unsigned width,
                                   unsigned height, char error[256]);
int bk_selection_ui_render_draw(BkSelectionUiRender *, char error[256]);
#endif
