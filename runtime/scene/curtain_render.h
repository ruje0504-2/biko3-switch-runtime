#ifndef BK_SCENE_CURTAIN_RENDER_H
#define BK_SCENE_CURTAIN_RENDER_H
#include "render/renderer.h"
#include "resource/store.h"
#include "scene/common_hud.h"
typedef struct BkCurtainRender BkCurtainRender;
/* Process-level ma_01.tga owner; not destroyed on game-entry changes.
 * Prepare outside GPU frames, draw after phase HUD inside the game viewport.
 * Uses the captured pre-request opacity from common_hud, no state updates. */
BkCurtainRender *bk_curtain_render_create(BkRenderer *, BkResourceStore *,
                                          char error[256]);
void bk_curtain_render_destroy(BkCurtainRender *);
int bk_curtain_render_prepare(BkCurtainRender *, const BkCommonHudFrame *,
                              unsigned width, unsigned height, char error[256]);
int bk_curtain_render_draw(BkCurtainRender *, char error[256]);
/* Draw an immutable CPU frame using transient vertices inside an active GPU
 * frame. Allows native UI evaluation after a synchronous capture; does not
 * change the owner's existing prepared mesh/snapshot. */
int bk_curtain_render_draw_frame(BkCurtainRender *, const BkCommonHudFrame *,
                                  unsigned width, unsigned height, char[256]);
#endif
