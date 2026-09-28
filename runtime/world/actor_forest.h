#ifndef BK_WORLD_ACTOR_FOREST_H
#define BK_WORLD_ACTOR_FOREST_H
#include "world/actor_pose.h"
#include "world/frame_tree.h"
typedef struct BkActorForest BkActorForest;
/* Owns topology/scratch; borrows distinct mutable actor poses until destroyed.
 * Nodes0/1 are global root/active camera anchors. Model frame IDs follow in
 * caller order. Import each model's file-order internal edges; leave its
 * roots detached until the scene loader explicitly attaches them in native
 * creation order. Camera1 initially is the first global child. No root order
 * is guessed from the actor array. Single-threaded, non-reentrant. */
BkActorForest *bk_actor_forest_create(BkActorPose *const *, uint32_t actors,
                                      char error[256]);
void bk_actor_forest_destroy(BkActorForest *);
/* Register one additional borrowed pose, importing only its internal edges.
 * Preserve existing actor/node IDs, all topology, anchors and cached view.
 * Never attach its root, publish matrices or consume any timeline. Allocation
 * and validation failures leave forest, poses and output index unchanged.
 * Success invalidates borrowed tree/visits; the forest handle stays stable. */
int bk_actor_forest_append(BkActorForest *, BkActorPose *, uint32_t *actor,
                           char error[256]);
uint32_t bk_actor_forest_node(const BkActorForest *, uint32_t actor,
                              uint32_t frame);
/* Return borrowed actor/frame binding; anchors0/1 have no actor. */
int bk_actor_forest_binding(const BkActorForest *, uint32_t node,
                            uint32_t *actor, uint32_t *frame);
const BkFrameTree *bk_actor_forest_tree(const BkActorForest *);
const float *bk_actor_forest_world(const BkActorForest *, uint32_t node);
/*425904 over the live forest, including reparented cross-model children.
 * First exact DFS match after removing the first name prefix through space.
 * Empty queries remain meaningful. Missing -> BK_FRAME_NONE. Anchors have no
 * asset name; a search reaching either anchor is rejected. */
int bk_actor_forest_find(const BkActorForest *, uint32_t root, const char *name,
                         uint32_t *node, char error[256]);
/*423a99 over the live subtree, including its root. Does not publish worlds,
 * change clocks or use stale model ancestry. Raw hidden values retained. */
int bk_actor_forest_visibility(BkActorForest *, uint32_t root, uint32_t hidden,
                               char error[256]);
/* Independent local/world/held parent snapshots for anchors0/1. Commit keeps
 * visibility and parent cache; rejects a stale parent and invalid matrices.
 * Does not recompute world from local (native aim preserves its world result).
 */
int bk_actor_forest_anchor_reference(const BkActorForest *, uint32_t node,
                                     BkNodeReference *, char error[256]);
int bk_actor_forest_commit_anchor_reference(BkActorForest *, uint32_t node,
                                            const BkNodeReference *,
                                            char error[256]);
/*423e72: derive/upload the view from the connected camera's LOCAL path.
 * Hidden nodes do not stop this prepass. No world/parent cache is published;
 * disconnected camera holds the previous view. Singular paths fail atomically.
 */
int bk_actor_forest_camera_publish(BkActorForest *, char error[256]);
/*42403c semantics for anchors0/1: local and own world change immediately
 * using held parent cache. Descendants remain held. No camera-view upload. */
int bk_actor_forest_anchor(BkActorForest *, uint32_t node,
                           const float local[16], uint32_t hidden,
                           char error[256]);
/*422015: append/reparent plus full connected-cache refresh ignoring hidden.
 * Validate pending topology and every composition before changing any pose.
 * Same-parent attach is a true no-op. Detach does not refresh caches. */
int bk_actor_forest_attach(BkActorForest *, uint32_t parent, uint32_t child,
                           char error[256]);
int bk_actor_forest_detach(BkActorForest *, uint32_t parent, uint32_t child,
                           char error[256]);
int bk_actor_forest_refresh(BkActorForest *, char error[256]);
/* Camera prepass, selected traversal, then publication. Keeps timeline,
 * physics placement and animation locals held. Includes publish-only visits;
 * renderer consumes only submit!=0 and bindings with meshes. Results are
 * borrowed until next successful mutation/draw; pose mutation by another
 * owner requires another draw before building render snapshots. Missing
 * camera path preserves the prior view; singular camera fails atomically.
 * Billboard/light/projection side effects are still separate services. */
int bk_actor_forest_draw(BkActorForest *, uint32_t target,
                         const BkFrameVisit **visits, uint32_t *count,
                         char error[256]);
const float *bk_actor_forest_view(const BkActorForest *);
#endif
