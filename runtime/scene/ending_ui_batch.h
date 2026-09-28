#ifndef BK_SCENE_ENDING_UI_BATCH_H
#define BK_SCENE_ENDING_UI_BATCH_H
#include "scene/curtain_render.h"
#include "scene/ending_ui_frame.h"
#include "scene/ending_ui_render.h"
typedef struct BkEndingUiBatch BkEndingUiBatch;
/* Borrow the process curtain. Each prepared image owner must be exclusive to
 * this batch until it has been drawn; other prepare calls would replace its
 * mesh snapshots. One batch per sequential UI frame, no per-frame allocation.
 * All begin/prepare/clear/destroy calls are outside active GPU frames. */
BkEndingUiBatch *bk_ending_ui_batch_create(BkCurtainRender *, char error[256]);
void bk_ending_ui_batch_clear(BkEndingUiBatch *);
void bk_ending_ui_batch_destroy(BkEndingUiBatch *);
/* Call BEFORE the CPU dispatcher: pins old images even if its reload callback
 * releases the application reference. Retires the previous completed batch. */
int bk_ending_ui_batch_begin(BkEndingUiBatch *, BkEndingUiRender *base,
                             BkEndingUiRender *stage, char error[256]);
/* Call after successful CPU dispatch with current post-reload owners. NULL
 * owners are valid only if the corresponding image set has no draw. Same
 * owners on both sides are merged into one mesh preparation. Failed prepare
 * makes this batch undrawable; retained resources survive until clear. */
int bk_ending_ui_batch_prepare(BkEndingUiBatch *,
                               const BkEndingUiCompositeFrame *,
                               BkEndingUiRender *base, BkEndingUiRender *stage,
                               unsigned width, unsigned height,
                               char error[256]);
/* Original sprite order, then captured common curtain at its exact boundary,
 * then tail sprites using post-reload owners. Pure redraw, no CPU updates. */
int bk_ending_ui_batch_draw(BkEndingUiBatch *, char error[256]);
#endif
