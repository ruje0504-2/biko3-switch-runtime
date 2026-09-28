#ifndef BK_WORLD_ACTOR_POSE_H
#define BK_WORLD_ACTOR_POSE_H
#include "model/playback.h"
#include "world/node_reference.h"
#include "world/placement.h"
/* CPU pose lifecycle, not actor AI/physics/mesh skinning. Assets are borrowed.
 * Root setters publish the root immediately; children retain their last
 * traversal values until publish. This distinction is required for cameras. */
typedef struct BkActorPose BkActorPose;
typedef struct {
  uint32_t frame, hidden;
} BkActorVisibilityEdit;
/* head_node=NULL makes a headless auxiliary instance (e.g. mesh shadow);
 * head() then returns NULL. Other pose/timeline/cache semantics are identical.
 */
BkActorPose *bk_actor_pose_create(const BkModel *model, const BkClipSet *clips,
                                  uint32_t root, const char *head_node,
                                  const float position[3], float yaw_degrees,
                                  unsigned initial_clip, int instant,
                                  char error[256]);
void bk_actor_pose_destroy(BkActorPose *actor);
/* As original prop loading: preserve authored timeline without requesting
 * or sampling anything. Includes static/empty XAN descriptors. */
BkActorPose *bk_actor_pose_create_loaded(const BkModel *, const BkClipSet *,
                                         uint32_t root, const float position[3],
                                         float yaw, char error[256]);
const BkModel *bk_actor_pose_model(const BkActorPose *actor);
/* Apply physics/route placement without consuming a clip request or clock.
 * Root local/world change immediately; child/head world caches stay held.
 * Failed placement preserves all pose and playback state. */
int bk_actor_pose_place(BkActorPose *actor, const float position[3],
                        float yaw_degrees, char error[256]);
/* Copy an explicit anchor world (+c0) instead of rebuilding it from yaw.
 * Local is world * inverse(cached parent), as 42407a. Singular cached
 * parents are rejected (the original fallback corrupts its parent cache).
 * Position/yaw metadata remain distinct; no child publication or sampling. */
int bk_actor_pose_place_exact(BkActorPose *, const BkActorPlacement *,
                              char error[256]);
/* 42403c: set the root LOCAL and immediately derive root world from its
 * last traversed parent cache. Descendants and parent cache stay held.
 * Position metadata follows root world; yaw metadata is retained. */
int bk_actor_pose_root_local(BkActorPose *, const float local[16],
                             char error[256]);
/* request=-1 keeps the current request. Time is already scaled by caller:
 * NPC/player action policies differ and must not be inferred here. Failed
 * steps preserve request, clock, placement and all published matrices.
 * A hidden root still accepts the request and placement but skips the
 * scheduler and SRT submission entirely, including zero-duration steps. */
int bk_actor_pose_step(BkActorPose *actor, const float position[3],
                       float yaw_degrees, int request, float animation_seconds,
                       char error[256]);
int bk_actor_pose_step_mode(BkActorPose *actor, const float position[3],
                            float yaw_degrees, int request,
                            BkClipRequestMode mode, float animation_seconds,
                            char error[256]);
/* XAN update without rewriting an exact/root anchor already installed. */
int bk_actor_pose_advance(BkActorPose *, int request, float seconds,
                          char error[256]);
/* Explicit401d24/401f71 selection without entering scheduler or publishing. */
int bk_actor_pose_select(BkActorPose *, unsigned slot, int instant,
                         char error[256]);
/* 401b0a only: preserve locals/world caches and scheduler first-step state.
 * This differs from advance(...,0), which still enters the scheduler. */
int bk_actor_pose_request(BkActorPose *, unsigned slot, char error[256]);
/*4018c8/configured or401b0a/ten ticks, without entering4026fe. */
int bk_actor_pose_request_mode(BkActorPose *, unsigned slot, BkClipRequestMode,
                               char error[256]);
int bk_actor_pose_edit_clips(BkActorPose *, const BkClipEdit *, size_t count,
                             char error[256]);
int bk_actor_pose_clip_link(const BkActorPose *, unsigned slot, int32_t *chain,
                            int32_t *next);
void bk_actor_pose_publish(BkActorPose *actor);
/* Publish beneath an explicit external parent (e.g. snow under camera).
 * Refreshes visible-node world/parent caches without advancing animation.
 * Matrix must be finite affine. Failure leaves all published caches held.
 * Plain publish remains an independent-root traversal. */
