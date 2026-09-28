#ifndef BK_SCENE_PLAYER_HUD_SESSION_H
#define BK_SCENE_PLAYER_HUD_SESSION_H
#include "scene/game_frame.h"
#include "scene/player_hud.h"
/*51a190 phase1 HUD: borrows real action bindings, inventory, contacts and
 * player visibility produced by the current game update. Device view is the
 * current rendered view AFTER forest publication. Coordinates are local to
 * the game's viewport (origin0); letterbox offset is applied by renderer.
 * Native projects the retained all-zero prop matrix before a prop is ever
 * available, yielding undefined integer coordinates/NaN. If projection is
 * invalid while availability!=1, retain the previous unused depth and expose
 * unprojectable=1. An available prop with invalid projection is fatal.
 * counter734050 and menu71bcd9 bind directly to game_frame.hotkeys;
 * specialBEF778 is supplied by the enclosing process owner. */
int bk_player_hud_session_step(BkPlayerHudState *, BkGameFrameState *,
                               uint8_t special_mode, const float view[16],
                               const BkCameraLens *, unsigned width,
                               unsigned height, float seconds, uint32_t now,
                               BkPlayerHudFrame *, int *unprojectable,
                               char error[256]);
#endif
