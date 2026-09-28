#ifndef BK_SCENE_ENDING_UI_TOOLBAR_H
#define BK_SCENE_ENDING_UI_TOOLBAR_H
#include "game/ending_auxiliary.h"
#include "scene/ending_stage_ui.h"
typedef struct {
  const BkEndingFrameState *frame;
  const BkEndingControlState *control;
  const BkEndingAuxiliaryState *auxiliary;
  int32_t *open; /* live72210c; request is frame.camera_request */
} BkEndingUiToolbarBindings;
/* Original4d4a06..4d4ed8, after pointer capture and before branch hints.
 * Selects hover gates, shared toolbar buttons, toggles and pause indicators.
 * Appends immutable snapshots in original order, including repeated draws.
 * Reads live frame/control/auxiliary fields, never copies their ownership.
 * Not the complete4d499b: one-time setup, branch hints, cursor, curtain,
 * reload and final actions remain the caller's ordered responsibilities.
 * Later failure preserves the executed prefix and ends the frame. */
int bk_ending_ui_toolbar(BkEndingUi *, BkEndingStageUi *,
                         const BkEndingUiToolbarBindings *,
                         const float pointer[2], float scale, float seconds,
                         const BkEndingUiHoverOps *, uint8_t *visible,
                         BkEndingUiFrame *, char error[256]);
/* Full UI composition reads beeb7f directly from the shared common owner.
 * The standalone interface above uses frame.curtain_wanted for diagnostics. */
int bk_ending_ui_toolbar_shared(BkEndingUi *, BkEndingStageUi *,
                                const BkEndingUiToolbarBindings *,
                                const uint8_t *curtain_wanted,
                                const float pointer[2], float scale,
                                float seconds, const BkEndingUiHoverOps *,
                                uint8_t *visible, BkEndingUiFrame *,
                                char error[256]);
#endif
