#include "game/area_entry.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
int bk_area_entry_reset(BkPlayerControl *player, BkNpcSpatialState *npc,
                        float *npc_vertical, BkPlayerView *view,
                        BkGameCameraState *camera,
                        const BkNpcEntryRoute *metadata,
                        const BkNpcFootstepActions *actions,
                        const BkRoute *route, const BkEntryRequest *next,
                        char error[256]) {
  BkEntrySelection selection;
  if (!player || !npc || !npc_vertical || !view || !camera || !metadata ||
      !actions || !bk_game_entry_select(&selection, next) || next->area == 0)
    goto bad;
  const BkRoutePoint *first = bk_route_point(route, 0);
  const BkRoutePoint *start = bk_route_point(route, metadata->start);
  BkActorPlacement player_place, npc_place;
  float xyz[3], yaw = selection.player_yaw;
  for (unsigned i = 0; i < 3; ++i)
    xyz[i] = selection.player_position[i] >= 99999
                 ? player->spatial.movement.position[i]
                 : selection.player_position[i];
  if (yaw >= 99999)
    yaw = player->spatial.movement.yaw;
  if (!first || !start || !bk_actor_placement(&player_place, xyz, yaw) ||
      !bk_route_placement(&npc_place, route, 0, selection.player_position[1]))
    goto bad;
  /*4befc0's global previous48 override applies to BOTH calls. Vertical+2a8
   * still receives the original height argument, including this branch. */
  if (next->previous_flow == 0x48) {
    if (!bk_player_entry_placement(&player_place, next))
      goto bad;
    npc_place = player_place;
  }
  BkPlayerControl p = *player;
  BkNpcSpatialState n = *npc;
  BkPlayerView v = *view;
  BkGameCameraState c = *camera;
  memcpy(p.spatial.movement.position, player_place.position, 12);
  memcpy(p.spatial.scene.wall.position, player_place.position, 12);
  p.spatial.movement.yaw = player_place.yaw_degrees;
  memset(p.spatial.movement.velocity, 0, 12);
  p.spatial.movement.pitch = 0;
  p.spatial.movement.interaction_mode = 0;
  p.spatial.vertical_position = xyz[1];
  n.path.cursor = 0;
  memcpy(n.path.position, npc_place.position, 12);
  n.path.yaw_degrees = npc_place.yaw_degrees;
  n.ai.point.motion.route_flag = (int8_t)start->flags;
  n.ai.point.motion.behavior = 1;
  n.ai.point.action_wait.duration = 3000;
  n.ai.point.action_wait.armed = 0;
  n.ai.point.motion.action = actions->walk[0];
  n.ai.point.background_wait = n.ai.point.fade_out = 0;
  if (!bk_entry_player_view_reset(&v, n.path.position))
    goto bad;
  v.yaw = first->parameter;
  if (!isfinite(v.yaw))
    goto bad;
  p.spatial.scene.wall.camera_distance = v.target_distance;
  c.phase = selection.phase;
  n.ai.point.motion.mode = (int8_t)c.phase;
  *player = p;
  *npc = n;
  *npc_vertical = selection.player_position[1];
  *view = v;
  *camera = c;
  return 1;
bad:
  snprintf(error, 256, "area entry: invalid next profile/route/bindings");
  return 0;
}
