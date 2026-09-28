#ifndef BK_SCENE_ENDING_UI_RENDER_H
#define BK_SCENE_ENDING_UI_RENDER_H
#include "render/renderer.h"
#include "resource/store.h"
#include "scene/ending_stage_ui.h"
typedef struct BkEndingUiRender BkEndingUiRender;
/* Owns the62 actual bk3_00 images and independent per-draw quads. Required
 * images must exist/decode; any failure releases all partially created data.
 * Create/prepare/destroy outside active frames; retain through final draw. */
BkEndingUiRender *bk_ending_ui_render_create(BkRenderer *, BkResourceStore *,
                                             char error[256]);
/* Owns only the currently loaded stage images, including shared cursor8.
 * Copies texture ownership at creation; later CPU release/reload does not
 * invalidate an old render snapshot. Retire after its final GPU use. */
BkEndingUiRender *bk_ending_ui_render_create_stage(BkRenderer *,
                                                   BkResourceStore *,
                                                   const BkEndingStageUi *,
                                                   char error[256]);
void bk_ending_ui_render_destroy(BkEndingUiRender *);
/* Retain an image/mesh owner across CPU resource release. destroy releases
 * one reference; final release must remain outside the active GPU frame. */
BkEndingUiRender *bk_ending_ui_render_retain(BkEndingUiRender *);
/* Snapshots can contain multiple DIFFERENT draws of the same sprite slot.
 * Does not advance animation; caller supplies and sets logical viewport. */
int bk_ending_ui_render_prepare(BkEndingUiRender *, const BkEndingUiFrame *,
                                unsigned width, unsigned height,
                                char error[256]);
int bk_ending_ui_render_draw(BkEndingUiRender *, char error[256]);
/* Allows the global UI scheduler to interleave base/stage/curtain owners
 * without losing native draw order. No additional state advancement. */
int bk_ending_ui_render_draw_range(BkEndingUiRender *, unsigned first,
                                   unsigned count, char error[256]);
#endif
