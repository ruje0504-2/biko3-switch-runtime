#ifdef NDEBUG
#undef NDEBUG
#endif
#include "game/player_movement.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
int main(void) {
  char error[256];
  BkPlayerMovementInput in = {
      .seconds = 1, .controls_allowed = 1, .active_clip = 1};
  for (unsigned i = 0; i < 21; i++)
    in.actions[i] = (int32_t)(100 + i);
  BkPlayerMovement s = {.position = {1, 2, 3}, .action = 100};
  in.buttons = BK_PLAYER_FORWARD | BK_PLAYER_LEFT | BK_PLAYER_RIGHT;
  assert(bk_player_movement(&s, &in, error));
  assert(s.position[0] == 1 && s.position[1] == 2 && s.position[2] == 19);
  assert(s.previous[2] == 3 && s.yaw == 310 && s.action == 103 &&
         s.acceleration == 1);
  s = (BkPlayerMovement){.action = 100, .move_latch = 1};
  in.buttons = BK_PLAYER_FORWARD;
  assert(bk_player_movement(&s, &in, error));
  assert(s.position[2] == 8 && s.action == 101);
  in.buttons = BK_PLAYER_BACKWARD;
  assert(bk_player_movement(&s, &in, error));
  assert(s.position[2] == 0 && s.action == 105);
  in.buttons = 0;
  s.velocity[0] = 123;
  assert(bk_player_movement(&s, &in, error));
  assert(!s.move_latch && !s.acceleration && s.velocity[0] == 123 &&
         s.action == 100);
  s.action = in.actions[11];
  in.buttons = BK_PLAYER_FORWARD;
  in.look[1] = 40;
  assert(bk_player_movement(&s, &in, error));
  assert(s.action == 111 && s.position[2] == 0 && s.pitch == 0);
  s.interaction_mode = 5;
  s.yaw = 720;
  s.pitch = 100;
  assert(bk_player_movement(&s, &in, error));
  assert(s.yaw == 360 && s.pitch == 80 && s.action == 111);
  BkPlayerMovement held = s;
  in.seconds = NAN;
  assert(!bk_player_movement(&s, &in, error) && !memcmp(&s, &held, sizeof(s)));
  in.seconds = 1;
  in.buttons = 64;
  assert(!bk_player_movement(&s, &in, error) && !memcmp(&s, &held, sizeof(s)));
  puts("PASS player movement old-yaw order, action/latch/pitch gates and "
       "rollback");
}
