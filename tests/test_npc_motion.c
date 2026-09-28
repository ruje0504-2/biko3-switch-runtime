#include "game/npc_action.h"
#include "game/npc_ai.h"
#include "game/npc_motion.h"
#include "game/npc_route.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static void word(uint8_t *p, uint32_t n) {
  for (unsigned i = 0; i < 4; i++)
    p[i] = (uint8_t)(n >> (8 * i));
}
static void number(uint8_t *p, float f) {
  uint32_t n;
  memcpy(&n, &f, 4);
  word(p, n);
}
static void route_integration(void) {
  uint8_t data[140] = {0};
  for (unsigned i = 0; i < 6; i++) {
    number(data + 20 * i, 1);
    number(data + 20 * i + 8, 10.0f * i);
  }
  data[16] = data[116] = 5;
  data[36] = 6;
  char error[256];
  BkRoute *route = bk_route_decode(data, sizeof(data), error);
  assert(route);
  BkNpcMotionActions actions;
  assert(bk_npc_motion_actions(&actions, 0));
  BkNpcRouteState state = {.path = {.position = {1, 7, 0}, .cursor = 1},
                           .point = {.motion = {.action = 1, .mode = 1}}};
  BkNpcRouteEffects effects = {0};
  BkNpcRouteState saved = state;
  /* Exact first-segment end does not trigger its point action yet. */
  assert(bk_npc_route_move(&state, route, &actions, 0, 1.25f, 100, &effects,
                           error));
  assert(state.path.cursor == 1 && !state.path.crossed &&
         state.path.position[2] == 10);
  assert(
      bk_npc_route_move(&state, route, &actions, 0, 0, 100, &effects, error));
  assert(state.path.cursor == 1 && !state.path.crossed);
  state = saved;
  assert(bk_npc_route_move(&state, route, &actions, 0, 1.5f, 100, &effects,
                           error));
  assert(state.path.cursor == 2 && state.path.position[2] == 10 &&
         state.path.position[1] == 7 && state.path.run_remaining == 3 &&
         state.point.motion.action == actions.run &&
         effects.point.snap_to_point);
  assert(
      bk_npc_route_move(&state, route, &actions, 0, 1, 1100, &effects, error));
  assert(state.path.cursor == 3 && state.path.position[2] == 20 &&
         state.path.run_remaining == 2);
  assert(
      bk_npc_route_move(&state, route, &actions, 0, 1, 2100, &effects, error));
  assert(state.path.cursor == 4 && state.path.position[2] == 30 &&
         state.path.run_remaining == 1);
  assert(
      bk_npc_route_move(&state, route, &actions, 0, 1, 3100, &effects, error));
  assert(state.path.cursor == 5 && state.path.position[2] == 46 &&
         state.path.run_remaining == 0 &&
         state.point.motion.action == actions.walk &&
         !effects.point.snap_to_point);
  assert(
      bk_npc_route_move(&state, route, &actions, 0, 1, 4100, &effects, error));
  assert(state.path.cursor == 6 && state.path.position[2] == 50 &&
         state.point.motion.action == actions.idle &&
         effects.point.snap_to_point);
  saved = state;
  assert(bk_npc_route_move(&state, route, &actions, 0, 100, 100000, &effects,
                           error));
  assert(!memcmp(&saved, &state, sizeof(saved)) && !effects.movement.allowed &&
         !effects.point.play_wait_sound && !effects.point.play_route_sound);
  /* An overrun must not publish the timer/action or partially crossed route. */
  state.point.motion.action = actions.walk;
  state.point.motion.route_flag = 0;
  saved = state;
  BkNpcRouteEffects saved_effects = effects;
  assert(!bk_npc_route_move(&state, route, &actions, 0, 100, 100000, &effects,
                            error));
  assert(!memcmp(&saved, &state, sizeof(saved)) &&
         !memcmp(&saved_effects, &effects, sizeof(effects)));
  state.path.cursor = 1;
  state.path.position[0] = 1;
  state.path.position[2] = 0;
  saved = state;
  for (unsigned i = 0; i < 4; i++) {
    const float invalid[] = {-1, NAN, INFINITY, FLT_MAX};
    assert(!bk_npc_route_move(&state, route, &actions, 0, invalid[i], 100000,
                              &effects, error));
    assert(!memcmp(&saved, &state, sizeof(saved)) &&
           !memcmp(&saved_effects, &effects, sizeof(effects)));
  }
  /* Cursor zero is deliberately started later in the native actor update. */
  state.path.cursor = 0;
  state.path.crossed = 0;
  saved = state;
  assert(bk_npc_route_move(&state, route, &actions, 0, 1, 0, &effects, error));
  assert(!memcmp(&saved, &state, sizeof(saved)));
  bk_route_destroy(route);
}
static void ai_ordering(void) {
  char error[256];
  uint32_t random = 123;
  BkNpcAiInput input = {.contact = {.group = 0,
                                    .area = 8,
                                    .cursor = 1,
                                    .actor_position = {30, 0, 0},
                                    .alpha = 1,
                                    .player_direction = {0, 0, 1}},
                        .suppressed_actions = {18, 19, 20, 21, 23, 27},
                        .active_clip = 7};
  BkNpcAiState actor = {
      .point = {.motion = {.action = 7, .behavior = 2, .mode = 1},
                .action_wait = {2000, 100, 1}}};
  BkNpcInteractionState shared = {9, 8, 7};
  BkNpcAiEffects effects = {255, 255};
  BkNpcAiState saved = actor;
  /* Office AREA return1 bypasses the already expired completion timer. */
  assert(
      bk_npc_ai_step(&actor, &shared, &random, &input, 100, &effects, error));
  assert(!memcmp(&actor, &saved, sizeof(actor)) && random == 123 &&
         shared.prompt == 0 && shared.response == 8 && shared.outcome == 7 &&
         !effects.sound);
  /* Suppression expires the same timer BEFORE the office query. */
  input.player_action = 18;
  actor.stimulus = 2;
  assert(
      bk_npc_ai_step(&actor, &shared, &random, &input, 100, &effects, error));
  assert(actor.point.motion.action == 1 && actor.point.motion.behavior == 1 &&
         !actor.point.action_wait.armed && !actor.stimulus && random == 123);
  /* Suppressed near reaction has defined choice0 without consuming RNG. */
  input.contact.area = 0;
  assert(
      bk_npc_ai_step(&actor, &shared, &random, &input, 100, &effects, error));
  assert(actor.point.motion.action == 7 && actor.point.motion.behavior == 2 &&
         actor.point.action_wait.duration == 5000 &&
         actor.point.action_wait.deadline == 5100 &&
         actor.point.action_wait.armed && actor.point.gate_state == 1 &&
         effects.sound == 1 && effects.used_zero_choice && random == 123);
  /* Early mode check preserves prompt and does not consume time or RNG. */
  actor.point.motion.mode = 2;
  saved = actor;
  shared.prompt = 13;
  assert(
      bk_npc_ai_step(&actor, &shared, &random, &input, 9000, &effects, error));
  assert(!memcmp(&actor, &saved, sizeof(actor)) && shared.prompt == 13 &&
         !effects.sound && !effects.used_zero_choice && random == 123);
  /* Scripted contact precedes even hidden/alpha gates. */
  input.contact.area = 4;
  input.contact.cursor = 67;
  input.contact.actor_position[0] = 0;
  input.contact.alpha = 0;
  actor.point.motion.mode = 1;
  actor.point.motion.hidden = 1;
  actor.point.motion.behavior = 0;
  assert(
      bk_npc_ai_step(&actor, &shared, &random, &input, 9000, &effects, error));
  assert(shared.prompt == 1 && shared.response == 2 && random == 123);
  /* Group2 office can still contact while hidden and partially faded. */
  input.contact.group = 2;
  input.contact.area = 8;
  input.contact.interaction_df = 1;
  assert(
      bk_npc_ai_step(&actor, &shared, &random, &input, 9000, &effects, error));
  assert(shared.response == 4);
  input.contact.interaction_df = 0;
  input.contact.interaction_e0 = 1;
  assert(
      bk_npc_ai_step(&actor, &shared, &random, &input, 9000, &effects, error));
  assert(shared.response == 3);
  input.contact.interaction_e0 = 0;
  assert(
      bk_npc_ai_step(&actor, &shared, &random, &input, 9000, &effects, error));
  assert(shared.outcome == 6 && actor.point.motion.behavior == 4);
  /* Rejected geometry/profile cannot publish any part of AI state. */
  saved = actor;
  BkNpcInteractionState saved_shared = shared;
  BkNpcAiEffects saved_effects = effects;
  for (unsigned i = 0; i < 4; i++) {
    BkNpcAiInput bad = input;
    if (i == 0)
      bad.contact.group = 5;
    if (i == 1)
      bad.contact.alpha = NAN;
    if (i == 2)
      bad.contact.actor_position[0] = INFINITY;
    if (i == 3)
      bad.contact.player_direction[2] = NAN;
    assert(
        !bk_npc_ai_step(&actor, &shared, &random, &bad, 9000, &effects, error));
    assert(!memcmp(&actor, &saved, sizeof(actor)) &&
           !memcmp(&shared, &saved_shared, sizeof(shared)) &&
           !memcmp(&effects, &saved_effects, sizeof(effects)) && random == 123);
  }
}
int main(void) {
  route_integration();
  ai_ordering();
  BkNpcMotionActions actions;
  assert(bk_npc_motion_actions(&actions, 0));
  assert(!bk_npc_motion_actions(&actions, 5));
  BkNpcMotionState state = {.action = 1, .mode = 1, .wait = {.duration = 100}};
  BkNpcMotion out = {0};
  assert(bk_npc_motion_select(&state, &actions, 0, .25f, 100, &out));
  assert(out.allowed && out.distance == 2 && !state.wait.armed);
  state.route_flag = 5;
  assert(bk_npc_motion_select(&state, &actions, 0, .25f, 100, &out));
  assert(!out.allowed && !out.distance && state.action == actions.idle);
  state.route_flag = 0;
  assert(bk_npc_motion_select(&state, &actions, 0, .25f, 100, &out));
  assert(!out.allowed && state.wait.armed && state.wait.deadline == 200);
  assert(bk_npc_motion_select(&state, &actions, 0, .25f, 199, &out));
  assert(state.action == actions.idle);
  assert(bk_npc_motion_select(&state, &actions, 0, .25f, 200, &out));
  assert(state.action == actions.walk && state.behavior == 1 && !out.allowed &&
         !state.wait.armed);
  assert(bk_npc_motion_select(&state, &actions, 0, .25f, 200, &out));
  assert(out.allowed && out.distance == 2);
  state.action = actions.stationary[0];
  assert(bk_npc_motion_select(&state, &actions, 0, .25f, 200, &out));
  assert(out.allowed && out.distance == 0);
  state.action = actions.run;
  BkNpcMotionState saved = state;
  BkNpcMotion previous = out;
  for (unsigned i = 0; i < 4; i++) {
    const float bad[] = {-1, NAN, INFINITY, FLT_MAX};
    assert(!bk_npc_motion_select(&state, &actions, 0, bad[i], 200, &out));
    assert(!memcmp(&saved, &state, sizeof(saved)) &&
           !memcmp(&previous, &out, sizeof(previous)));
  }
  BkNpcPointState point = {.motion = {.action = actions.stationary[1],
                                      .behavior = 2,
                                      .route_flag = 3},
                           .action_wait = {.duration = 2000}};
  assert(bk_npc_action_finish(&point, &actions, 7, 0));
  assert(point.action_wait.armed &&
         point.motion.action == actions.stationary[1]);
  assert(bk_npc_action_finish(&point, &actions, 7, 2000));
  assert(point.motion.action == actions.stationary[0] &&
         point.motion.behavior == 2);
  assert(bk_npc_action_finish(&point, &actions, 10, 2001));
  assert(point.action_wait.armed);
  assert(bk_npc_action_finish(&point, &actions, 12, 4001));
  assert(point.motion.action == actions.stationary[2] &&
         point.motion.behavior == 2);
  assert(bk_npc_action_finish(&point, &actions, 0, 4002));
  assert(point.motion.action == actions.idle && point.motion.behavior == 1 &&
         point.motion.route_flag == 3);
  point.motion.action = actions.stationary[3];
  point.motion.route_flag = 9;
  assert(bk_npc_action_finish(&point, &actions, actions.stationary[3], 5000));
  assert(point.motion.action == actions.stationary[3]);
  assert(bk_npc_action_finish(&point, &actions, 0, 5000));
  assert(point.motion.action == actions.walk && point.motion.route_flag == 0);
  assert(!bk_npc_action_finish(NULL, &actions, 0, 0));
  assert(!bk_npc_action_finish(&point, NULL, 0, 0));
  BkTimer timer = {.duration = 32};
  assert(bk_timer_poll(&timer,
                       0x7ffffff0)); /* original signed-boundary behavior */
  timer = (BkTimer){.duration = 32};
  assert(!bk_timer_poll(&timer, 0xfffffff0));
  assert(!bk_timer_poll(&timer, 15));
  assert(bk_timer_poll(&timer, 16));
  puts("PASS: NPC motion priority, action delay, timer exact deadline/wrap, "
       "atomic invalid input");
  return 0;
}
