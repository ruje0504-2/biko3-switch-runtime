#ifndef BK_WORLD_FRAME_TREE_H
#define BK_WORLD_FRAME_TREE_H
#include <stdint.h>
#define BK_FRAME_NONE UINT32_MAX
#define BK_FRAME_TREE_LIMIT 65536u
typedef struct BkFrameTree BkFrameTree;
typedef struct {
  uint32_t node, submit;
} BkFrameVisit;
/* Registry for one scene assembly; node0 is the native global root.
 * All other nodes start detached. IDs retain identity for this tree's entire
 * lifetime. Borrowed model/pose bindings live in scene, not in this topology.
 * A larger registry may import its old prefix; no pointer/ID recycling. */
BkFrameTree *bk_frame_tree_create(uint32_t count);
void bk_frame_tree_destroy(BkFrameTree *);
/* Copy connectivity into an equally sized registry. Scratch is not copied.
 * Used to validate a scene reparent before publishing any cache changes. */
int bk_frame_tree_copy(BkFrameTree *destination, const BkFrameTree *source);
/* Copy source into the prefix of an equal/larger registry. Retain destination
 * suffix connectivity, which must have no edges crossing the prefix boundary.
 * Reject crossing edges before modifying anything; scratch is not copied. */
int bk_frame_tree_copy_prefix(BkFrameTree *destination,
                              const BkFrameTree *source);
uint32_t bk_frame_tree_count(const BkFrameTree *);
uint32_t bk_frame_tree_parent(const BkFrameTree *, uint32_t node);
uint32_t bk_frame_tree_first(const BkFrameTree *, uint32_t node);
uint32_t bk_frame_tree_next(const BkFrameTree *, uint32_t node);
/*422015 appends to the new parent's tail, unlinks the old parent, and asks
 * for423be2 refresh of EVERY connected node, even hidden ones. Same parent
 * is a no-op (refresh=0). Cycles/self-parenting are rejected atomically.
 * This topology API does not publish matrices; caller must consume refresh
 * with refresh_walk and compose/publish poses before later consumers. */
int bk_frame_tree_attach(BkFrameTree *, uint32_t parent, uint32_t child,
                         int *refresh);
/*4222d4: wrong parent or absent child edge is a no-op. No world refresh. */
int bk_frame_tree_detach(BkFrameTree *, uint32_t parent, uint32_t child);
/*423be2 traversal: exclude global root, include all connected descendants,
 * ignoring hidden flags. Matrix base is global root's cached WORLD. */
int bk_frame_tree_refresh_walk(BkFrameTree *, BkFrameVisit *, uint32_t capacity,
                               uint32_t *count);
/*42261a/42273b traversal, with billboard/projection/light side effects outside
 * this API. hidden contains registry_count values. Matrix base is global
 * root LOCAL. A hidden requested node skips traversal completely. Otherwise
 * each visited node publishes itself even when hidden, then hidden prunes
 * descendants. submit marks the native selected-root latch (hidden nodes
 * never submit). A parent's sibling loop stops after its direct requested
 * child; the latch remains set while unwinding more distant ancestors.
 * detached targets still allow preceding connected nodes to be published,
 * without submissions. Camera prepass423e72 is separate and ignores hidden.
 * Capacity must be at least registry_count-1; validation precedes output.
 * Walk methods use owned scratch; not reentrant. No allocations per walk. */
int bk_frame_tree_draw_walk(BkFrameTree *, uint32_t target,
                            const uint32_t *hidden, BkFrameVisit *,
                            uint32_t capacity, uint32_t *count);
/* Camera prepass: connected path from root0 (excluded) to camera (included),
 * regardless of hidden. Detached camera returns an empty path. Path order
 * is parent first so caller can use original per-level matrix rounding. */
int bk_frame_tree_camera_path(BkFrameTree *, uint32_t camera, uint32_t *path,
                              uint32_t capacity, uint32_t *count);
#endif
