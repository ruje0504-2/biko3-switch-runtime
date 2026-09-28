#ifndef BK_SCENE_DIALOGUE_RENDER_H
#define BK_SCENE_DIALOGUE_RENDER_H
#include "scene/actor_render.h"
#include "scene/dialogue_world.h"
typedef struct BkDialogueRender BkDialogueRender;
/* Owns GPU actor/batch/light snapshots, borrows world. Preparation executes
 * original root/light dispatch and publishes the forest once at draw phase.
 * No actor stepping, UI or audio work is hidden in draw. */
BkDialogueRender *bk_dialogue_render_create(BkRenderer *, BkResourceStore *,
                                            BkDialogueWorld *, char error[256]);
void bk_dialogue_render_destroy(BkDialogueRender *);
int bk_dialogue_render_prepare(BkDialogueRender *, const BkMenuCamera *,
                               const BkFog *, char error[256]);
int bk_dialogue_render_draw(BkDialogueRender *, char error[256]);
#endif
