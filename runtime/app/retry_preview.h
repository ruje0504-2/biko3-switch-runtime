#ifndef BK_APP_RETRY_PREVIEW_H
#define BK_APP_RETRY_PREVIEW_H
#include "scene/checkpoint_prompt.h"
#include "scene/retry.h"
#include "scene/scene.h"
/* All CPU state belongs to the process session. No constructor tick. */
BkScene *bk_retry_preview_create(const BkSceneServices *, BkRetryState *,
                                 const BkPauseBindings *,
                                 const BkFlowTransitionOps *, double elapsed,
                                 char error[256]);
/* Same actual input/audio/renderer adapter for the five-sprite area prompt.
 * phase and reserve borrow the retained game's live CPU fields. */
BkScene *bk_checkpoint_preview_create(const BkSceneServices *,
                                      BkCheckpointPrompt *,
                                      const BkPauseBindings *,
                                      const BkFlowTransitionOps *,
                                      uint8_t *phase, float *reserve,
                                      double elapsed, char error[256]);
/* Explicit wall time immediately before the following scene step. */
void bk_retry_preview_clock(BkScene *, double elapsed);
#endif
