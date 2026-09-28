#ifndef BK_SCENE_OPENING_SESSION_H
#define BK_SCENE_OPENING_SESSION_H
#include "scene/dialogue_assets.h"
#include "scene/game_frame.h"
#include "scene/item_notice_state.h"
#include "scene/opening_phase.h"
#include "ui/text_draw.h"
typedef enum { BK_NOTICE_TEXT_OPENING, BK_NOTICE_TEXT_ITEMS } BkNoticeTextKind;
int bk_notice_text_style(BkNoticeTextKind, BkTextStyle *);
/* Actual shared font owner: recreate also resets binding to empty, clear
 * frees font surfaces, bind retains the supplied stable message address.
 * Font is Type_S.FTT; opening geometry104,380,432,72 step16,18;
 * item geometry192,400,316,268 step16,16. White shadow1 in both cases. */
typedef struct {
  void *context;
  int (*recreate)(void *, BkNoticeTextKind, char error[256]);
  int (*clear)(void *, char error[256]);
  int (*bind)(void *, const BkMessage *, char error[256]);
} BkNoticeTextOps;
typedef struct {
  BkResourceStore *store;
  BkEntryAssets *entry;
  BkDialogueAssets *dialogue;
  BkItemFeedback *items;
  BkSystemAudio *click;
  BkNoticeTextOps text;
} BkOpeningServices;
/*4ebfd0 dialogue-specific initialization after actor entry init. Retains
 * dialogue current/media state, camera stage and text delay/scroll/target.
 * Requires caller's panel already initialized by4e82b8. No prompt/commonHUD.
 * Selects real camera slot0, loads opening range for phase0 and first page.
 * phase2/3 hide panel without preloading item text (done on handover). */
int bk_opening_session_initialize(const BkOpeningServices *, BkGameFrameState *,
                                  BkItemNoticeState *, BkTextFlow *,
                                  char error[256]);
/* Concrete opening policy with real parser/PCM/item reload/camera selection.
 * Does not step/draw panel or prompt. Preserve the old phase for51a190's
 * remaining UI branch after this call may change phase to1. */
int bk_opening_session_step(const BkOpeningServices *, BkGameFrameState *,
                            BkItemNoticeState *, BkTextFlow *, int advance,
                            char error[256]);
#endif
