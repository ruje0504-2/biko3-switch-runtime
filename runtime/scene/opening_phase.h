#ifndef BK_SCENE_OPENING_PHASE_H
#define BK_SCENE_OPENING_PHASE_H
#include "game/camera_policy.h"
#include "ui/fade_sprite.h"
#include "ui/text_flow.h"
/* Borrow live globals, including the SAME panel and notice byte as items.
 * phase is read once; completed handover does not run phase1 this frame. */
typedef struct {
  BkGameCameraState *camera;
  uint8_t *notice_visible, *npc_hidden, *player_hidden;
  int32_t *npc_behavior;
  BkFadeSprite *panel;
  BkTextFlow *flow;
} BkOpeningBindings;
typedef struct {
  void *context;
  int (*click)(void *, char error[256]);
  int (*next)(void *, int *done, char error[256]);
  int (*bind)(void *, char error[256]);
  int (*clear_font)(void *, char error[256]);
  int (*close_dialogue)(void *, char error[256]);
  int (*prepare_items)(void *, char error[256]);
  int (*select_camera)(void *, char error[256]);
} BkOpeningOps;
/*4ec528/4ec6c0/4ec777 and enclosing phase=1 write only. Called AFTER
 * game_frame and world traversal, BEFORE panel/prompt steps and common HUD.
 * advance is the edge result of native input aliases0,Z,33450..33452(1,0).
 * Source/end are slot0, not whichever camera clip is active.
 * All callbacks are required; failure is fatal to the session (partial
 * native-ordered side effects remain). Reject phase1/unknown phases. */
int bk_opening_phase_step(const BkOpeningBindings *, const BkOpeningOps *,
                          int advance, float source, float end,
                          char error[256]);
#endif
