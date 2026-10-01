#ifndef BK_SCENE_ALBUM_CATALOG_H
#define BK_SCENE_ALBUM_CATALOG_H
#include "resource/assets.h"
/*472DCF catalog:1024x768 background,4x5 thumbnails, black-key frame on each
 * present photo, empty tile otherwise. Photos borrow decoded images orNULL.
 * Source/destination rectangles follow the native blits; nearest sampling is
 * the portable DirectDraw replacement. Output must start empty. */
int bk_album_catalog_build(const BkImage *background,const BkImage *border,
                            const BkImage *empty,const BkImage *const photos[20],
                            BkImage *out,char error[256]);
#endif
