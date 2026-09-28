#ifndef BK_GAME_NPC_SPATIAL_H
#define BK_GAME_NPC_SPATIAL_H
#include "game/npc_ai.h"
#include "game/npc_head.h"
#include "game/npc_route.h"
#include "game/npc_scene.h"
#include "world/placement.h"
typedef struct {
  BkRouteMotion path;
  BkNpcAiState ai;
  BkNpcHeadState head;
  float alpha;
  uint8_t visible;
  char surface_name[260];
} BkNpcSpatialState;
typedef struct {
  int32_t group, area, player_action, suppressed_actions[6];
  int32_t active_clip, background_clip, short_range_action;
  float player_position[3], player_direction[3], player_head[3];
  float head_world[16], head_local[16], torso_local[16];
  /* +0x298 is added to the corrected body Y to produce +0x2a8. */
  float vertical_offset;
  int8_t interaction_df, interaction_e0;
  const char *excluded_surface;
} BkNpcSpatialInput;
typedef struct {
  BkNpcAiEffects ai;
  BkNpcRouteEffects route;
  BkActorPlacement placement;
  float vertical_position;
} BkNpcSpatialEffects;
/* Original 0x4fc8f9..0x4fcdb6 ordering: AI, route/actions, cached head,
 * cursor-zero startup, scene queries and root placement. Shared state,
 * RNG and all outputs commit together only after every stage succeeds.
 * Matrices are supplied from the pre-update cache/local pose, while body
 * yaw is the post-route value. No animation sampling/publication occurs.
 * This stops before the unported 0x4fd796 action/media dispatch. Caller must
 * consume audio commands in AI-then-route order and apply the placement;
 * this API alone is not a complete NPC or gameplay frame. */
int bk_npc_spatial_step(BkNpcSpatialState *state,
                        BkNpcInteractionState *interaction,
                        uint32_t *random_state, const BkRoute *route,
                        const BkCollision *collision,
                        const BkNpcSpatialInput *input, float seconds,
                        uint32_t now_ms, BkNpcSpatialEffects *effects,
                        char error[256]);
#endif
