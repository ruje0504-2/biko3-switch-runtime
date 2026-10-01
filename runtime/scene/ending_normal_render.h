#ifndef BK_SCENE_ENDING_NORMAL_RENDER_H
#define BK_SCENE_ENDING_NORMAL_RENDER_H
#include "game/draw_dispatch.h"
#include "scene/actor_render.h"
#include "scene/ending_normal_assets.h"
#include "scene/ending_secondary_assets.h"
#include "scene/ending_tertiary_assets.h"
#include "scene/ending_selected_assets.h"
#include "scene/ending_auxiliary_assets.h"
#include "game/ending_reload.h"
#include "scene/ending_special_scene.h"
#include "scene/ending_audio.h"
#include "scene/lighting_registry.h"
typedef struct BkEndingNormalRender BkEndingNormalRender;
/*Real registry stages used by ending reload. RESET retains device light
 * state; SELECT reconstructs BK3_L groups; ENABLE is the historical enum
 * name for4a4438 ambient initialization, not an all-lights-on command.
 * Already prepared view descriptors remain immutable for pure redraw. */
int bk_ending_normal_render_relight(BkEndingNormalRender *,
                                    BkEndingReloadLight, char error[256]);
/*At stage replacement, preserve device state of retained background models
 * before the native RESET/SELECT/ambient sequence. Old view snapshots remain
 * immutable; the destination must not have captured a frame yet.*/
int bk_ending_normal_render_inherit_lighting(BkEndingNormalRender *,
                                            const BkEndingNormalRender *,
                                            char error[256]);
/*Value-only handoff across a pure-UI interval. The caller owns the retained
 * CPU background, so the old renderer and all other models may be destroyed.
 * Saving does not change an already captured frame. Restore precedes the
 * first prepare and the native registration/ambient-selection sequence.*/
int bk_ending_normal_render_save_lighting(const BkEndingNormalRender *,
                                         const BkModel *retained_model,
                                         BkRetainedLightState *, char error[256]);
int bk_ending_normal_render_restore_lighting(BkEndingNormalRender *,
                                            const BkRetainedLightState *, char error[256]);
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
/*Independent4D00FA CPU topology, sharing only the renderer's actual
 * queue/light/video/dual-view machinery. Actor IDs remain0/3, cameras1/2;
 * no auxiliary actor or BOM is synthesized. Borrow both material instances
 * and disjoint model/FAM vertex owners from the secondary assets. This
 * constructor includes the second GPU view required by4D9733/4D9898.
 * All prepare/draw/inspection operations below accept the returned owner. */
BkEndingNormalRender *
bk_ending_secondary_render_create(BkRenderer *, BkResourceStore *,
                                   BkEndingSecondaryAssets *,
                                   int32_t movie_clock, char error[256]);
/*4d2320 actual primary, two optional auxiliaries, combined BOM, independent
 * retained background and original video texture. Both views own independent
 * geometry for every body model. The original root dispatcher still selects
 * the primary tree: reparented auxiliary nodes do not add fabricated passes.
 * Sources are pinned at the primary draw when a retained background precedes
 * the new actors. Physical registry indices come from assets_registry(). */
BkEndingNormalRender *
bk_ending_tertiary_render_create(BkRenderer *, BkResourceStore *,
                                  BkEndingTertiaryAssets *,
                                  int32_t movie_clock, char error[256]);
/*4D1025 selected-ending topology: primary0, camera tracks1/2 and background3,
 *with no upper/lower actor or BOM. The renderer owns only GPU views and borrows
 *the selected asset owner.*/
BkEndingNormalRender *
bk_ending_selected_render_create(BkRenderer *, BkResourceStore *,
                                  BkEndingSelectedAssets *,
                                  int32_t movie_clock, char error[256]);
/*4D39E6 topology: the bk3_12 primary and camera tracks borrow the retained
 * outer background. It has no BOM or synthetic upper/lower actor. */
BkEndingNormalRender *
bk_ending_auxiliary_render_create(BkRenderer *, BkResourceStore *,
                                   BkEndingAuxiliaryAssets *,
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
 * state changes. In ordinary asset/BOM topology, auxiliary sources write one
 * skinned primary mesh, never a source. Secondary assets use only their own
 * primary geometry, without an auxiliary or BOM. No time/video advance or
 * frame allocation. Store the original End/Viewport/Clear/Begin ordering;
 * draw executes the two views with actual viewport/depth clear and restores
 * main viewport. Missing/invalid state aborts; no usable partial snapshot. */
int bk_ending_normal_render_prepare_special(BkEndingNormalRender *,
                                            const BkEndingSpecialRenderInput *,
                                            char error[256]);
typedef struct {
  BkDrawDispatchInput roots;
  int32_t event_mode; /* live721ec4 at the draw boundary */
  BkEndingSpecialRenderInput scene;
  BkEndingAudio *audio;
  int32_t *duck_transition; /* borrowed process719c5c, never reset here */
  int32_t voice_master;
  float seconds;
  int32_t movie_clock, movie_restart_clock;
} BkEndingEventRenderInput;
/* Actual51c736/4d9733 service adapter: video, conditional/forced second
 * view, real speech1 status and speech0 volume, then regular fallback only
 * when native dispatch requests it. scene.dispatch is ignored: the mutable
 * descriptor is created by the original root selector for this call.
 * Retained scalar owners and all special bindings remain caller-owned.
 * Missing owners needed by the chosen branch fail; any late failure makes
 * BOTH snapshots unusable while retaining already-executed CPU/audio effects.
 * Does not advance actor controllers, manufacture entry state or present.
 * Pure draw reuses its snapshots and never queries/changes audio or video. */
int bk_ending_normal_render_prepare_event(BkEndingNormalRender *,
                                          const BkEndingEventRenderInput *,
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
 * Normal primary materials may be NULL for original immutable values;
 * secondary materials always use the exact asset-owned instance. A new normal
 * scene's visible last-loaded background publishes the connected forest before
 * actor walks. A retained background precedes the new actors: geometry is
 * captured after each original draw walk, with a separate auxiliary snapshot
 * for the primary BOM pass. Later publications cannot overwrite that source.
 * Every ending topology permits a hidden background, retaining its empty
 * flush and delaying upload until the primary walk publishes its cache.
 * Distinct flushes retain independent lights/queues. No allocations per frame.
 * Failure invalidates draw; stateful forest publication is not rolled back. */
int bk_ending_normal_render_prepare_regular(
    BkEndingNormalRender *, const BkDrawDispatch *, const BkMenuCamera *,
    const BkFog *, const BkMaterialPose *primary_materials,
    const int32_t *bom_disabled, size_t count, char error[256]);
int bk_ending_normal_render_draw(BkEndingNormalRender *, char error[256]);
/* Borrowed inspection only; actor IDs match the CPU owner: normal0/1/4,
 * independent secondary0/3; third uses its actual registered primary,
 * auxiliaries and background. Camera IDs never map to drawable geometry. */
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
