#include "world/ending_camera.h"
#include "core/matrix.h"
#include "world/orbit_internal.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
static int fail(char e[256]) {
  snprintf(e, 256, "ending camera: invalid input/arithmetic/aim geometry");
  return 0;
}
static int finite_values(const float *v, unsigned count) {
  for (unsigned i = 0; i < count; ++i)
    if (!isfinite(v[i]))
      return 0;
  return 1;
}
static float clamp(float x, float lo, float hi) {
  return x >= hi ? hi : x <= lo ? lo : x;
}
static int preset_pose(BkMenuCamera *s, float *progress,
                       const BkEndingCameraPresets *presets, unsigned choice,
                       const float offset[3], float seconds, int *complete) {
  if (!presets || choice >= 3 || !offset || !finite_values(offset, 3) ||
      !isfinite(*progress) || !isfinite(seconds) || seconds < 0)
    return 0;
  float target[16];
  if (!bk_orbit_matrix(target, presets->active[0][choice],
                       presets->active[1][choice], presets->active[2][choice],
                       presets->active[3][choice], NULL))
    return 0;
  for (unsigned i = 0; i < 3; ++i)
    target[12 + i] = (float)((double)target[12 + i] + offset[i]);
  float next = (float)((double)*progress + seconds);
  if (next >= 1)
    next = 1;
  if (!bk_camera_blend_matrix(s->pose.world, target, s->matrix, next))
    return 0;
  memcpy(s->pose.position, s->pose.world + 12, sizeof(s->pose.position));
  *complete = next >= 1;
  if (*complete) {
    memcpy(s->matrix, s->pose.world, sizeof(s->matrix));
    next = 0;
  }
  *progress = next;
  return 1;
}
int bk_ending_camera_preset(BkMenuCamera *s, BkEndingCameraTransitions *t,
                            const BkEndingCameraPresets *presets,
                            unsigned choice, const float offset[3],
                            const BkEndingCameraPresetGate *gate, float seconds,
                            int *complete, char e[256]) {
  if (!s || !t || !gate || !complete)
    return fail(e);
  BkMenuCamera n = *s;
  BkEndingCameraTransitions nt = *t;
  int done;
  if (!preset_pose(&n, &nt.preset_progress, presets, choice, offset, seconds,
                   &done))
    return fail(e);
  if (gate->previous_flow == 0x18
          ? gate->phase != 8 && gate->state_ed8 != 2 && gate->state_ed8 != 3 &&
                gate->state_ed8 != 4
          : gate->state_ee4 != 4 && gate->state_eec != 4 &&
                gate->state_ef0 != 4 && gate->state_ee0 == 4 &&
                gate->state_719b20 == 0)
    n.fov = .2f;
  *s = n;
  *t = nt;
  *complete = done;
  return 1;
}
int bk_ending_camera_preset_zoom(BkMenuCamera *s, BkEndingCameraTransitions *t,
                                 const BkEndingCameraPresets *presets,
                                 unsigned choice, const float offset[3],
                                 uint8_t flow, float seconds, int *complete,
                                 char e[256]) {
  if (!s || !t || !complete || (flow == 0x10 && !isfinite(t->zoom_fov)))
    return fail(e);
  BkMenuCamera n = *s;
  BkEndingCameraTransitions nt = *t;
  int done;
  if (!preset_pose(&n, &nt.zoom_progress, presets, choice, offset, seconds,
                   &done))
    return fail(e);
  if (flow == 0x10) {
    /* The x87 temporary FST does not pop: subtract the unrounded product. */
    nt.zoom_fov = (float)((double)nt.zoom_fov - (double)seconds * .6);
    if ((double)nt.zoom_fov <= .4)
      nt.zoom_fov = .4f;
    if (!isfinite(nt.zoom_fov))
      return fail(e);
    n.fov = nt.zoom_fov;
  } else
    n.fov = .4f;
  *s = n;
  *t = nt;
  *complete = done;
  return 1;
}
int bk_ending_camera_root_rotation(float out[16], const float local[16],
                                   int32_t degrees) {
  if (!out || !local || !finite_values(local, 16))
    return 0;
  float radians = (float)((double)degrees * .01745329238474369f);
  float c = (float)cos((double)radians), s = (float)sin((double)radians);
  float t = (float)(1.0 - c);
  float rotation[16] = {c, 0, -s, 0, 0, (float)((double)t + c), 0, 0, s, 0, c,
                        0, 0, 0,  0, 1};
  float base[16], result[16];
  memcpy(base, local, sizeof(base));
  base[12] = base[13] = base[14] = 0;
  bk_matrix_multiply(result, rotation, base);
  memcpy(result + 12, local + 12, 3 * sizeof(float));
  if (!finite_values(result, 16))
    return 0;
  memcpy(out, result, sizeof(result));
  return 1;
}
static int adjust(BkMenuCamera *n, const float motion[2], unsigned buttons,
                  float seconds) {
  if (!motion || !finite_values(motion, 2) || !isfinite(seconds) ||
      seconds < 0 || (buttons & ~3u) || !isfinite(n->yaw) ||
      !isfinite(n->pitch) || !isfinite(n->radius) || !isfinite(n->height))
    return 0;
  if (buttons & 1) {
    n->yaw = (float)((double)n->yaw + (double)seconds * 4 * motion[0]);
    n->pitch = (float)((double)n->pitch + (double)seconds * 4 * motion[1]);
  } else if (buttons & 2) {
    n->radius = (float)((double)n->radius + (double)motion[0] * seconds);
    n->height = (float)((double)n->height - (double)motion[1] * seconds);
  }
  if (!isfinite(n->yaw) || !isfinite(n->pitch) || !isfinite(n->radius) ||
      !isfinite(n->height))
    return 0;
  if (n->yaw >= 360)
    n->yaw = (float)((double)n->yaw - 360);
  else if (n->yaw <= 0)
    n->yaw = (float)((double)n->yaw + 360);
  return 1;
}
int bk_ending_camera_orbit(BkMenuCamera *s, const float motion[2],
                           unsigned buttons, const float offset[3],
                           uint8_t flow, float seconds, char e[256]) {
  if (!s || !offset || !finite_values(offset, 3))
    return fail(e);
  BkMenuCamera n = *s;
  if (!adjust(&n, motion, buttons, seconds))
    return fail(e);
  n.pitch = clamp(n.pitch, -80, 80);
  n.radius = clamp(n.radius, 5, flow == 0x10 ? 120 : 100);
  n.height = clamp(n.height, flow == 0x10 ? -10 : 0, 30);
  if (!bk_orbit_matrix(n.matrix, n.yaw, n.pitch, n.radius, n.height, offset))
    return fail(e);
  memcpy(n.pose.position, n.matrix + 12, sizeof(n.pose.position));
  memcpy(n.pose.world, n.matrix, sizeof(n.matrix));
  n.fov = flow == 0x10 ? .2f : .4f;
  *s = n;
  return 1;
}
int bk_ending_camera_manual(BkMenuCamera *s, const float motion[2],
                            unsigned buttons, const float offset[3],
                            const float target[3], float seconds, char e[256]) {
  if (!s || !offset || !target || !finite_values(offset, 3) ||
      !finite_values(target, 3))
    return fail(e);
  BkMenuCamera n = *s;
  if (!adjust(&n, motion, buttons, seconds))
    return fail(e);
  n.pitch = clamp(n.pitch, -180, 180);
  n.radius = clamp(n.radius, 0, 120);
  n.height = clamp(n.height, -100, 100);
  if (!bk_orbit_matrix(n.matrix, n.yaw, n.pitch, n.radius, n.height, offset) ||
      !bk_camera_aim(n.pose.world, n.matrix, target))
    return fail(e);
  memcpy(n.pose.position, n.matrix + 12, sizeof(n.pose.position));
  n.fov = .2f;
  *s = n;
  return 1;
}
int bk_ending_camera_fixed(BkMenuCamera *s, const float offset[3],
                           char e[256]) {
  if (!s || !offset)
    return fail(e);
  BkMenuCamera n = *s;
  if (!bk_orbit_matrix(n.matrix, n.yaw, n.pitch, n.radius, n.height, offset))
    return fail(e);
  memcpy(n.pose.position, n.matrix + 12, sizeof(n.pose.position));
  memcpy(n.pose.world, n.matrix, sizeof(n.matrix));
  n.fov = .2f;
  *s = n;
  return 1;
}
int bk_ending_camera_track_pose(BkMenuCamera *s, const float track[3],
                                const float target[3], float seconds,
                                char e[256]) {
  if (!s || !track || !target || !finite_values(track, 3) ||
      !finite_values(target, 3) || !finite_values(s->pose.position, 3) ||
      !isfinite(seconds) || seconds < 0)
    return fail(e);
  BkMenuCamera n = *s;
  for (unsigned i = 0; i < 3; ++i) {
    float delta = (float)((double)track[i] - s->pose.position[i]);
    delta = (float)((double)seconds * delta);
    n.pose.position[i] = (float)((double)s->pose.position[i] + delta);
  }
  if (!finite_values(n.pose.position, 3))
    return fail(e);
  memcpy(n.pose.world + 12, n.pose.position, sizeof(n.pose.position));
  if (!bk_camera_aim(n.pose.world, n.pose.world, target))
    return fail(e);
  memcpy(n.matrix, n.pose.world, sizeof(n.matrix));
  n.fov = .2f;
  *s = n;
  return 1;
}
