#ifndef BK_SCENE_GALLERY_MENU_RENDER_H
#define BK_SCENE_GALLERY_MENU_RENDER_H
#include "scene/gallery_menu.h"
#include "render/renderer.h"
#include "resource/store.h"
typedef struct BkGalleryMenuRender BkGalleryMenuRender;
BkGalleryMenuRender *bk_gallery_menu_render_create(BkRenderer *, BkResourceStore *, char error[256]);
void bk_gallery_menu_render_destroy(BkGalleryMenuRender *);
/* Retires the preceding immutable frame at the next actual menu update. */
void bk_gallery_menu_render_begin(BkGalleryMenuRender *);
/* BkGalleryMenuOps.image adapter; closing a picture retains its last draw. */
int bk_gallery_menu_render_image(BkGalleryMenuRender *, unsigned slot,
                                  const char *name, char error[256]);
int bk_gallery_menu_render_prepare(BkGalleryMenuRender *, const BkGalleryMenuFrame *,
                                    unsigned width, unsigned height, char error[256]);
int bk_gallery_menu_render_draw(BkGalleryMenuRender *, char error[256]);
#endif
