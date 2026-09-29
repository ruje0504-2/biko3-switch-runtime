#include "world/ending_camera.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static BkMenuCamera initial(void) {
  BkMenuCamera s = {.yaw = 0,
                    .pitch = 0,
                    .radius = 140,
                    .height = -20,
                    .focus = {71, 72, 73},
                    .fov = 1};
  for (unsigned i = 0; i < 16; ++i)
    s.pose.world[i] = s.matrix[i] = i % 5 == 0;
  return s;
}
int main(void) {
  char e[256];
  const float zero[2] = {0, 0}, offset[3] = {3, 4, 5};
  BkMenuCamera s = initial();
  assert(bk_ending_camera_orbit(&s, zero, 0, offset, 0x10, 0, e));
  assert(s.yaw == 360 && s.radius == 120 && s.height == -10 && s.fov == .2f);
  assert(s.pose.position[1] == -6 && s.focus[0] == 71);
  assert(bk_ending_camera_orbit(&s, zero, 0, offset, 0x38, 0, e));
  assert(s.yaw == 0 && s.radius == 100 && s.height == 0 && s.fov == .4f);
  assert(s.pose.position[0] == 3 && s.pose.position[1] == 4 &&
         s.pose.position[2] == -95);
  const float motion[2] = {10, -20};
  assert(bk_ending_camera_orbit(&s, motion, 3, offset, 0x10, .5f, e));
  assert(s.yaw == 20 && s.pitch == -40 && s.radius == 100 && s.height == 0);
  assert(bk_ending_camera_orbit(&s, motion, 2, offset, 0x10, .5f, e));
  assert(s.radius == 105 && s.height == 10);
  /* Fixed camera deliberately preserves parameters outside orbit clamps. */
  s = initial();
  s.yaw = 720;
  s.pitch = 120;
  assert(bk_ending_camera_fixed(&s, offset, e));
  assert(s.yaw == 720 && s.pitch == 120 && s.radius == 140 && s.height == -20);
  assert(s.fov == .2f && s.focus[0] == 71);
  assert(!memcmp(s.pose.world, s.matrix, sizeof(s.matrix)));
  BkMenuCamera old = s;
  assert(!bk_ending_camera_fixed(&s, (float[3]){NAN, 0, 0}, e));
  assert(!memcmp(&old, &s, sizeof(s)));
  assert(!bk_ending_camera_orbit(&s, motion, 4, offset, 0x10, 0, e));
  assert(!memcmp(&old, &s, sizeof(s)));
  assert(!bk_ending_camera_orbit(&s, motion, 1, offset, 0x10, INFINITY, e));
  assert(!memcmp(&old, &s, sizeof(s)));
  /* Offset may point into the old state; no partial overwrite. */
  BkMenuCamera alias = s;
  float copied[3];
  memcpy(copied, s.pose.position, sizeof(copied));
  assert(bk_ending_camera_fixed(&s, s.pose.position, e));
  assert(bk_ending_camera_fixed(&alias, copied, e));
  assert(!memcmp(&s, &alias, sizeof(s)));
  s = initial();
  const float track[3] = {10, 20, 30}, target[3] = {0, 18, 0};
  BkMenuCamera opening = s;
  opening.fov = .731f;
  assert(bk_ending_camera_opening_pose(&opening, track, target, .5f, e));
  assert(opening.fov == .731f && opening.focus[0] == 71);
  assert(bk_ending_camera_track_pose(&s, track, target, .5f, e));
  assert(!memcmp(&s.pose, &opening.pose, sizeof(s.pose)) &&
         !memcmp(s.matrix, opening.matrix, sizeof(s.matrix)));
  assert(s.pose.position[0] == 5 && s.pose.position[1] == 10 &&
         s.pose.position[2] == 15 && s.focus[0] == 71 && s.fov == .2f);
  old = s;
  assert(!bk_ending_camera_track_pose(&s, track, track, 1, e));
  assert(!memcmp(&old, &s, sizeof(s)));
  assert(!bk_ending_camera_track_pose(&s, track, target, -1, e));
  assert(!memcmp(&old, &s, sizeof(s)));
  /* The original factor is not clamped at1: a long step can overshoot. */
  assert(bk_ending_camera_track_pose(&s, track, target, 2, e));
  assert(s.pose.position[0] == 15 && s.pose.position[1] == 30 &&
         s.pose.position[2] == 45 && s.focus[0] == 71);
  s = initial();
  s.yaw = 0;
  s.pitch = -240;
  s.height = -140;
  assert(bk_ending_camera_manual(&s, zero, 0, offset, target, 0, e));
  assert(s.yaw == 360 && s.pitch == -180 && s.radius == 120 &&
         s.height == -100 && s.fov == .2f && s.focus[0] == 71);
  /* The cached matrix deliberately differs from the final aimed world. */
  assert(memcmp(s.matrix, s.pose.world, sizeof(s.matrix)));
  assert(!memcmp(s.matrix + 12, s.pose.position, sizeof(s.pose.position)));
  s = initial();
  s.radius = -10;
  s.height = 0;
  old = s;
  assert(!bk_ending_camera_manual(&s, zero, 0, offset, offset, 0, e));
  assert(!memcmp(&old, &s, sizeof(s)));
  BkEndingCameraPresets presets = {
      .active = {{0, 90, 180}, {0, 20, -40}, {100, 40, 60}, {0, 10, -10}}};
  BkEndingCameraTransitions clocks = {0, .75f, 1};
  BkEndingCameraPresetGate gate = {.state_ee0 = 4};
  s = initial();
  old = s;
  int complete = -1;
  assert(bk_ending_camera_preset(&s, &clocks, &presets, 0, offset, &gate, .25f,
                                 &complete, e));
  assert(!complete && clocks.preset_progress == .25f &&
         clocks.zoom_progress == .75f && clocks.zoom_fov == 1 && s.fov == .2f);
  assert(s.pose.position[0] == .75f && s.pose.position[1] == 1 &&
         s.pose.position[2] == -23.75f);
  assert(!memcmp(s.matrix, old.matrix, sizeof(s.matrix)));
  assert(bk_ending_camera_preset(&s, &clocks, &presets, 0, offset, &gate, .25f,
                                 &complete, e));
  /* Halfway from the retained source, not halfway from the last output. */
  assert(s.pose.position[2] == -47.5f && !complete);
  assert(bk_ending_camera_preset_zoom(&s, &clocks, &presets, 0, offset, 0x10,
                                      .25f, &complete, e));
  assert(complete && clocks.zoom_progress == 0 &&
         clocks.preset_progress == .5f && clocks.zoom_fov == .85f);
  assert(!memcmp(s.pose.world, s.matrix, sizeof(s.matrix)));
  assert(s.yaw == old.yaw && s.pitch == old.pitch && s.radius == old.radius &&
         s.height == old.height &&
         !memcmp(s.focus, old.focus, sizeof(s.focus)));
  assert(bk_ending_camera_preset_zoom(&s, &clocks, &presets, 2, offset, 0x38, 0,
                                      &complete, e));
  assert(!complete && clocks.zoom_fov == .85f && s.fov == .4f);
  assert(bk_ending_camera_preset_zoom(&s, &clocks, &presets, 2, offset, 0x10, 2,
                                      &complete, e));
  assert(complete && clocks.zoom_fov == .4f && s.fov == .4f);
  old = s;
  BkEndingCameraTransitions old_clocks = clocks;
  complete = 7;
  assert(!bk_ending_camera_preset(&s, &clocks, &presets, 3, offset, &gate, .1f,
                                  &complete, e));
  assert(!memcmp(&s, &old, sizeof(s)) && complete == 7 &&
         !memcmp(&clocks, &old_clocks, sizeof(clocks)));
  presets.active[2][0] = INFINITY;
  assert(!bk_ending_camera_preset_zoom(&s, &clocks, &presets, 0, offset, 0x10,
                                       .1f, &complete, e));
  assert(!memcmp(&s, &old, sizeof(s)) && complete == 7 &&
         !memcmp(&clocks, &old_clocks, sizeof(clocks)));
  float invalid[16];
  memcpy(invalid, s.matrix, sizeof(invalid));
  invalid[9] = NAN;
  assert(!bk_camera_blend_matrix(s.matrix, invalid, s.matrix, .5f));
  assert(!memcmp(&s, &old, sizeof(s)));
  puts("ending camera input/cache/preset/clocks/alias/atomic failures passed");
  return 0;
}
