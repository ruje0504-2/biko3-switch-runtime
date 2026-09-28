#ifndef BK_SCENE_ACTOR_RENDER_H
#define BK_SCENE_ACTOR_RENDER_H
#include "model/material_pose.h"
#include "model/morph_group.h"
#include "render/renderer.h"
#include "scene/eye_assets.h"
#include "scene/face_assets.h"
#include "world/actor_pose.h"
typedef struct BkActorRender BkActorRender;
typedef struct {
  uint32_t submitted_instances, visible_instances, skinned_parts, morph_parts;
  uint64_t uploaded_vertices;
} BkActorRenderStats;
/* Owns GPU textures/meshes and CPU skin scratch. Borrows model/renderer until
 * destruction; optional eye assets are also borrowed until destruction.
 * Resource store is used only during construction. No gameplay,
 * actor publication, clock advance or invented animation occurs here. */
BkActorRender *bk_actor_render_create(BkRenderer *renderer,
                                      BkResourceStore *resources,
                                      const char *pack, const BkModel *model,
                                      const BkEyeAssets *eyes, char error[256]);
void bk_actor_render_destroy(BkActorRender *actor);
/* Independent geometry/palette/material/queue state for another view of the
 * same model. Share retained immutable textures, eye surface and sort IDs;
 * copy the current borrowed surface override. No archive access or duplicate
 * texture upload. Either actor may be destroyed first. Model/eyes/renderer
 * and any borrowed override still must outlive every view. Later overrides,
 * callbacks and prepare revisions are independent. Outside active frames. */
BkActorRender *bk_actor_render_create_view(const BkActorRender *,
                                           char error[256]);
/* Replace one model texture's GPU surface (original texture object+70),
 * retaining its filename alpha hint and original texture sorting identity.
 * Borrowed surface must belong to this renderer and outlive prepared draws.
 * NULL with BK_MODEL_NONE clears it. Eye-slot overrides still take precedence.
 * Outside active frames; invalidates prepared actors/batches. No ownership
 * transfer: the actor retains/destroys its original textures, not the loan. */
int bk_actor_render_texture_surface(BkActorRender *, uint32_t texture_index,
                                    BkTexture *borrowed, char error[256]);
/* Outside active GPU frames. Snapshot the actor's already published WORLD
 * cache, hidden subtrees, current material values and optional face vertices.
 * Then ENVL deformation, lit vertex upload and original two-pass sorting.
 * Actor must borrow the same model; materials/face must belong to this actor.
 * NULL material pose/face uses immutable model values. No per-frame allocation.
 * Unchanged rigid geometry reuses its GPU vertices; matrices/lights/queue
 * still update. Material edits and dropping MORP restore/upload as needed.
 * Failure invalidates the prepared draw; retrying prepare rebuilds everything.
 * Caller must set the light viewer to match view before renderer.begin. */
int bk_actor_render_prepare(BkActorRender *actor, const BkActorPose *pose,
                            const BkMaterialPose *materials,
                            const BkFaceAssets *face, const float view[16],
                            const float projection[16], char error[256]);
/* Whole-model MORP instead of FAM subsets, sharing the identical material,
 * skinning, upload and traversal paths. Used by loaded BOM auxiliaries. */
int bk_actor_render_prepare_morph(BkActorRender *, const BkActorPose *,
                                  const BkMaterialPose *, const BkMorphGroup *,
                                  const float view[16],
                                  const float projection[16], char error[256]);
/* Borrow a checked GPU mesh until actor destruction. Transfers may retain
 * the GPU allocation, but callback users must still outlive the actor handle.
 * Revision is0 until successful prepare; later mutation invalidates snapshots.
 */
BkGpuMesh *bk_actor_render_mesh(BkActorRender *, const BkModel *,
                                uint32_t submesh);
uint64_t bk_actor_render_revision(const BkActorRender *);
typedef int (*BkActorMeshCallback)(void *, uint32_t submesh, char error[256]);
/* One owner per actor. Install/remove outside frames, before prepare.
 * Remove with NULL callback and the same context. Invoked on each queued,
 * nonhidden mesh draw BEFORE the material-alpha early out. No implicit draw
 * deduplication. The callback must not mutate/reprepare this actor. */
int bk_actor_render_mesh_callback(BkActorRender *, BkActorMeshCallback, void *,
                                  char error[256]);
/* Active frame only; lighting is borrowed. Skinned vertices use identity
 * world; rigid/MORP parts use the cached owning frame matrix. */
int bk_actor_render_draw(BkActorRender *actor, BkLightSet *lights,
                         char error[256]);
int bk_actor_render_stats(const BkActorRender *actor, BkActorRenderStats *out);
typedef struct BkActorRenderBatch BkActorRenderBatch;
/* One original42aa47 flush over several prepared models. Borrows actors until
 * draw completes, including their GPU buffers. Distinct light passes require
 * distinct batch/light snapshots. Ordinary texture order uses stable GPU
 * resource identities rather than original process addresses. */
BkActorRenderBatch *
bk_actor_render_batch_create(BkRenderer *, uint32_t capacity, char error[256]);
void bk_actor_render_batch_destroy(BkActorRenderBatch *);
/* actors are in scene traversal order; duplicate occurrences are retained.
 * Gather unsorted model traversals before partition/sort. Failure invalidates
 * the batch. No allocation or GPU mutation during prepare. */
int bk_actor_render_batch_prepare(BkActorRenderBatch *, BkActorRender *const *,
                                  uint32_t actor_count, char error[256]);
typedef struct {
  BkActorRender *actor;
  uint32_t frame;
} BkActorRenderVisit;
/* Ordered frame-level submissions from a global frame-tree walk. Only each
 * listed frame's meshes are collected; children require their own visit.
 * Source index returned by batch_item is the visit index for this variant.
 * Duplicates are retained, including separate visits to the same frame.
 * Inputs must come from already published/prepared actors. */
int bk_actor_render_batch_prepare_visits(BkActorRenderBatch *,
                                         const BkActorRenderVisit *,
                                         uint32_t visit_count, char error[256]);
/* Preflight rejects actors prepared again since the batch snapshot, before
 * submitting any draw. The owner must not destroy borrowed actors early. */
int bk_actor_render_batch_draw(BkActorRenderBatch *, BkLightSet *,
                               char error[256]);
uint32_t bk_actor_render_batch_count(const BkActorRenderBatch *);
/* Inspect ordered submission identity without drawing. */
int bk_actor_render_batch_item(const BkActorRenderBatch *, uint32_t index,
                               uint32_t *actor_index, uint32_t *frame,
                               uint32_t *submesh);
#endif
