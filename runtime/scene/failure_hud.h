#ifndef BK_SCENE_FAILURE_HUD_H
#define BK_SCENE_FAILURE_HUD_H
#include "scene/common_hud.h"
#include "ui/text_flow.h"
typedef struct {
  uint32_t advances; /*BFBBB4, native wrapping DWORD*/
  uint8_t visible;   /*BF9B98*/
} BkFailureHudState;
typedef struct {
  BkCommonHudState *common;
  BkFadeSprite *panel, *prompt; /*BEEB88, BEF608; borrowed opening sprites*/
  BkTextFlow *text;
  const uint32_t *group;
  uint8_t *outcome, *pause_overlay, *npc_visible;
  int8_t *player_script_phase, *player_completion_mode, *npc_stimulus;
  int32_t *npc_behavior;
  uint32_t *npc_wait_duration;
} BkFailureHudBindings;
typedef struct {
  void *context;
  int (*click)(void *, char error[256]); /*46435e system slot4*/
  /*51876a selection then4aa9f4 font binding. Must consume the real message. */
  int (*message)(void *, uint32_t label, char error[256]);
  int (*release)(void *, uint8_t flow, char error[256]);
  int (*schedule)(void *, uint8_t target, uint8_t mode, char error[256]);
} BkFailureHudOps;
typedef struct {
  /* Draw order: weather, panel, prompt, optional text, curtain. Alpha values
   * precede the following native requests/resource releases. */
  float panel_alpha, prompt_alpha, curtain_alpha;
  int draw_text;
} BkFailureHudFrame;
/*51AFCB only; loader4EB0EE and outcome camera/animation51B244 are separate.
 * Confirmation aliases0/Z/33450/33451/33452 are decoded by the platform.
 * No reset at entry: caller retains both state fields and shared sprites.
 * Invalid basic inputs reject first; later service failure is fatal with the
 * original ordered side effects preserved. Does not claim missing loaders. */
int bk_failure_hud_step(BkFailureHudState *, const BkFailureHudBindings *,
                        const BkFailureHudOps *, int advance, float seconds,
                        BkFailureHudFrame *, char error[256]);
#endif
