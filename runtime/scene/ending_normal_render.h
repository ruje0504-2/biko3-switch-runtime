#ifndef BK_SCENE_ENDING_NORMAL_RENDER_H
#define BK_SCENE_ENDING_NORMAL_RENDER_H
#include "game/draw_dispatch.h"
#include "scene/actor_render.h"
#include "scene/ending_normal_assets.h"
#include "scene/ending_special_scene.h"
typedef struct BkEndingNormalRender BkEndingNormalRender;
/* Own GPU actors, per-flush queues/lights, BOM callbacks and poi.avi. Borrow
 * the exact CPU owner, whose outer background must already be loaded. Destroy
 * this renderer before the CPU owner. Create/prepare/destroy outside frames.
 * No timeline, UI, event dispatch or special4d9898 scene is implied. */
BkEndingNormalRender *bk_ending_normal_render_create(BkRenderer *,
                                                     BkResourceStore *,
                                                     BkEndingNormalAssets *,
                                                     int32_t movie_clock,
                                                     char error[256]);
void bk_ending_normal_render_destroy(BkEndingNormalRender *);
/* Also reserve independent primary/auxiliary geometry and two light/queue
 * snapshots for4d9898. Textures, video and the live light registry are shared.
 * No second background mesh is allocated. Ordinary create remains sufficient
 * for regular-only callers; special prepare requires this constructor. */
BkEndingNormalRender *
bk_ending_normal_render_create_special(BkRenderer *, BkResourceStore *,
                                       BkEndingNormalAssets *,
                                       int32_t movie_clock, char error[256]);
typedef struct {
  const BkEndingSpecialBindings *bindings;
  BkDrawDispatch *dispatch;
  BkMenuCamera *camera;
  const BkFog *fog;
  const BkMaterialPose *primary_materials;
  const BkEndingSpecialMaterial *materials;
  size_t material_count;
  const int32_t *bom_disabled;
  size_t bom_count;
  BkViewport main_viewport, special_viewport;
} BkEndingSpecialRenderInput;
/* Run actual CPU special-scene operations and capture each view before its
 * state changes. Require the ordinary asset/BOM topology: auxiliary sources
 * write one skinned primary mesh, never a source. No time/video advance or
 * frame allocation. Store the original End/Viewport/Clear/Begin ordering;
 * draw executes the two views with actual viewport/depth clear and restores
 * main viewport. Missing/invalid state aborts; no usable partial snapshot. */
int bk_ending_normal_render_prepare_special(BkEndingNormalRender *,
                                            const BkEndingSpecialRenderInput *,
                                            char error[256]);
/* Actual4e1473 video service; separate from a pure redraw/geometry prepare. */
int bk_ending_normal_render_movie_step(BkEndingNormalRender *, int32_t now,
                                       int32_t restart, char error[256]);
uint32_t bk_ending_normal_render_movie_frame(const BkEndingNormalRender *);
const BkImage *
bk_ending_normal_render_movie_image(const BkEndingNormalRender *);
/* ONLY the finalized regular4a4701 descriptor, after the caller's required
 * event services. Accept normal mode1 background/primary/optional auxiliary,
 * or no objects for event7. This does not replace draw_dispatch_run.
 * Camera must match the forest anchor; fog is retained explicit device state.
 * Primary materials may be NULL for original immutable values. Each actor
 * is prepared once per frame; a visible last-loaded
 * background root is required. The first background walk
 * publishes the full connected forest before the later actor-root walks.
 * Distinct flushes retain independent lights/queues. No allocations per frame.
 * Failure invalidates draw; stateful forest publication is not rolled back. */
int bk_ending_normal_render_prepare_regular(
    BkEndingNormalRender *, const BkDrawDispatch *, const BkMenuCamera *,
    const BkFog *, const BkMaterialPose *primary_materials,
    const int32_t *bom_disabled, size_t count, char error[256]);
int bk_ending_normal_render_draw(BkEndingNormalRender *, char error[256]);
/* Borrowed inspection only; actor indices match CPU owner:0,1,4. */
BkGpuMesh *bk_ending_normal_render_mesh(BkEndingNormalRender *, unsigned actor,
                                        uint32_t submesh);
BkGpuMesh *bk_ending_normal_render_view_mesh(BkEndingNormalRender *,
                                             unsigned view, unsigned actor,
                                             uint32_t submesh);
/* Inspection: special passes are regular passes followed by secondary ones.
 * Returns UINT32_MAX for an invalid pass. The per-view buffers are independent.
 */
unsigned bk_ending_normal_render_pass_view(const BkEndingNormalRender *,
                                           unsigned pass);
uint32_t bk_ending_normal_render_pass_count(const BkEndingNormalRender *);
uint32_t bk_ending_normal_render_queue_count(const BkEndingNormalRender *,
                                             unsigned pass);
int bk_ending_normal_render_queue_item(const BkEndingNormalRender *,
                                       unsigned pass, uint32_t item,
                                       uint32_t *actor, uint32_t *frame,
                                       uint32_t *submesh);
#endif
