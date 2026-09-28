#ifndef BK_SCENE_SAVE_MENU_RENDER_H
#define BK_SCENE_SAVE_MENU_RENDER_H
#include "scene/save_menu_view.h"
#include "scene/text_render.h"
typedef struct BkSaveMenuRender BkSaveMenuRender;
/* Real bk3_00 images and original Japanese Type_G.FTT, owned until destroy
 * outside GPU frame. mode0/load and1/save have distinct backgrounds, portraits
 * and questions. */
BkSaveMenuRender *bk_save_menu_render_create(BkRenderer *, BkResourceStore *,
                                             unsigned mode, unsigned group,
                                             char error[256]);
void bk_save_menu_render_destroy(BkSaveMenuRender *);
/* Replaces42..55 only after all new textures loaded. Never during a frame. */
int bk_save_menu_render_details(BkSaveMenuRender *, unsigned group,
                                char error[256]);
int bk_save_menu_render_prepare(BkSaveMenuRender *, const BkSaveMenuFrame *,
                                const uint8_t *labels, size_t label_size,
                                BkTextFlow *, float seconds, unsigned width,
                                unsigned height, char error[256]);
/* Caller sets correct4:3 viewport. Zero-step redraw retains identical state. */
int bk_save_menu_render_draw(BkSaveMenuRender *, char error[256]);
#endif
