#include "game/player_control.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
int main(void) {
  char error[256];
  BkPlayerMovement m = {
      .position = {0, 7, 0}, .previous = {4, 5, 6}, .velocity = {1, 2, 3}};
  BkPlayerInteraction s = {0};
  BkPlayerInteractionInput in = {.trigger = {.wall_name = ""}};
  for (unsigned i = 0; i < 21; ++i)
    in.trigger.actions[i] = (int32_t)i;
  for (unsigned i = 0; i < 128; ++i)
    in.clips[i] = (BkPlayerClipTiming){300, 600, 380};
  /* Original-order priority beats nearest-object selection; Y is ignored. */
  in.trigger.props[0] = (BkPlayerTriggerProp){1, 10, {20, 10000, 0}};
  in.trigger.props[1] = (BkPlayerTriggerProp){1, 19, {1, 7, 0}};
  in.buttons = BK_PLAYER_INTERACT;
  int allowed = -1;
  assert(bk_player_interaction(&m, &s, &allowed, &in, error));
  assert(!allowed && m.action == 11 && s.script_phase == 1);
  assert(s.trigger.prop_kind == 10 && s.trigger.target[0] == 20 &&
         s.trigger.target[1] == 10000);
  /* Already entered: target remains fixed, exit switches phase and mode. */
  BkPlayerTrigger old = s.trigger;
  in.trigger.props[0].position[0] = 3;
  m.interaction_mode = 2;
  assert(bk_player_interaction(&m, &s, &allowed, &in, error));
  assert(m.action == 12 && s.script_phase == 2 && !m.interaction_mode &&
         !allowed);
  assert(!memcmp(&old, &s.trigger, sizeof(old)));
  /* Stance wins simultaneous input and can immediately finish slot9. */
  m.action = 8;
  m.interaction_mode = 1;
  in.buttons = 3;
  in.active_clip = 0;
  assert(bk_player_interaction(&m, &s, &allowed, &in, error));
  assert(m.action == 0 && !m.interaction_mode && allowed);
  /* Source is the action's retained slot, independent of active clip. */
  m.action = 13;
  in.buttons = 0;
  in.active_clip = 7;
  in.clips[7].source = 0;
  in.clips[13].source = 600;
  assert(bk_player_interaction(&m, &s, &allowed, &in, error));
  assert(m.interaction_mode == 2 && s.script_phase == 0 && allowed);
  /* Inclusive trigger20, strict scripted369/400 boundaries. First root still
   * rotates/sets pitch even when translation is blocked. */
  BkPlayerScriptEffects effect;
  m.action = 11;
  s.script_phase = 1;
  s.trigger.target[0] = 20;
  s.trigger.target[2] = 10;
  s.trigger.target[3] = 90;
  in.clips[11].source = 369;
  assert(bk_player_script_step(&m, &s, .1f, 0, &in, &effect, error));
  assert(!effect.complete && effect.placements == 1 && m.position[0] == 0 &&
         m.pitch == 20);
  assert(m.previous[1] == 5 && m.velocity[1] == 2);
  in.clips[11].source = 380;
  assert(bk_player_script_step(&m, &s, .1f, 0, &in, &effect, error));
  assert(effect.placements == 2 && m.position[0] == 4 && m.position[2] == 2);
  /* Interaction can change the phase/action, but the current displacement
   * was already calculated from its original target. */
  in.buttons = BK_PLAYER_INTERACT;
  s.trigger.origin[0] = -50;
  assert(bk_player_script_step(&m, &s, .1f, 0, &in, &effect, error));
  assert(m.action == 12 && s.script_phase == 2 && m.position[0] > 4);
  /* Early completion has no interaction, pitch or root effects. */
  in.clips[12].source = 600;
  BkPlayerMovement before = m;
  BkPlayerInteraction prior = s;
  assert(bk_player_script_step(&m, &s, .1f, 0, &in, &effect, error));
  assert(effect.complete && !effect.placements &&
         !memcmp(&m, &before, sizeof(m)) && !memcmp(&s, &prior, sizeof(s)));
  /* Invalid exit span fails atomically, including emitted root commands. */
  in.buttons = 0;
  in.clips[12] = (BkPlayerClipTiming){300, 300, 301};
  BkPlayerScriptEffects saved = effect;
  assert(!bk_player_script_step(&m, &s, .1f, 0, &in, &effect, error));
  assert(!memcmp(&effect, &saved, sizeof(saved)) &&
         !memcmp(&m, &before, sizeof(m)) && !memcmp(&s, &prior, sizeof(s)));
  /* Parent dispatch: completion handshake, then phase3 holds. */
  BkPlayerControl control = {
      .spatial = {.movement = m}, .interaction = s, .completion_mode = 1};
  BkPlayerControlInput input = {.interaction = in,
                                .spatial = {.movement = {.seconds = .1f}}};
  memcpy(input.spatial.movement.actions, in.trigger.actions,
         sizeof(in.trigger.actions));
  input.interaction.clips[12] = (BkPlayerClipTiming){300, 600, 600};
  BkPlayerControlEffects output;
  assert(bk_player_control_step(&control, NULL, &input, &output, error));
  assert(control.interaction.script_phase == 3 &&
         control.completion_requested == 1 && !output.placements);
  BkPlayerControl held = control;
  assert(bk_player_control_step(&control, NULL, &input, &output, error));
  assert(!memcmp(&control, &held, sizeof(held)) && !output.placements &&
         !output.shadow_place);
  puts("PASS player target priority, transitions, retained timing, scripted "
       "gates and atomic failure");
  return 0;
}
