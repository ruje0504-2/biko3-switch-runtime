#ifndef BK_SCENE_SPECIAL_RENDER_H
#define BK_SCENE_SPECIAL_RENDER_H
#include "scene/actor_render.h"
#include "scene/special_world.h"
typedef struct BkSpecialRender BkSpecialRender;
/* Owns one real bk3_14 actor, light/queue snapshots, and group1/4's
 * bk3_18/poi.avi surface. Borrows world/renderer until destruction.
 * CPU world must outlive retained GPU snapshots; creation/prepare/destruction
 * occur outside active GPU frames. Movie time is signed process elapsed ms. */
BkSpecialRender *bk_special_render_create(BkRenderer *, BkResourceStore *,
    BkSpecialWorld *, int32_t movie_clock_ms, char error[256]);
void bk_special_render_destroy(BkSpecialRender *);
/* Only4e6138 updates movie time; prepare and pure redraw do not. */
int bk_special_render_movie_step(BkSpecialRender *, int32_t now_ms,
                                 int32_t restart_ms, char error[256]);
uint32_t bk_special_render_movie_frame(const BkSpecialRender *);
const BkImage *bk_special_render_movie_image(const BkSpecialRender *);
/* Executes the exact flow48 mode1/10 light plan and one body draw/flush,
 * publishing that original subtree, then captures immutable GPU inputs.
 * Fog is explicit retained device state. Camera must match forest anchor1.
 * No animation, UI, input, audio or movie time advancement here. */
int bk_special_render_prepare(BkSpecialRender *, const BkMenuCamera *,
                               const BkFog *, char error[256]);
int bk_special_render_draw(BkSpecialRender *, char error[256]);
int bk_special_render_stats(const BkSpecialRender *, BkActorRenderStats *);
/* Borrowed inspection/mesh-transfer access, same lifetime as this owner. */
BkActorRender *bk_special_render_actor(BkSpecialRender *);
uint32_t bk_special_render_submissions(const BkSpecialRender *);
#endif
