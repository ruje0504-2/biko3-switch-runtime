#ifndef BK_SCENE_ENDING_UI_FRAME_H
#define BK_SCENE_ENDING_UI_FRAME_H
#include "game/ending_reload.h"
#include "scene/ending_ui_hints.h"
#include "scene/ending_ui_select.h"
#include "scene/ending_ui_tail.h"
#include "scene/ending_ui_toolbar.h"
typedef struct {
  BkEndingUiHintsState hints;
  BkEndingUiNormalNotice normal;
  BkEndingUiAuxNotice auxiliary;
} BkEndingUiController;
/* Compose existing live bindings. Identity checks reject accidentally copied
 * frame/control/auxiliary/notice/target owners. Resource pointer bindings may
 * be rebound by their loader between calls; pointed-to state remains live. */
typedef struct {
  BkCommonHudState *common;
  BkEndingUiToolbarBindings toolbar;
  BkEndingUiHintsBindings hints;
  BkEndingUiSelectBindings select;
  BkEndingUiCursorBindings cursor;
  BkEndingReloadBindings reload;
  BkEndingUiTailBindings tail;
} BkEndingUiFrameBindings;
typedef struct {
  void *context;
  int (*position)(void *, float out[2], char error[256]); /*4b75aa*/
  int (*motion)(void *, float out[2],
                char error[256]); /*4b757e after position*/
  int (*key)(void *, unsigned code, unsigned mode, uint32_t *, char error[256]);
  int (*voice_playing)(void *, int *, char error[256]);
  BkEndingReloadOps reload;
  BkEndingUiTailOps tail;
} BkEndingUiFrameOps;
typedef struct {
  BkEndingUiFrame sprites;
  unsigned curtain_after; /* all earlier sprites use pre-reload image owners */
  BkCommonHudFrame curtain;
  int early_return, complete;
} BkEndingUiCompositeFrame;
/* Complete4d4979/4d499b scheduling with real implemented children. The position
 * and motion services are ordered, not pre-sampled before first-use
 * initialization. Shared beeb7e/7f belong to common, not frame's standalone
 * diagnostic fields. A failure preserves the native prefix and leaves
 * output.complete=0; do not render or continue that frame. Phase7 is
 * demonstrably called by4d4979 with uninitialized selection: explicit policy
 * selects no cursor unless visible1 overrides it. Other unknown phases still
 * fail. Final image74 advances before first-use initialization/input,
 * matching4d4979. No invented loader/key/audio callbacks or production
 * lifecycle is supplied.
 */
int bk_ending_ui_frame(BkEndingUi *, BkEndingStageUi *, BkEndingUiController *,
                       const BkEndingUiFrameBindings *,
                       const BkEndingUiFrameOps *, float scale, float seconds,
                       BkEndingUiCompositeFrame *, char error[256]);
#endif
