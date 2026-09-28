#ifndef BK_SCENE_DIALOGUE_UI_RENDER_H
#define BK_SCENE_DIALOGUE_UI_RENDER_H
#include "scene/dialogue_ui.h"
#include "scene/text_render.h"
typedef struct BkDialogueUiRender BkDialogueUiRender;
/* Owns ma_04/ma_05 textures, meshes and Type_S.FTT canvas. Borrow services.
 * Common curtain belongs to the outer session and draws after this owner. */
BkDialogueUiRender *
bk_dialogue_ui_render_create(BkRenderer *, BkResourceStore *, char error[256]);
void bk_dialogue_ui_render_destroy(BkDialogueUiRender *);
/* Called by the control's text service at4aaa17, outside active GPU frames.
 * Updates the retained font canvas and shared text flow exactly once. */
int bk_dialogue_ui_render_text(BkDialogueUiRender *, const BkMessage *,
                               BkTextFlow *, float game_seconds, unsigned width,
                               unsigned height, char error[256]);
/* Captures panel/prompt after control. Must follow text preparation with the
 * same extent. Draw emits panel, prompt, text without further state changes. */
int bk_dialogue_ui_render_prepare(BkDialogueUiRender *,
                                  const BkDialogueUiFrame *, unsigned width,
                                  unsigned height, char error[256]);
int bk_dialogue_ui_render_draw(BkDialogueUiRender *, char error[256]);
const BkImage *bk_dialogue_ui_render_text_image(const BkDialogueUiRender *);
const BkTextDraw *
bk_dialogue_ui_render_text_snapshot(const BkDialogueUiRender *);
#endif
