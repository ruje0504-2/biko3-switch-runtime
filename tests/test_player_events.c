#ifdef NDEBUG
#undef NDEBUG
#endif
#include "game/player_animation.h"
#include "game/player_events.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
int main(void) {
  uint8_t steps[544] = {0}, shared[426] = {0};
  BkPlayerEventState state = {0, 7};
  BkPlayerEventInput in = {.surface = "miss"};
  for (unsigned i = 0; i < 21; ++i)
    in.actions[i] = (int)i;
  BkPlayerEvents out;
  in.action = 1;
  in.source = 130;
  assert(bk_player_events(&state, steps, sizeof(steps), shared, sizeof(shared),
                          &in, &out));
  assert(out.count == 1 && !strcmp(out.commands[0].file, "se104.wav"));
  assert(steps[123] == 1 && steps[107] == 0 && shared[123] == 0);
  assert(bk_player_events(&state, steps, sizeof(steps), shared, sizeof(shared),
                          &in, &out));
  assert(out.count == 1 &&
         steps[107] == 1); /* Second branch runs next frame. */
  in.action = 11;
  in.source = 409;
  assert(bk_player_events(&state, steps, sizeof(steps), shared, sizeof(shared),
                          &in, &out));
  assert(out.count == 2 && out.commands[0].loop && !out.commands[1].loop);
  assert(!strcmp(out.commands[0].file, "se119.wav") &&
         !strcmp(out.commands[1].file, "se110.wav"));
  assert(shared[408] == 1 && steps[408] == 0 && state.loop_latched == 1);
  in.action = 1;
  in.voice_present = 1;
  in.source = 0;
  assert(bk_player_events(&state, steps, sizeof(steps), shared, sizeof(shared),
                          &in, &out));
  assert(out.count == 1 && !out.commands[0].file && state.loop_latched == 0);
  in.action = 8;
  assert(bk_player_events(&state, steps, sizeof(steps), shared, sizeof(shared),
                          &in, &out));
  assert(out.count == 1 && out.commands[0].loop && !out.default_direction);
  in.source = NAN;
  uint8_t saved_steps[544], saved_shared[426];
  memcpy(saved_steps, steps, sizeof(steps));
  memcpy(saved_shared, shared, sizeof(shared));
  BkPlayerEventState old = state;
  BkPlayerEvents oldout = out;
  assert(!bk_player_events(&state, steps, sizeof(steps), shared, sizeof(shared),
                           &in, &out));
  assert(!memcmp(&old, &state, sizeof(old)) &&
         !memcmp(&oldout, &out, sizeof(out)) &&
         !memcmp(steps, saved_steps, sizeof(steps)) &&
         !memcmp(shared, saved_shared, sizeof(shared)));
  assert(bk_player_presentation_alpha(1, 0, 0) == 0);
  assert(bk_player_presentation_alpha(1, 0x40, 0) == 1);
  assert(bk_player_presentation_alpha(1, 0, 2) == 1);
  assert(bk_player_presentation_alpha(0, 0, 0) == 1);
  puts("PASS player dual event banks, short-circuit priority, loop/stop and "
       "alpha");
}