int bk_actor_pose_publish_under(BkActorPose *, const float parent_world[16],
                                char error[256]);
/* Publish one visited frame into the native world/parent caches. The global
 * frame tree decides whether to visit hidden descendants and supplies the
 * actual parent (which can belong to another model). No timeline/local/root
 * placement metadata changes. Frame-local * parent-world uses the same
 * per-node rounding as the ordinary pose publisher. Finite constant-W and
 * projective parents are retained. Failure leaves both caches unchanged. */
int bk_actor_pose_publish_node(BkActorPose *, uint32_t frame,
                               const float parent_world[16], char error[256]);
/* 0x423a99: ordered subtree assignments, committed atomically. Values stay
 * raw32-bit; any nonzero hidden value stops traversal AFTER updating that
 * node's own world matrix. Descendant caches retain their previous values. */
int bk_actor_pose_visibility(BkActorPose *actor,
                             const BkActorVisibilityEdit *edits, size_t count,
                             char error[256]);
int bk_actor_pose_hidden(const BkActorPose *actor, uint32_t frame,
                         uint32_t *hidden);
/* Borrow contiguous published cache (frame order). Read-only; contents may
 * change on the next pose mutation and storage expires with the actor. */
const float *bk_actor_pose_world(const BkActorPose *, size_t *float_count);
const float *bk_actor_pose_frame(const BkActorPose *actor, uint32_t frame);
/* Submitted locals are current immediately after step, independently of the
 * published/cached world matrices returned by frame/head. */
const float *bk_actor_pose_local(const BkActorPose *actor, uint32_t frame);
const float *bk_actor_pose_head(const BkActorPose *actor);
const BkActorPlacement *bk_actor_pose_placement(const BkActorPose *actor);
int bk_actor_pose_state(const BkActorPose *actor, BkClipState *state);
int bk_actor_pose_timing(const BkActorPose *actor, unsigned slot,
                         BkClipTiming *timing);
int bk_actor_pose_loops(const BkActorPose *, unsigned slot, int32_t *loops);
/* Cached parent world from this node's last traversal, NOT its parent's
 * current matrix. Root setters and procedural eye updates do not refresh it. */
const float *bk_actor_pose_parent_world(const BkActorPose *actor,
                                        uint32_t frame);
/* 4a0823 procedural update: read cached eye/parent worlds, then commit both
 * locals and only these eyes' published worlds. Children keep their caches.
 * Either BK_MODEL_NONE eye skips the whole operation. Failure is atomic. */
int bk_actor_pose_eyes(BkActorPose *actor, const uint32_t frames[2],
                       const float target_world[16], uint32_t texture_mode,
                       uint32_t variant, float pitch_limit, float yaw_limit,
                       char error[256]);
/* BOM4a4dc3: align a non-root node to a reference cached world with zero
 * reference-space position and forwardZ/upY. No topology/descendant change.
 * Local edits persist until that node's next genuine ANIM submission. */
int bk_actor_pose_align_reference(BkActorPose *, uint32_t frame,
                                  const float reference[16], char error[256]);
/* Procedural non-root node snapshot/commit. Commit retains the cached parent,
 * all descendants and clock, and rejects a changed parent since snapshot.
 * Local and own world may differ from a full hierarchy publication. */
int bk_actor_pose_node_reference(const BkActorPose *, uint32_t frame,
                                 BkNodeReference *out, char error[256]);
int bk_actor_pose_commit_reference(BkActorPose *, uint32_t frame,
                                   const BkNodeReference *, char error[256]);
/*40168c request; does not sample or publish. */
int bk_actor_pose_request_active(BkActorPose *, unsigned slot, char error[256]);
/*4021a1 clock/ANIM stage. Hidden root sets submitted=0 and leaves sample and
 * pose untouched; otherwise submitted=1 with the source for later MORP and
 * material services. No world/parent publication or implicit root placement.
 * This API alone does not implement all4021a1 media/effect calls. */
int bk_actor_pose_advance_frame(BkActorPose *, BkClipSample *sample,
                                int *submitted, char error[256]);
/*4026fe with explicit effect sample; no request or world publication.
 * Hidden actors return submitted0 and preserve the output and clock. */
int bk_actor_pose_advance_effects(BkActorPose *, float seconds,
                                  BkPlaybackEffects *, int *submitted,
                                  char error[256]);
#endif
