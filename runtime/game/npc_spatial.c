#include "game/npc_spatial.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
int bk_npc_spatial_step(BkNpcSpatialState *state,
                        BkNpcInteractionState *interaction,
                        uint32_t *random_state, const BkRoute *route,
                        const BkCollision *collision,
                        const BkNpcSpatialInput *input, float seconds,
                        uint32_t now_ms, BkNpcSpatialEffects *effects,
                        char error[256]) {
  if (!state || !interaction || !random_state || !route || !collision ||
      !input || !effects || !isfinite(seconds) || seconds < 0 ||
      !isfinite(input->vertical_offset) || !isfinite(state->alpha))
    goto invalid;
  BkNpcSpatialState next = *state;
  BkNpcInteractionState shared = *interaction;
  uint32_t random = *random_state;
  BkNpcSpatialEffects result = {0};
  BkNpcMotionActions actions;
  if (input->group < 0 ||
      !bk_npc_motion_actions(&actions, (unsigned)input->group))
    goto invalid;
  BkNpcAiInput ai = {.contact = {.group = input->group,
                                 .area = input->area,
                                 .cursor = next.path.cursor,
                                 .alpha = next.alpha,
                                 .interaction_df = input->interaction_df,
                                 .interaction_e0 = input->interaction_e0},
                     .player_action = input->player_action,
                     .active_clip = input->active_clip};
  memcpy(ai.contact.actor_position, next.path.position, 12);
  memcpy(ai.contact.player_position, input->player_position, 12);
  memcpy(ai.contact.player_direction, input->player_direction, 12);
  memcpy(ai.suppressed_actions, input->suppressed_actions,
         sizeof(ai.suppressed_actions));
  if (!bk_npc_ai_step(&next.ai, &shared, &random, &ai, now_ms, &result.ai,
                      error))
    return 0;
  BkNpcRouteState moving = {next.path, next.ai.point};
  if (!bk_npc_route_move(&moving, route, &actions, input->background_clip,
                         seconds, now_ms, &result.route, error))
    return 0;
  next.path = moving.path;
  next.ai.point = moving.point;
  BkNpcHeadInput head = {.actor_yaw = next.path.yaw_degrees,
                         .actor_kind = input->group};
  memcpy(head.actor_head_world, input->head_world, 64);
  memcpy(head.actor_head_local, input->head_local, 64);
  memcpy(head.torso_local, input->torso_local, 64);
  memcpy(head.player_position, input->player_position, 12);
  memcpy(head.player_head, input->player_head, 12);
  if (!bk_npc_head_update(&next.head, &head, error))
    return 0;
  if (!bk_route_point(route, next.path.cursor)) {
    snprintf(error, 256, "NPC spatial: cursor beyond decoded route");
    return 0;
  }
  if (!next.path.cursor) {
    next.alpha = .99f;
    next.ai.point.fade_out = 0;
    next.path.cursor = 1;
  }
  BkNpcSceneState scene = {.behavior = next.ai.point.motion.behavior,
                           .hidden = next.ai.point.motion.hidden,
                           .visible = next.visible};
  memcpy(scene.position, next.path.position, 12);
  memcpy(scene.surface_name, next.surface_name, sizeof(scene.surface_name));
  BkNpcSceneInput geometry = {
      .cone = {.head_distance = next.head.sight.distance,
               .facing = next.head.facing,
               .actor_kind = input->group,
               .player_action = input->player_action,
               .short_range_action = input->short_range_action},
      .sight = next.head.sight,
      .excluded_surface = input->excluded_surface};
  memcpy(geometry.cone.actor_position, next.path.position, 12);
  memcpy(geometry.cone.player_position, input->player_position, 12);
  memcpy(geometry.suppressed_actions, input->suppressed_actions,
         sizeof(geometry.suppressed_actions));
  if (!bk_npc_scene_step(&scene, collision, &geometry, seconds, error))
    return 0;
  memcpy(next.path.position, scene.position, 12);
  next.ai.point.motion.behavior = scene.behavior;
  next.visible = scene.visible;
  memcpy(next.surface_name, scene.surface_name, sizeof(next.surface_name));
  if (!bk_actor_placement(&result.placement, next.path.position,
                          next.path.yaw_degrees))
    goto invalid;
  result.vertical_position =
      (float)((double)next.path.position[1] + input->vertical_offset);
  if (!isfinite(result.vertical_position))
    goto invalid;
  *state = next;
  *interaction = shared;
  *random_state = random;
  *effects = result;
  return 1;
invalid:
  snprintf(error, 256, "NPC spatial: invalid state/profile/step");
  return 0;
}
