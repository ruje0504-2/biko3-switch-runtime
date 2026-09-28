#ifndef BK_ACTOR_POSE_INTERNAL_H
#define BK_ACTOR_POSE_INTERNAL_H
#include "world/actor_pose.h"
/* Only actor_forest calls this after validating the entire walk. Registry,
 * frame bounds, parent and world matrices are already checked; no callbacks
 * or mutation may intervene. Retains per-node cache publication order. */
void bk_actor_pose_commit_composed(BkActorPose *, uint32_t frame,
                                   const float world[16],
                                   const float parent[16]);
/* Forest visibility uses the live cross-model topology, not the model's
 * original subtree. Called only with a validated binding. */
void bk_actor_pose_commit_hidden(BkActorPose *, uint32_t frame, uint32_t hidden);
#endif
