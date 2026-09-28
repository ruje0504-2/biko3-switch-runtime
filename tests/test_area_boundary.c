#include "game/area_boundary.h"
#include "game/npc_detection.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
int main(void) {
  BkNpcSpatialState npc = {0};
  BkNpcInteractionState contact = {0};
  BkNpcDetectionInput detect = {0, 10, {11, 12, 13, 14}, {15, 16}};
  npc.visible = 1;
  npc.ai.stimulus = 1;
  assert(bk_npc_detection_resolve(&npc, &contact, &detect));
  assert(contact.outcome == 3 && npc.ai.point.motion.behavior == 4);
  detect.area = 8;
  assert(bk_npc_detection_resolve(&npc, &contact, &detect));
  assert(!npc.visible && contact.outcome == 3);
  BkNpcSpatialState saved_npc = npc;
  assert(!bk_npc_detection_resolve(&npc, &contact, NULL));
  assert(memcmp(&npc, &saved_npc, sizeof(npc)) == 0);
  BkAreaBounds b = {-200, 200, 200, -200};
  BkPropState props[16] = {0};
  props[0].kind = 1;
  props[0].path.position[0] = -600;
  props[0].path.yaw = 90;
  uint8_t gate = 77;
  assert(bk_area_prop_boundary(&props[0], &gate, 0, 6, &b));
  assert(!props[0].hidden && gate == 1);
  props[0].path.position[0] = 2000;
  assert(bk_area_prop_boundary(&props[0], &gate, 0, 2, &b));
  assert(props[0].hidden && gate == 1); /* special preserves gate */
  props[0].path.position[0] = NAN;
  BkPropState saved_prop = props[0];
  assert(!bk_area_prop_boundary(&props[0], &gate, 0, 6, &b));
  assert(memcmp(&saved_prop, &props[0], sizeof(saved_prop)) == 0 && gate == 1);
  props[0].path.position[0] = -600;
  BkAreaBoundaryState state = {0};
  BkAreaBoundaryInput input = {.group = 0,
                               .area = 6,
                               .props_present = 3,
                               .npc_present = 1,
                               .npc_group = 0,
                               .effect_volume = 0,
                               .player_position = {500, 0, 500},
                               .player_yaw = 180,
                               .player_wall = "exit",
                               .boundary_wall = "exit",
                               .bounds = b};
  npc.path.position[0] = npc.path.position[2] = 500;
  npc.path.cursor = 234;
  BkAreaBoundaryCommands commands;
  assert(
      bk_area_boundary_step(&state, props, &npc, &contact, &input, &commands));
  assert(!state.ambient_gate); /* last nontrain overwrites first train gate */
  assert(npc.ai.point.motion.hidden == 1 && contact.response == 1);
  assert(state.npc_sound_played == 1 && state.player_sound_played == 1);
  assert(commands.count == 2 &&
         commands.commands[0].recipient == BK_AREA_SOUND_NPC &&
         commands.commands[1].recipient == BK_AREA_SOUND_PLAYER);
  for (unsigned i = 0; i < 2; ++i)
    assert(!strcmp(commands.commands[i].file, "se304.wav") &&
           commands.commands[i].gain.volume == 0 &&
           commands.commands[i].gain.pan == 0);
  assert(
      bk_area_boundary_step(&state, props, &npc, &contact, &input, &commands));
  assert(commands.count == 0);
  /* Player response still uses stale NPC hidden when actor is absent. */
  input.npc_present = 0;
  input.props_present = 0;
  npc.path.position[0] = NAN;
  state.player_sound_played = 0;
  contact.response = 0;
  assert(
      bk_area_boundary_step(&state, props, &npc, &contact, &input, &commands));
  assert(contact.response == 1 && commands.count == 1);
  /* Later geometry failure preserves preceding prop/gate writes. */
  input.props_present = 3;
  props[1].path.position[2] = NAN;
  BkPropState saved[16];
  memcpy(saved, props, sizeof(saved));
  BkAreaBoundaryState saved_state = state;
  saved_npc = npc;
  BkNpcInteractionState saved_contact = contact;
  BkAreaBoundaryCommands saved_commands = commands;
  assert(
      !bk_area_boundary_step(&state, props, &npc, &contact, &input, &commands));
  assert(memcmp(saved, props, sizeof(saved)) == 0);
  assert(memcmp(&state, &saved_state, sizeof(state)) == 0);
  assert(memcmp(&npc, &saved_npc, sizeof(npc)) == 0);
  assert(memcmp(&contact, &saved_contact, sizeof(contact)) == 0);
  assert(memcmp(&commands, &saved_commands, sizeof(commands)) == 0);
  puts("PASS detection/shared outcome and area boundary "
       "ordering/retention/atomic rejection");
}
