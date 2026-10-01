#ifndef BK_SCENE_ALBUM_RENDER_H
#define BK_SCENE_ALBUM_RENDER_H
#include "scene/album_menu.h"
#include "render/renderer.h"
typedef struct BkAlbumRender BkAlbumRender;
BkAlbumRender *bk_album_render_create(BkRenderer *,char[256]);
void bk_album_render_destroy(BkAlbumRender *);
/* Retire images from the preceding snapshot only on the next actual tick. */
void bk_album_render_begin(BkAlbumRender *);
/* NULL releases the current slot; replacement may follow in the same tick.
 * Draw generations retain exactly the texture observed by the CPU draw. */
int bk_album_render_image(BkAlbumRender *,unsigned slot,uint32_t generation,
                           const BkImage *,char[256]);
int bk_album_render_prepare(BkAlbumRender *,const BkAlbumFrame *,unsigned width,
                             unsigned height,char[256]);
int bk_album_render_draw(BkAlbumRender *,char[256]);
#endif
