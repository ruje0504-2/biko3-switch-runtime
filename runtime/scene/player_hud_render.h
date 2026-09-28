#ifndef BK_SCENE_PLAYER_HUD_RENDER_H
#define BK_SCENE_PLAYER_HUD_RENDER_H
#include "render/renderer.h"
#include "resource/store.h"
#include "scene/player_hud.h"
typedef struct BkPlayerHudRender BkPlayerHudRender;
/* Owns loaded textures and a distinct mesh for every draw (digits repeat
 * slots within the same frame). Mount bk3_00. Creation/reload/prepare must
 * occur outside active renderer frames. Caller sets the game's viewport. */
BkPlayerHudRender *bk_player_hud_render_create(BkRenderer *, BkResourceStore *,
                                               unsigned group,
                                               uint8_t special_mode,
                                               char error[256]);
void bk_player_hud_render_destroy(BkPlayerHudRender *);
/*4c9bf0 ownership: skipped slots retain actual previous resources, even if
 * group changes. Atomic resource replacement; no missing texture fallback.*/
int bk_player_hud_render_reload(BkPlayerHudRender *, BkResourceStore *,
                                unsigned group, uint8_t special_mode,
                                char error[256]);
int bk_player_hud_render_prepare(BkPlayerHudRender *, const BkPlayerHudFrame *,
                                 unsigned width, unsigned height,
                                 char error[256]);
/*49d0eb service callback is required when frame.capture=1; it executes
 * after fullscreen interaction overlays and before other HUD. Its owner
 * decides whether a pending capture exists. Failure terminates this frame.*/
typedef struct {
  void *context;
  int (*capture)(void *, char error[256]);
} BkPlayerHudCapture;
int bk_player_hud_render_draw(BkPlayerHudRender *, const BkPlayerHudCapture *,
                              char error[256]);
#endif
