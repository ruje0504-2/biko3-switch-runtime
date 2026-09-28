#ifndef BK_SCENE_FPS_OVERLAY_H
#define BK_SCENE_FPS_OVERLAY_H
#include "render/renderer.h"
typedef struct BkFpsOverlay BkFpsOverlay;
BkFpsOverlay *bk_fps_overlay_create(BkRenderer *, char error[256]);
/* Active frame after gameplay/captures. One atlas, no frame uploads/waits.
 * Inspection scenes place the label at the top to avoid their debug banner.
 * Counter is supplied by core/game_clock's original4adc96 sampler. */
int bk_fps_overlay_draw(BkFpsOverlay *, unsigned fps, int inspection,
                        char error[256]);
void bk_fps_overlay_destroy(BkFpsOverlay *);
#endif
