#include "game/npc_ai.h"
#include "core/random.h"
#include "game/npc_action.h"
#include "world/proximity.h"
#include <math.h>
#include <stdio.h>
static int query(BkNpcAiState *actor, BkNpcInteractionState *shared,
                 const BkNpcContactInput *input, BkNpcContactQuery kind,
                 uint8_t *result) {
  BkNpcContactState state = {actor->point.motion.behavior, shared->prompt,
                             shared->response, shared->outcome};
  if (!bk_npc_contact_query(&state, input, kind, result))
    return 0;
  actor->point.motion.behavior = state.behavior;
  *shared =
      (BkNpcInteractionState){state.prompt, state.response, state.outcome};
  return 1;
}
static void react(BkNpcAiState *actor, int32_t behavior, int32_t action,
                  uint32_t duration) {
  actor->point.motion.behavior = behavior;
  actor->point.motion.action = action;
  actor->point.action_wait.duration = duration;
  actor->point.action_wait.armed = 0;
}
int bk_npc_ai_step(BkNpcAiState *state, BkNpcInteractionState *interaction,
                   uint32_t *random_state, const BkNpcAiInput *input,
                   uint32_t now_ms, BkNpcAiEffects *effects, char error[256]) {
  if (!state || !interaction || !random_state || !input || !effects ||
      !isfinite(input->contact.alpha))
    goto invalid;
  for (unsigned i = 0; i < 3; i++)
    if (!isfinite(input->contact.actor_position[i]) ||
        !isfinite(input->contact.player_position[i]) ||
        !isfinite(input->contact.player_direction[i]))
      goto invalid;
  BkNpcAiState next = *state;
  BkNpcInteractionState shared = *interaction;
  BkNpcAiEffects result = {0};
  uint32_t random = *random_state;
  BkNpcMotionActions actions;
  /* This path currently supports the five verified NPC action profiles. */
  if (input->contact.group < 0 ||
      !bk_npc_motion_actions(&actions, (unsigned)input->contact.group))
    goto invalid;
  BkNpcMotionState *motion = &next.point.motion;
  const BkNpcContactInput *contact = &input->contact;
  uint8_t code;
  if (contact->area < 8 && (motion->mode == 0 || motion->mode == 2))
    goto done;
  if (!query(&next, &shared, contact, BK_NPC_CONTACT_SCRIPTED, &code))
    goto invalid;
  if (code) {
    shared.response = 2;
    goto done;
  }
  if (motion->hidden == 1 || (double)contact->alpha <= .9) {
    if (contact->group == 2 && contact->area == 8 &&
        !query(&next, &shared, contact, BK_NPC_CONTACT_AREA, &code))
      goto invalid;
    goto done;
  }
  int suppressed = 0;
  for (unsigned i = 0; i < 6; i++)
    if (input->player_action == input->suppressed_actions[i]) {
      suppressed = 1;
      next.stimulus = 0;
      if ((motion->behavior == 2 || motion->behavior == 3) &&
          bk_timer_poll(&next.point.action_wait, now_ms)) {
        motion->action = actions.walk;
        motion->behavior = 1;
        motion->route_flag = 0;
      }
      break;
    }
  shared.prompt = 0;
  if (!query(&next, &shared, contact, BK_NPC_CONTACT_SCRIPTED, &code))
    goto invalid;
  if (code) {
    shared.response = 2;
    goto done;
  }
  if (!query(&next, &shared, contact, BK_NPC_CONTACT_WAITING, &code))
    goto invalid;
  if (code)
    goto done;
  if (!query(&next, &shared, contact, BK_NPC_CONTACT_AREA, &code))
    goto invalid;
  if (code)
    goto done;
  /* Original initializes this only in !suppressed, but its near50 branch
   * below can read it after suppression too. Define that stack-dependent
   * case as zero, preserving the original RNG consumption everywhere. */
  unsigned choice = 0;
  int near;
  if (!suppressed) {
    choice = bk_random_next(&random) % 4;
    if (next.stimulus == 1 || next.stimulus == 2) {
      int second = next.stimulus == 2;
      next.stimulus = 0;
      if (motion->behavior == 1)
        result.sound = second ? 2 : 1;
      react(&next, second ? 3 : 2, actions.stationary[second ? 0 : 1], 5000);
      if (motion->route_flag == 4)
        motion->route_flag = 0;
    }
    if (motion->behavior != 0) {
      if (!bk_proximity_xz(&near, contact->actor_position,
                           contact->player_position, 8))
        goto invalid;
      if (near) {
        shared.outcome = 2;
        motion->behavior = 4;
      }
    }
  }
  if (!bk_proximity_xz(&near, contact->actor_position, contact->player_position,
                       motion->route_flag == 4 ? 70 : 50))
    goto invalid;
  if (!near)
    next.point.gate_state = 0;
  else if (!next.point.gate_state && motion->behavior == 1) {
    if (motion->route_flag == 4)
      react(&next, 2, actions.stationary[1], 5000);
    else {
      int short_wait = choice == 1 || choice == 3;
      react(&next, 2, actions.stationary[short_wait ? 0 : 1],
            short_wait ? 2000 : 5000);
      result.used_zero_choice = (uint8_t)suppressed;
    }
    result.sound = 1;
    next.point.gate_state = 1;
  }
  bk_npc_action_finish(&next.point, &actions, input->active_clip, now_ms);
done:
  *state = next;
  *interaction = shared;
  *random_state = random;
  *effects = result;
  return 1;
invalid:
  snprintf(error, 256, "NPC AI: invalid actor/profile/contact geometry");
  return 0;
}
