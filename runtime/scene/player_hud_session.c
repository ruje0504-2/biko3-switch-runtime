#include "scene/player_hud_session.h"
#include <string.h>
int bk_player_hud_session_step(BkPlayerHudState *h, BkGameFrameState *s,
                               uint8_t special, const float view[16],
                               const BkCameraLens *lens, unsigned width,
                               unsigned height, float seconds, uint32_t now,
                               BkPlayerHudFrame *f, int *unprojectable,
                               char e[256]) {
  if (!h || !s || !view || !lens || !f || !unprojectable || !width || !height ||
      s->camera.phase != 1) {
    snprintf(e, 256, "player HUD session: invalid phase/binding");
    return 0;
  }
  BkPlayerHudInput in = {
      .action = s->player.spatial.movement.action,
      .trigger_kind = s->player.interaction.trigger.prop_kind,
      .npc_behavior = s->npc.ai.point.motion.behavior,
      .counter = s->hotkeys.photo_count,
      .interface_mode = (uint8_t)s->player.spatial.movement.interaction_mode,
      .npc_in_view = (uint8_t)s->player.spatial.scene.npc_in_view,
      .menu_request = s->hotkeys.menu_request,
      .response = s->interaction.response,
      .special_mode = special,
      .prop_available = s->prop_interaction.available,
      .cover_available = (uint8_t)s->player.wall_available,
      .npc_prompt = s->interaction.prompt};
  memcpy(in.actions, s->player_actions, sizeof(in.actions));
  memcpy(in.inventory, s->pickup.collected, sizeof(in.inventory));
  BkViewport viewport = {0, 0, width, height};
  BkScreenPoint projected;
  int absent = !bk_camera_project_frame(&projected, s->prop_interaction.matrix,
                                        view, lens, &viewport);
  if (absent) {
    /* Validate the camera independently before attributing failure to the
     * unavailable, never-populated prop frame. */
    float projection[16], inverse[16];
    if (in.prop_available == 1 || !bk_camera_projection(projection, lens) ||
        !bk_camera_view(inverse, view)) {
      snprintf(e, 256, "player HUD session: invalid active projection");
      return 0;
    }
    projected = (BkScreenPoint){{0, 0}, h->depth};
  }
  BkPlayerHudState next = *h;
  BkPlayerHudFrame frame;
  uint8_t outcome = s->interaction.outcome;
  if (!bk_player_hud_update(&next, &in, &outcome, &projected, seconds, now,
                            width, e) ||
      !bk_player_hud_draws(&next, &in, width, &frame, e))
    return 0;
  *h = next;
  *f = frame;
  *unprojectable = absent;
  s->interaction.outcome = outcome;
  s->player.completion_requested = (int8_t)outcome;
  return 1;
}
