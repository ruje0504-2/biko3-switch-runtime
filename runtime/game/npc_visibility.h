#ifndef BK_GAME_NPC_VISIBILITY_H
#define BK_GAME_NPC_VISIBILITY_H
#include "world/actor_pose.h"
typedef struct {
  uint8_t hidden;
  int8_t interface_mode, phase;
  int32_t area, behavior;
} BkNpcVisibilityInput;
typedef struct {
  uint32_t root_hidden, marks[4];
  int override_marks;
} BkNpcVisibility;
/* 0x4fc37f..0x4fc6a3: root visibility, then status marker overrides.
 * Root hidden stops timeline/SRT advancement and descendant publication;
 * action requests and fade still execute.
 * The optional accessory root receives the same root_hidden separately.
 * Mark order is mark_02,mark_01,mark_00,mark_03. Unknown phases/behaviors in
 * interface mode2/area<8 preserve the preceding root-wide assignment. */
int bk_npc_visibility_plan(BkNpcVisibility *out,
                           const BkNpcVisibilityInput *input);
int bk_npc_visibility_apply(BkActorPose *actor, uint32_t root,
                            const uint32_t marks[4],
                            const BkNpcVisibilityInput *input, char error[256]);
#endif
