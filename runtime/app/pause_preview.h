#ifndef BK_APP_PAUSE_PREVIEW_H
#define BK_APP_PAUSE_PREVIEW_H
#include "scene/pause.h"
#include "scene/scene.h"
BkScene *bk_pause_preview_create(const BkSceneServices *, char error[256]);
/* Shared CPU objects outlive this menu, including the last draw after flow
 * changes. No constructor step; owner runs it after common HUD writes flow4. */
BkScene *bk_pause_preview_create_shared(const BkSceneServices *, BkPauseState *,
                                        const BkPauseBindings *,
                                        const BkFlowTransitionOps *,
                                        double elapsed, char error[256]);
int bk_pause_preview_finished_context(void *context);
int bk_pause_preview_resume_context(void *context);
/* Explicit wall time immediately before the following scene step. */
void bk_pause_preview_clock(BkScene *, double elapsed);
#endif
