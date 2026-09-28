#ifndef BK_SCENE_COMMON_HUD_H
#define BK_SCENE_COMMON_HUD_H
#include "core/timer.h"
#include "ui/fade_sprite.h"
typedef struct {
  BkFadeSprite curtain;          /*beea18*/
  BkTimer wait;                  /*beeb54*/
  uint8_t gate, action, blocked; /*beeb7c/7e/7f*/
} BkCommonHudState;
typedef struct {
  const uint8_t *outcome; /*71bcd8, shared with player completion*/
  uint8_t *menu_request, *response, *pause_overlay; /*71bcd9/da,725d38*/
  uint32_t *group, *area;
  uint8_t *flow;        /*beeb84, NOT the camera phase*/
  uint8_t special_mode; /*bef778*/
} BkCommonHudBindings;
typedef struct {
  void *context;
  /* Native DirectSound Play(0,0,0), preserving current source position. */
  int (*play_outcome)(void *, char error[256]);
  int (*play_response)(void *, char error[256]);
  /*51c47e transition scheduling, with actual ownership effects. */
  int (*schedule)(void *, uint8_t target, uint8_t mode, char error[256]);
  /*4e7671(4): must create the pause UI before flow changes to4. */
  int (*load_pause)(void *, char error[256]);
} BkCommonHudOps;
typedef struct {
  float curtain_alpha;
} BkCommonHudFrame;
/*4e6dee curtain-specific fields only; retains gate/action/blocked and timer
 * armed/deadline. Process baseline is separately zeroed. */
void bk_common_hud_initialize(BkCommonHudState *);
/*4ebfd0 entry writes: retain curtain, timer deadline and requested action. */
void bk_common_hud_entry_reset(BkCommonHudState *);
/*Complete51a190 tail51a3bc..51a671. Curtain advances before requests, then
 * outcome/menu/response priority, then scheduling once stage3 is reached.
 * All service callbacks required; any failure is fatal, earlier native-
 * ordered side effects remain. Does not implement preceding phase1 HUD.
 * frame captures opacity at draw time, before callbacks may load/release. */
int bk_common_hud_step(BkCommonHudState *, const BkCommonHudBindings *,
                       const BkCommonHudOps *, float seconds, uint32_t now,
                       BkCommonHudFrame *, char error[256]);
#endif
