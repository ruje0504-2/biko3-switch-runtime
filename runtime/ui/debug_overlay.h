#ifndef BK_DEBUG_OVERLAY_H
#define BK_DEBUG_OVERLAY_H
#include "resource/assets.h"
/* CPU image belongs to the caller; release it with bk_image_free. */
int bk_debug_banner_create(BkImage *image, char error[256]);
int bk_debug_banner_lines(BkImage *image, const char *first, const char *second,
                          char error[256]);
/* Tiny reusable ASCII atlas for the original FPS counter's portable display.
 * A..Z,0..9,-,[,],. occupy consecutive6x8 cells in a256x8 image. */
int bk_debug_font_atlas(BkImage *, char error[256]);
#endif
