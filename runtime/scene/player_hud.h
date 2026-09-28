#ifndef BK_SCENE_PLAYER_HUD_H
#define BK_SCENE_PLAYER_HUD_H
#include "core/camera.h"
#include "core/timer.h"
#define BK_PLAYER_HUD_SLOTS 49
#define BK_PLAYER_HUD_DRAWS 40
/* Specialization of50de40/50e6ba for enter0/exit0/idle0. Coordinates are
 * already scaled by721ad0; projected coordinates are device pixels. */
typedef struct {
  float x, y, width, height, sx, sy, px, py, uv[4], alpha;
  uint32_t rgb;
  uint8_t stage, blink;
  BkTimer timer;
} BkPlayerHudSprite;
typedef struct {
  BkPlayerHudSprite sprites[BK_PLAYER_HUD_SLOTS];
  float indicator_x, scroll, depth, reserve, extent; /*709c58..68*/
} BkPlayerHudState;
typedef struct {
  const char *name;
  float x, y, width, height;
  uint8_t requested;
} BkPlayerHudLayout;
/* NULL names mean retained slots, including unused slot2 in either mode. */
int bk_player_hud_layout(BkPlayerHudLayout[BK_PLAYER_HUD_SLOTS], unsigned group,
                         uint8_t special_mode, unsigned width);
/*4c9bf0: initializes loaded slots only; timer armed/deadline and blink bytes
 * survive even loaded slots. depth also survives. CPU only, no fake assets. */
int bk_player_hud_initialize(BkPlayerHudState *, unsigned group,
                             uint8_t special_mode, unsigned width);
/*4cc320: phase0/2 central-frame tail and area-prompt transition tail. */
void bk_player_hud_reset_reserve(BkPlayerHudState *);
typedef struct {
  int32_t action, actions[21], trigger_kind, npc_behavior, counter;
  uint8_t interface_mode, npc_in_view, menu_request, response, special_mode;
  uint8_t prop_available, cover_available, npc_prompt, inventory[5];
} BkPlayerHudInput;
/*4cbca4, before4cb902. Projection must use the actual current device view,
 * lens and viewport with prop_interaction.matrix, even if prop_available=0.
 * Changes outcome only on reserve exhaustion. Inputs/state are transactional
 * on invalid values; no outcome audio or phase change occurs in this call. */
int bk_player_hud_update(BkPlayerHudState *, const BkPlayerHudInput *,
                         uint8_t *outcome, const BkScreenPoint *, float seconds,
                         uint32_t now, unsigned width, char error[256]);
typedef struct {
  unsigned slot;
  BkPlayerHudSprite
      sprite; /* Snapshot, NOT an index into final mutable state. */
} BkPlayerHudDraw;
typedef struct {
  BkPlayerHudDraw draws[BK_PLAYER_HUD_DRAWS];
  unsigned count, capture_after;
  uint8_t capture; /*49d0eb call, after overlays, before other HUD elements.*/
} BkPlayerHudFrame;
/*4cb902 +4cc32f +instant50e6ba. Captures exact per-draw state and records
 * screenshot service boundary; caller must execute it in draw order.
 * No screenshot implementation or menu policy is implied by this record.
 * Invalid digit indices reject atomically instead of reading outside49slots.*/
int bk_player_hud_draws(BkPlayerHudState *, const BkPlayerHudInput *,
                        unsigned width, BkPlayerHudFrame *, char error[256]);
/*43ed45 axis-aligned specialization, with original float intermediates. */
int bk_player_hud_rect(const BkPlayerHudSprite *, float rect[4]);
#endif
