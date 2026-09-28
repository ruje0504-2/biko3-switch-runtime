#ifndef BK_SCENE_SELECTION_RENDER_H
#define BK_SCENE_SELECTION_RENDER_H
#include "scene/actor_render.h"
#include "scene/selection_world.h"
typedef struct BkSelectionRender BkSelectionRender;
/* Owns stage/body GPU resources; borrows this exact world and current body.
 * Normal60 constructor; alternate61 requires create_at with a movie clock.
 * Create/prepare/destroy outside an active GPU frame. Retain old CPU bodies
 * returned by world_replace until this renderer's final snapshot is submitted
 * and this owner destroyed. A replacement requires a new renderer. */
BkSelectionRender *bk_selection_render_create(BkRenderer *, BkResourceStore *,
                                              BkSelectionWorld *,
                                              char error[256]);
/* Also supports61: owns real bk3_18/poi.avi and D_moza.bmp surface binding.
 * Clock is signed process-elapsed ms, separate from game/face timers.
 * Video uses the explicit portable DIB/device policies in avi_texture.h. */
BkSelectionRender *bk_selection_render_create_at(BkRenderer *,
                                                 BkResourceStore *,
                                                 BkSelectionWorld *,
                                                 int32_t movie_clock_ms,
                                                 char error[256]);
/* Called ONLY at51ac5d's movie stage for a61 actor. No-op substitution for a
 * missing movie is forbidden. Owner must still refer to its current body. */
int bk_selection_render_movie_step(BkSelectionRender *, int32_t clock_ms,
                                   int32_t restart_clock_ms, char error[256]);
uint32_t bk_selection_render_movie_frame(const BkSelectionRender *);
void bk_selection_render_destroy(BkSelectionRender *);
/* Execute native mode1's stage/body publication and snapshot distinct lights,
 * geometry and queues. Camera must match the world's installed anchor. Fog
 * is explicit retained device state; this adapter does not guess entry resets.
 * No timeline/UI/audio update. prepare failure invalidates the next draw. */
int bk_selection_render_prepare(BkSelectionRender *, const BkMenuCamera *,
                                const BkFog *, char error[256]);
/* Caller selects viewport and draws selection UI after these two flushes. */
int bk_selection_render_draw(BkSelectionRender *, char error[256]);
int bk_selection_render_stats(const BkSelectionRender *, unsigned object,
                              BkActorRenderStats *);
#endif
