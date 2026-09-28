#ifndef BK_SCENE_PAUSE_RENDER_H
#define BK_SCENE_PAUSE_RENDER_H
#include "render/renderer.h"
#include "resource/store.h"
#include "scene/pause.h"
typedef struct BkPauseRender BkPauseRender;
/* Real bk3_00 art and a decoded owned-output screenshot supplied by app.
 * Images are copied to textures; caller retains ownership of the image.
 * Create/prepare/destroy outside active frames. Logical release4 during the
 * CPU step must NOT destroy this object until its last backdrop+UI snapshot
 * has been submitted. This preserves the screenshot pass from before release.
 */
BkPauseRender *bk_pause_render_create(BkRenderer *, BkResourceStore *,
                                      const BkImage *capture, char error[256]);
/* Six real retry images and shared cursor/curtain; no captured background. */
BkPauseRender *bk_pause_render_create_retry(BkRenderer *, BkResourceStore *,
                                            char error[256]);
BkPauseRender *bk_pause_render_create_checkpoint(BkRenderer *,
                                                 BkResourceStore *,
                                                 char error[256]);
void bk_pause_render_destroy(BkPauseRender *);
/* The backdrop snapshot may be prepended to the UI snapshot in the same frame.
 * Geometry is captured in logical viewport pixels. The caller sets viewport. */
int bk_pause_render_prepare(BkPauseRender *, const BkPauseFrame *,
                            unsigned width, unsigned height, char error[256]);
int bk_pause_render_draw(BkPauseRender *, char error[256]);
#endif
