#include "game/player_control.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
static void query_scene(BkPlayerInteractionInput *in,
                        const BkPlayerSpatial *state) {
  in->trigger.wall_name = state->scene.wall_name;
  in->trigger.wall_heading = state->scene.wall_heading;
  memcpy(in->normal, state->scene.wall.normal, 12);
}
int bk_player_control_step(BkPlayerControl *state, const BkCollision *collision,
                           const BkPlayerControlInput *in,
                           BkPlayerControlEffects *effects, char error[256]) {
  if (!state || !in || !effects || (in->has_shadow != 0 && in->has_shadow != 1))
    goto invalid;
  BkPlayerControl next = *state;
  BkPlayerControlEffects out = {0};
  int phase = next.interaction.script_phase;
  if (phase == 0 || phase == 1 || phase == 2) {
    if (!memchr(next.spatial.scene.wall_name, 0,
                sizeof(next.spatial.scene.wall_name)))
      goto invalid;
    BkPlayerInteractionInput interaction = in->interaction;
    query_scene(&interaction, &next.spatial);
    memcpy(interaction.trigger.actions, in->spatial.movement.actions,
           sizeof(interaction.trigger.actions));
    interaction.active_clip = in->spatial.movement.active_clip;
    if (phase == 1 || phase == 2) {
      BkPlayerScriptEffects script;
      if (!bk_player_script_step(&next.spatial.movement, &next.interaction,
                                 in->spatial.movement.seconds, next.return_yaw,
                                 &interaction, &script, error))
        return 0;
      out.placements = script.placements;
      memcpy(out.roots, script.roots, sizeof(out.roots));
      memcpy(next.spatial.scene.wall.position, next.spatial.movement.position,
             12);
      if (script.complete) {
        next.interaction.script_phase = next.completion_mode == 1 ? 3 : 0;
        if (next.completion_mode == 1)
          next.completion_requested = 1;
      }
    } else {
      BkPlayerSpatialInput spatial = in->spatial;
      if (!bk_player_interaction(&next.spatial.movement, &next.interaction,
                                 &spatial.movement.controls_allowed,
                                 &interaction, error) ||
          !bk_player_spatial_step(&next.spatial, collision, &spatial,
                                  &out.roots[0], error))
        return 0;
      out.placements = 1;
      /* Only this ordinary path refreshes availability and mesh shadow. */
      query_scene(&interaction, &next.spatial);
      int cover, available = 0;
      if (!bk_player_trigger_cover(&cover, &interaction.trigger) ||
          (!cover &&
           !bk_player_trigger_wall_available(&available, &interaction.trigger)))
        goto invalid;
      next.wall_available = (int8_t)available;
      if (in->has_shadow) {
        float position[3];
        memcpy(position, next.spatial.movement.position, 12);
        position[1] = (float)((double)position[1] + .1f);
        if (!bk_actor_placement(&out.shadow, position,
                                next.spatial.movement.yaw))
          goto invalid;
        out.shadow_place = 1;
      }
    }
  }
  *state = next;
  *effects = out;
  return 1;
invalid:
  if (error)
    snprintf(error, 256, "Invalid player control stage");
  return 0;
}
