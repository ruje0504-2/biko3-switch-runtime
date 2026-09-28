#ifndef BK_SCENE_TITLE_MENU_RENDER_H
#define BK_SCENE_TITLE_MENU_RENDER_H
#include "render/renderer.h"
#include "resource/store.h"
#include "scene/title_menu.h"
typedef struct BkTitleMenuRender BkTitleMenuRender;
/* Real bk3_00 images. Creation/preparation/destruction are outside active
 * frames. Keep this owner alive through its final pre-release snapshot. */
BkTitleMenuRender *bk_title_menu_render_create(BkRenderer *, BkResourceStore *,
                                               uint8_t special,
                                               char error[256]);
void bk_title_menu_render_destroy(BkTitleMenuRender *);
/* Already evaluated corners, including zero-sized initial zooms. No menu
 * stepping or pointer/flow mutation. Caller sets the logical viewport. */
int bk_title_menu_render_prepare(BkTitleMenuRender *, const BkTitleMenuFrame *,
                                 unsigned width, unsigned height,
                                 char error[256]);
int bk_title_menu_render_draw(BkTitleMenuRender *, char error[256]);
#endif
