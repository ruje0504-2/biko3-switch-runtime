#include "game/player_view_policy.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static void wall_feedback(void) {
  char error[256];
  const unsigned rates[] = {15, 30, 53, 60};
  const float pitches[] = {-60, 20, 60};
  const float triangles[2][3][3] = {
      {{-100, 0, -10}, {-100, 40, -10}, {100, 0, -10}},
      {{-100, 40, -10}, {100, 40, -10}, {100, 0, -10}}};
  for (unsigned h = 0; h < 4; h++)
    for (unsigned p = 0; p < 3; p++) {
      float spans[2];
      for (unsigned correct = 0; correct < 2; correct++) {
        BkPlayerView view = {.pose = {.position = {5, 18, -40}},
                             .target_distance = 40,
                             .distance = 40};
        for (unsigned i = 0; i < 4; i++)
          view.pose.world[i * 5] = 1;
        BkPlayerViewInput input = {.position = {5, 0, 0},
                                   .vertical = 18,
                                   .yaw = 25,
                                   .pitch = pitches[p],
                                   .npc_position = {5, 0, 50},
                                   .npc_height = 18,
                                   .npc_vertical = 18,
                                   .seconds = 2.f / rates[h]};
        BkPlayerViewEffects effects;
        assert(bk_player_view_step(&view, BK_PLAYER_VIEW_ORBIT, &input,
                                   &effects, error));
        float lo = INFINITY, hi = -INFINITY;
        for (unsigned frame = 0; frame < 400; frame++) {
          BkPlayerWall wall = {.position = {5, 0, 0},
                               .camera_distance = view.target_distance};
          BkPlayerWallInput query = {.previous = {5, 0, 0}, .height = 18};
          assert(bk_player_view_collision_query(&view, &query));
          if (!correct)
            memcpy(query.camera, view.pose.world + 12, 12);
          int near;
          for (unsigned t = 0; t < 2; t++)
            assert(bk_player_wall_triangle(&wall, &near, &query, triangles[t],
                                           error));
          assert(!wall.singular_camera);
          view.target_distance = wall.camera_distance;
          assert(bk_player_view_step(&view, BK_PLAYER_VIEW_ORBIT, &input,
                                     &effects, error));
          if (frame >= 200) {
            lo = fminf(lo, view.pose.world[14]);
            hi = fmaxf(hi, view.pose.world[14]);
          }
        }
        spans[correct] = hi - lo;
        BkPlayerWallInput output = {.height = 123}, saved = output;
        view.probe[0] = NAN;
        assert(!bk_player_view_collision_query(&view, &output));
        assert(!memcmp(&output, &saved, sizeof(output)));
      }
      assert(spans[0] > .001f && spans[1] < .00001f);
    }
}
int main(void) {
  wall_feedback();
  char error[256];
  int32_t actions[21];
  for (unsigned i = 0; i < 21; ++i)
    actions[i] = (int32_t)i;
  BkPlayerViewFlags flags = {0, 9};
  BkPlayerViewKind route;
  assert(bk_player_view_route(&flags, &route, 2, 0, 0, actions));
  assert(flags.hidden == 0 && route == BK_PLAYER_VIEW_HOLD);
  flags.mode = 1;
  assert(bk_player_view_route(&flags, &route, 2, 0, 0, actions) &&
         route == BK_PLAYER_VIEW_HEAD);
  flags.mode = 6;
  assert(bk_player_view_route(&flags, &route, 0, 1, 0, actions) &&
         route == BK_PLAYER_VIEW_HOLD && flags.mode == 0);
  BkPlayerView s = {
      .pose = {.world = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 4, 20, 6, 1},
               .position = {100, 200, 300}},
      .focus = {15, 5, 9},
      .lean = 10};
  s.matrix[0] = s.matrix[5] = s.matrix[10] = s.matrix[15] = 1;
  for (unsigned i = 0; i < 8; ++i) {
    s.blocked[i] = 1;
    s.rays[i][0] = (float)i;
  }
  BkPlayerViewInput in = {.position = {10, 0, 20},
                          .vertical = 18,
                          .head = {11, 21, 22},
                          .yaw = 90,
                          .pitch = 20,
                          .seconds = .1f};
  BkPlayerViewEffects out;
  assert(bk_player_view_step(&s, BK_PLAYER_VIEW_RAY, &in, &out, error));
  assert(s.selected_ray == 6 && s.pose.world[12] == 6 &&
         s.pose.position[0] == 100 && s.matrix[12] == 0);
  for (unsigned i = 0; i < 8; ++i)
    assert(!s.blocked[i]);
  /* PROP_HEAD writes return yaw and root visibility; WALL deliberately holds
   * return yaw and stored pitch while using a zero-pitch rendered matrix. */
  assert(bk_player_view_step(&s, BK_PLAYER_VIEW_PROP_HEAD, &in, &out, error));
  assert(out.root_hidden == 1 && out.write_return_yaw &&
         s.pose.position[1] == 21);
  s.pitch = 37;
  assert(bk_player_view_step(&s, BK_PLAYER_VIEW_WALL, &in, &out, error));
  assert(!out.write_return_yaw && s.pitch == 37 && s.pose.position[1] == 20 &&
         s.pose.world[9] == 0);
  /* Cover lean latches without input; left wins simultaneous directions. */
  in.buttons = 3;
  assert(bk_player_view_step(&s, BK_PLAYER_VIEW_COVER, &in, &out, error));
  assert(s.lean == -10 && s.pitch == 0);
  in.buttons = 0;
  assert(bk_player_view_step(&s, BK_PLAYER_VIEW_COVER, &in, &out, error) &&
         s.lean == -10);
  /* Aim degeneracy is rejected atomically instead of publishing NaNs. */
  memcpy(s.focus, s.pose.world + 12, 12);
  BkPlayerView before = s;
  BkPlayerViewEffects held = out;
  assert(!bk_player_view_step(&s, BK_PLAYER_VIEW_RAY, &in, &out, error));
  assert(!memcmp(&s, &before, sizeof(s)) && !memcmp(&out, &held, sizeof(out)));
  puts("PASS player view dispatch, cached candidate, return-yaw and lean "
       "semantics");
  return 0;
}
