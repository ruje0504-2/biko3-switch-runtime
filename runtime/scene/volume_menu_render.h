#ifndef BK_SCENE_VOLUME_MENU_RENDER_H
#define BK_SCENE_VOLUME_MENU_RENDER_H
#include "scene/volume_menu.h"
#include "render/renderer.h"
#include "resource/store.h"
typedef struct BkVolumeMenuRender BkVolumeMenuRender;
BkVolumeMenuRender *bk_volume_menu_render_create(BkRenderer *, BkResourceStore *, char[256]);
void bk_volume_menu_render_destroy(BkVolumeMenuRender *);
/* Immutable draw snapshot. Caller holds this owner through the last redraw
 * and sets the logical4:3 viewport. No input, clock or sound on redraw. */
int bk_volume_menu_render_prepare(BkVolumeMenuRender *, const BkVolumeMenuFrame *,
                                   unsigned width, unsigned height, char[256]);
int bk_volume_menu_render_draw(BkVolumeMenuRender *, char[256]);
#endif
