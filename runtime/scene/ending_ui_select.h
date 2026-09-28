#ifndef BK_SCENE_ENDING_UI_SELECT_H
#define BK_SCENE_ENDING_UI_SELECT_H
#include "game/ending_auxiliary.h"
#include "scene/ending_stage_ui.h"
#include "scene/ending_ui_pick.h"
typedef struct {
  BkEndingFrameState *frame;
  const BkEndingControlState *control;
  const BkEndingAuxiliaryState *auxiliary;
  const int32_t *stage3_state; /*721ee8*/
  const int32_t *active_clip;  /*primary+140*/
  const int32_t *open;         /*72210c*/
  const int32_t *actions;      /*live709db8,80 words, not a new config copy*/
  const int32_t (*targets)[2]; /*live721f90*/
  size_t target_count;
  const float *camera_local; /*645604 node+80*/
  const BkEndingUiPickBindings *pick;
  const int32_t *unavailable; /*6afd0c/10, two live values*/
  const uint8_t *item;        /*71bcdc*/
} BkEndingUiSelectBindings;
typedef struct {
  void *context;
  int (*key)(void *, unsigned code, unsigned mode, uint32_t *, char error[256]);
  /*Actual speech slot1, not music. Missing buffer -> playing0; a missing
   * service is an interface failure. Queries occur only at native gates. */
  int (*voice_playing)(void *, int *, char error[256]);
} BkEndingUiSelectOps;
/* Full4db3e4 and4797dc. They append ring50 draws and change the SAME
 * frame.camera_request. Geometry/required bindings fail explicitly.
 * Original row14 returns an uninitialized value; retail normal tables set
 * its target=-1, bypassed before lookup. A live table making it reachable
 * is rejected. Subsequent errors retain already-executed side effects. */
int bk_ending_ui_select_normal(BkEndingUi *, BkEndingStageUi *,
                               const BkEndingUiSelectBindings *,
                               const float pointer[2], float seconds,
                               int32_t *selected, BkEndingUiFrame *,
                               char error[256]);
int bk_ending_ui_select_third(BkEndingUi *, BkEndingStageUi *,
                              const BkEndingUiSelectBindings *,
                              const float pointer[2], float seconds,
                              int32_t *selected, BkEndingUiFrame *,
                              char error[256]);
/*4d5e60..4d6420: selectors, speech gate, live held keys then visible/phase9
 * override. Calls real children above, never a guessed cursor. Unknown
 * phases without the final override have undefined native stack selection
 * and are rejected until the actual caller establishes reachability/policy.
 * Use before ending_ui_cursor; its matching final override is idempotent. */
int bk_ending_ui_select(BkEndingUi *, BkEndingStageUi *,
                        const BkEndingUiSelectBindings *,
                        const float pointer[2], float seconds, uint8_t visible,
                        const BkEndingUiSelectOps *, int32_t *selected,
                        BkEndingUiFrame *, char error[256]);
#endif
