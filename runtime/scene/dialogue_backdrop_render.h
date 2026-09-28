#ifndef BK_SCENE_DIALOGUE_BACKDROP_RENDER_H
#define BK_SCENE_DIALOGUE_BACKDROP_RENDER_H
#include "render/renderer.h"
#include "resource/store.h"
#include "scene/dialogue_backdrop.h"
typedef struct BkDialogueBackdropRender BkDialogueBackdropRender;
/* Owns current bk3_00 image and independent ma_02.tga curtain. All resource
 * changes/prepare/destroy occur outside active GPU frames; replacement
 * invalidates the preceding snapshot only after complete resource success. */
BkDialogueBackdropRender *bk_dialogue_backdrop_render_create(BkRenderer *,
                                                             BkResourceStore *,
                                                             const char *image,
                                                             char error[256]);
void bk_dialogue_backdrop_render_destroy(BkDialogueBackdropRender *);
int bk_dialogue_backdrop_render_replace(BkDialogueBackdropRender *,
                                        const char *image, char error[256]);
int bk_dialogue_backdrop_render_prepare(BkDialogueBackdropRender *,
                                        const BkDialogueBackdropFrame *,
                                        unsigned width, unsigned height,
                                        char error[256]);
/* Draw captured background then curtain. No control/animation side effects;
 * the same prepared frame may be redrawn without another state step. */
int bk_dialogue_backdrop_render_draw(BkDialogueBackdropRender *,
                                     char error[256]);
#endif
