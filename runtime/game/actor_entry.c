#include "game/actor_entry.h"
#include <math.h>
#include <string.h>
int bk_entry_player_view_reset(BkPlayerView *state, const float origin[3]) {
  BkCameraFollowPose pose;
  if (!state || !bk_game_entry_camera_pose(&pose, origin))
    return 0;
  state->pose = pose;
  state->yaw = state->pitch = 0;
  state->target_distance = state->distance = 40;
  memcpy(state->matrix, pose.world, sizeof(state->matrix));
  return 1;
}
int bk_player_entry_placement(BkActorPlacement *out,
                              const BkEntryRequest *request) {
  BkEntrySelection selection;
  if (!out || !bk_game_entry_select(&selection, request))
    return 0;
  static const float returns[5][4] = {{68, 0, -99, 180},
                                      {114, 0, -161, 60},
                                      {-77, 0, 29, 0},
                                      {27, 0, -254, 0},
                                      {120, -10, 11, 95}};
  const float *position = selection.player_position;
  float yaw = selection.player_yaw;
  if (request->previous_flow == 0x48) {
    position = returns[request->group];
    yaw = position[3];
  }
  return bk_actor_placement(out, position, yaw);
}
int bk_player_entry_reset(BkPlayerControl *state, int32_t actions[21],
                          uint8_t collected[BK_ITEM_TYPES],
                          const BkEntryRequest *request, float base_head) {
  BkActorPlacement placement;
  float vertical = (float)((double)base_head + 10.0);
  if (!state || !actions || !collected || !isfinite(base_head) ||
      !isfinite(vertical) || !bk_player_entry_placement(&placement, request))
    return 0;
  BkPlayerMovement *movement = &state->spatial.movement;
  memcpy(movement->position, placement.position, sizeof(movement->position));
  movement->yaw = placement.yaw_degrees;
  memset(movement->velocity, 0, sizeof(movement->velocity));
  movement->pitch = 0;
  movement->interaction_mode = 0;
  movement->action = 0;
  memcpy(state->spatial.scene.wall.position, placement.position, 12);
  state->spatial.scene.wall_heading = 0;
  bk_player_actions_initialize(actions);
  state->spatial.vertical_position = vertical;
  memset(state->spatial.scene.surface_name, 0,
         sizeof(state->spatial.scene.surface_name));
  memset(state->spatial.scene.wall_name, 0,
         sizeof(state->spatial.scene.wall_name));
  memset(&state->interaction.trigger, 0, sizeof(state->interaction.trigger));
  state->completion_requested = state->completion_mode = 0;
  if (request->previous_flow == 8 || request->previous_flow == 0x38)
    memset(collected, 0, BK_ITEM_TYPES);
  if (request->group == 1)
    collected[1] = 1;
  return 1;
}
int bk_npc_entry_reset(BkNpcSpatialState *state, BkNpcFootstepActions *actions,
                       BkNpcEntryRoute *metadata, BkRoute *route,
                       const BkEntryRequest *request, uint32_t route_start) {
  BkEntrySelection selection;
  BkActorPlacement placement;
  if (!state || !actions || !metadata ||
      !bk_game_entry_select(&selection, request))
    return 0;
  const BkRoutePoint *start = bk_route_point(route, route_start);
  if (!start || !bk_route_placement(&placement, route, selection.route_cursor,
                                    selection.player_position[1]))
    return 0;
  int8_t flag = (int8_t)start->flags;
  if (request->previous_flow == 0x48 &&
      !bk_route_set_flags(route, selection.route_cursor, 1))
    return 0;
  static const uint32_t ends[9] = {16, 0, 0, 0, 30, 15, 13, 0, 0};
  *metadata = (BkNpcEntryRoute){route_start,
                                request->group == 0 ? ends[request->area] : 0};
  memcpy(state->path.position, placement.position, sizeof(placement.position));
  state->path.yaw_degrees = placement.yaw_degrees;
  state->path.cursor = selection.route_cursor;
  state->path.segment_start = state->path.segment_end = 0;
  state->path.last_crossed = state->path.crossed = 0;
  state->ai.point.motion.action = 1;
  state->ai.point.motion.behavior = 1;
  state->ai.point.motion.route_flag = flag;
  state->ai.point.action_wait.duration = 2000;
  state->ai.point.background_wait = 0;
  state->ai.point.fade_out = selection.actor_fade_out;
  state->alpha = 1;
  memset(state->surface_name, 0, sizeof(state->surface_name));
  actions->walk[0] = 1;
  actions->run[0] = 4;
  actions->special[0] = 10;
  actions->special[1] = 12;
  actions->extra[0] = 7;
  actions->extra[1] = 9;
  return 1;
}

int bk_player_failure_reset(BkPlayerControl *s, int32_t actions[21],
                            float base_head) {
  float vertical = (float)((double)base_head + 10.0);
  if (!s || !actions || !isfinite(base_head) || !isfinite(vertical))
    return 0;
  s->spatial.movement.action = 0;
  s->spatial.movement.interaction_mode = 0;
  s->spatial.scene.wall_heading = 0;
  memset(s->spatial.scene.surface_name, 0,
         sizeof(s->spatial.scene.surface_name));
  memset(s->spatial.scene.wall_name, 0, sizeof(s->spatial.scene.wall_name));
  memset(&s->interaction.trigger, 0, sizeof(s->interaction.trigger));
  s->completion_mode = 0;
  s->spatial.vertical_position = vertical;
  return bk_player_actions_initialize(actions);
}
