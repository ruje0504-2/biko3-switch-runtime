#ifndef BK_GAME_NPC_HEAD_H
#define BK_GAME_NPC_HEAD_H
#include "world/visibility.h"
typedef struct {
  float actor_head_world[16], actor_head_local[16], torso_local[16];
  float player_head[3], player_position[3], actor_yaw;
  int32_t actor_kind;
} BkNpcHeadInput;
typedef struct {
  BkSightSegment sight;
  float bearing, facing;
} BkNpcHeadState;
/* Actual 0x4fca12..0x4fcc57 stage, after route movement and before ground.
 * World inputs are published/cached head poses, not newly sampled locals.
 * Actor kinds1/2 use the local head+torso roll and updated body yaw;
 * other kinds use head world yaw. A zero world yaw becomes360 in native.
 * No animation advancement or world publication occurs. */
int bk_npc_head_update(BkNpcHeadState *state, const BkNpcHeadInput *input,
                       char error[256]);
#endif
