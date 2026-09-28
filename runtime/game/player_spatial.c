#include "game/player_spatial.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

int bk_player_idle_step(BkPlayerSpatial *state, int32_t idle_action,
                        const BkCollision *collision,
                        const BkPlayerSceneInput *input,
                        BkActorPlacement *placement, char error[256]) {
  if (!state || !input || !placement) {
    if (error)
      snprintf(error, 256, "Invalid idle player stage");
    return 0;
  }
  BkPlayerSpatial next = *state;
  BkPlayerSceneInput in = *input;
  BkPlayerMovement *m = &next.movement;
  m->action = idle_action;
  memcpy(m->previous, m->position, 12);
  memcpy(next.scene.wall.position, m->position, 12);
  memcpy(in.wall.previous, m->previous, 12);
  in.wall.motion[0] = m->velocity[0];
  in.wall.motion[1] = m->velocity[2];
  in.wall.motion[2] = m->turn[0];
  in.wall.height = next.vertical_position;
  if (!bk_player_scene_step(&next.scene, collision, &in, error))
    return 0;
  memcpy(m->position, next.scene.wall.position, 12);
  BkActorPlacement root;
  if (!bk_actor_placement(&root, m->position, m->yaw)) {
    if (error)
      snprintf(error, 256, "Invalid idle player placement");
    return 0;
  }
  m->interaction_mode = 0;
  *state = next;
  *placement = root;
  return 1;
}

int bk_player_spatial_step(BkPlayerSpatial *state, const BkCollision *collision,
                           const BkPlayerSpatialInput *in,
                           BkActorPlacement *placement, char error[256]) {
  if (!state || !in || !placement || !isfinite(in->base_head_height) ||
      !isfinite(in->cached_head_height))
    goto invalid;
  BkPlayerSpatial next = *state;
  if (!bk_player_movement(&next.movement, &in->movement, error))
    return 0;
  int32_t action = next.movement.action;
  const int32_t *actions = in->movement.actions;
  next.vertical_position =
      action == actions[8] || action == actions[9]
          ? in->cached_head_height
          : (float)((double)next.movement.position[1] + in->base_head_height);
  if (!isfinite(next.vertical_position))
    goto invalid;
  memcpy(next.scene.wall.position, next.movement.position, 12);
  static const unsigned bypass_slots[] = {11, 12, 13, 14, 16, 17};
  int bypass = 0;
  for (unsigned i = 0; i < sizeof(bypass_slots) / sizeof(*bypass_slots); ++i)
    if (action == actions[bypass_slots[i]])
      bypass = 1;
  if (bypass) {
    int visible;
    if (!bk_player_scene_in_view(&visible, &in->scene))
      goto invalid;
    next.scene.npc_in_view = visible;
    next.scene.wall.near_wall = 0;
  } else {
    BkPlayerSceneInput query = in->scene;
    memcpy(query.wall.previous, next.movement.previous, 12);
    query.wall.motion[0] = next.movement.velocity[0];
    query.wall.motion[1] = next.movement.velocity[2];
    query.wall.motion[2] = next.movement.turn[0];
    query.wall.height = next.vertical_position;
    if (!bk_player_scene_step(&next.scene, collision, &query, error))
      return 0;
    memcpy(next.movement.position, next.scene.wall.position, 12);
  }
  BkActorPlacement root;
  if (!bk_actor_placement(&root, next.movement.position, next.movement.yaw))
    goto invalid;
  *state = next;
  *placement = root;
  return 1;
invalid:
  if (error)
    snprintf(error, 256, "Invalid player spatial stage");
  return 0;
}
