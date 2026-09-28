#include "core/matrix.h"
#include "scene/player_hud_session.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
int main(void) {
  const float bk_identity[16] = {1, 0, 0, 0, 0, 1, 0, 0,
                                 0, 0, 1, 0, 0, 0, 0, 1};
  char e[256];
  BkPlayerHudState s = {0};
  assert(bk_player_hud_initialize(&s, 2, 0, 640));
  BkPlayerHudState old = s;
  BkPlayerHudInput in = {.counter = 100};
  for (unsigned i = 0; i < 21; ++i)
    in.actions[i] = (int)i;
  BkPlayerHudFrame f = {0};
  assert(bk_player_hud_draws(&s, &in, 640, &f, e));
  unsigned repeated = 0;
  for (unsigned i = 0; i < f.count; ++i)
    if (f.draws[i].slot == 42) {
      assert(f.draws[i].sprite.x == (repeated ? 610 : 598));
      assert(f.draws[i].sprite.stage == (repeated ? 3 : 2));
      ++repeated;
    }
  assert(repeated == 2 && f.capture && f.capture_after == 4);
  old = s;
  BkPlayerHudFrame previous = f;
  in.counter = INT_MIN;
  assert(!bk_player_hud_draws(&s, &in, 640, &f, e));
  assert(!memcmp(&s, &old, sizeof(s)) && !memcmp(&f, &previous, sizeof(f)));
  uint8_t outcome = 11;
  BkScreenPoint point = {{0, 0}, NAN};
  assert(!bk_player_hud_update(&s, &in, &outcome, &point, 1, 10, 640, e));
  assert(outcome == 11 && !memcmp(&s, &old, sizeof(s)));
  s.sprites[21].x = 17;
  s.sprites[25].timer.armed = 1;
  s.sprites[25].timer.deadline = 300;
  s.sprites[25].blink = 255;
  assert(bk_player_hud_initialize(&s, 4, 1, 1280));
  assert(s.sprites[21].x == 17 && s.sprites[25].timer.armed == 1 &&
         s.sprites[25].timer.deadline == 300 && s.sprites[25].blink == 255);
  BkGameFrameState game = {.camera = {.phase = 1}};
  for (unsigned i = 0; i < 21; ++i)
    game.player_actions[i] = (int)i;
  BkCameraLens lens = {1, .75f, .5f, 126384};
  float view[16];
  memcpy(view, bk_identity, sizeof(view));
  int absent = -1;
  assert(bk_player_hud_session_step(&s, &game, 0, view, &lens, 640, 480, 1, 10,
                                    &f, &absent, e));
  assert(absent == 1);
  old = s;
  previous = f;
  game.prop_interaction.available = 1;
  assert(!bk_player_hud_session_step(&s, &game, 0, view, &lens, 640, 480, 1, 10,
                                     &f, &absent, e));
  assert(!memcmp(&s, &old, sizeof(s)) && !memcmp(&f, &previous, sizeof(f)));
  memcpy(game.prop_interaction.matrix, bk_identity, 64);
  game.prop_interaction.matrix[14] = 20;
  game.player.spatial.scene.npc_in_view = 0;
  s.sprites[5].stage = 3;
  s.extent = 1;
  s.reserve = .001f;
  assert(bk_player_hud_session_step(&s, &game, 0, view, &lens, 640, 480, 1,
                                    1000, &f, &absent, e));
  assert(!absent && game.interaction.outcome == 4 &&
         game.player.completion_requested == 4);
  assert(s.sprites[24].x == 320 && s.sprites[24].y == 240);
  puts("PASS player HUD snapshots, retention, projection and atomic rejection");
}
