#ifndef BK_SCENE_ENDING_UI_CURSOR_H
#define BK_SCENE_ENDING_UI_CURSOR_H
#include "game/ending_auxiliary.h"
#include "scene/ending_stage_ui.h"
/* Original+167 bytes have owners distinct from control.pause_flags. They
 * remain live for later event/UI writes and persist across image release. */
typedef struct {
  uint8_t notices[4]; /*slots53..56*/
  uint8_t popups[2];  /*slots72/73*/
} BkEndingUiNoticeState;
typedef struct {
  const BkEndingFrameState *frame;
  const BkEndingAuxiliaryState *auxiliary;
  const int32_t *active_clip;  /*actual primary+140*/
  const int32_t *normal_ready; /*719b0c*/
  const BkEndingUiNoticeState *notices;
} BkEndingUiCursorBindings;
/* Complete4d6404..4d677b, after the real selector supplies selected.
 * visible==1 or phase9 overrides to cursor0. Otherwise an out-of-range
 * explicit selection hides all cursors, as the original loop does.
 * This does NOT invent a value for the caller's uninitialized selection in
 * unknown phases; the full dispatcher must resolve or reject those paths.
 * Cursor position/draw precedes visibility request. Absent cursor images
 * skip animation but still receive position/request; popups/notice images
 * use the distinct unconditional update policy. No curtain update here.
 * Failure preserves the ordered prefix and ends the frame. */
int bk_ending_ui_cursor(BkEndingUi *, BkEndingStageUi *,
                        const BkEndingUiCursorBindings *, int32_t selected,
                        uint8_t visible, const float pointer[2], float seconds,
                        BkEndingUiFrame *, char error[256]);
#endif
