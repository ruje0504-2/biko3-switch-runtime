#include "core/camera.h"
#include "core/matrix.h"
#include <math.h>
#include <string.h>
int bk_camera_view(float view[16], const float world[16]) {
  if (!view || !world)
    return 0;
  for (unsigned i = 0; i < 16; i++)
    if (!isfinite(world[i]))
      return 0;
  if (world[3] != 0 || world[7] != 0 || world[11] != 0 || world[15] != 1)
    return 0;
  double a[3][6] = {0};
  for (unsigned i = 0; i < 3; i++) {
    for (unsigned j = 0; j < 3; j++)
      a[i][j] = world[i * 4 + j];
    a[i][i + 3] = 1;
  }
  for (unsigned c = 0; c < 3; c++) {
    unsigned pivot = c;
    for (unsigned i = c + 1; i < 3; i++)
      if (fabs(a[i][c]) > fabs(a[pivot][c]))
        pivot = i;
    if (fabs(a[pivot][c]) < 1e-12)
      return 0;
    for (unsigned j = 0; j < 6; j++) {
      double tmp = a[c][j];
      a[c][j] = a[pivot][j];
      a[pivot][j] = tmp;
    }
    double divisor = a[c][c];
    for (unsigned j = 0; j < 6; j++)
      a[c][j] /= divisor;
    for (unsigned i = 0; i < 3; i++) {
      if (i == c)
        continue;
      double factor = a[i][c];
      for (unsigned j = 0; j < 6; j++)
        a[i][j] -= factor * a[c][j];
    }
  }
  float result[16] = {0};
  for (unsigned j = 0; j < 3; j++) {
    double translation = 0;
    for (unsigned i = 0; i < 3; i++) {
      result[i * 4 + j] = (float)a[i][j + 3];
      translation -= world[12 + i] * a[i][j + 3];
    }
    result[12 + j] = (float)translation;
  }
  result[15] = 1;
  for (unsigned i = 0; i < 16; i++)
    if (!isfinite(result[i]))
      return 0;
  memcpy(view, result, sizeof(result));
  return 1;
}
int bk_camera_projection(float projection[16], const BkCameraLens *lens) {
  if (!projection || !lens || !isfinite(lens->fov_y) ||
      !isfinite(lens->height_over_width) || !isfinite(lens->near_z) ||
      !isfinite(lens->far_z) || lens->fov_y <= 0 || lens->fov_y >= 3.13f ||
      lens->height_over_width <= 0 || lens->near_z <= 0 ||
      lens->far_z <= lens->near_z)
    return 0;
  /* 0x523a10 stores sin and cos as float before division. */
  float half = lens->fov_y * .5f;
  double scale = (double)(float)cos((double)half) / (float)sin((double)half);
  double z = (double)lens->far_z / ((double)lens->far_z - lens->near_z);
  float result[16] = {0};
  result[0] = (float)(scale * lens->height_over_width);
  result[5] = (float)-scale;
  result[10] = (float)z;
  result[11] = 1;
  result[14] = (float)(-lens->near_z * z);
  for (unsigned i = 0; i < 16; i++)
    if (!isfinite(result[i]))
      return 0;
  memcpy(projection, result, sizeof(result));
  return 1;
}
int bk_camera_fit(BkViewport *viewport, uint32_t width, uint32_t height,
                  uint32_t aspect_width, uint32_t aspect_height) {
  if (!viewport || !width || !height || !aspect_width || !aspect_height)
    return 0;
  uint32_t w = width, h = height;
  if ((uint64_t)width * aspect_height > (uint64_t)height * aspect_width)
    w = (uint32_t)((uint64_t)height * aspect_width / aspect_height);
  else
    h = (uint32_t)((uint64_t)width * aspect_height / aspect_width);
  if (!w || !h)
    return 0;
  *viewport = (BkViewport){(width - w) / 2, (height - h) / 2, w, h};
  return 1;
}
int bk_camera_project_frame(BkScreenPoint *point, const float frame[16],
                            const float view[16], const BkCameraLens *lens,
                            const BkViewport *viewport) {
  if (!point || !frame || !view || !viewport || !viewport->width ||
      !viewport->height)
    return 0;
  for (unsigned i = 0; i < 16; ++i)
    if (!isfinite(frame[i]) || !isfinite(view[i]))
      return 0;
  float projection[16];
  if (!bk_camera_projection(projection, lens))
    return 0;
  projection[5] = -projection[5];
  float screen[16] = {0}, vp[16], vps[16], projected[16];
  screen[0] = (float)(viewport->width * .5);
  screen[5] = (float)(viewport->height * -.5);
  screen[10] = screen[15] = 1;
  screen[12] = (float)((double)viewport->x + screen[0]);
  screen[13] = (float)((double)viewport->y - screen[5]);
  bk_matrix_multiply(vp, view, projection);
  bk_matrix_multiply(vps, vp, screen);
  bk_matrix_multiply(projected, frame, vps);
  for (unsigned i = 0; i < 16; ++i)
    if (!isfinite(projected[i]))
      return 0;
  if (!projected[15])
    return 0;
  double x = (double)projected[12] / projected[15];
  double y = (double)projected[13] / projected[15];
  float depth = (float)((double)projected[14] / projected[15]);
  if (!isfinite(x) || !isfinite(y) || !isfinite(depth) ||
      trunc(x) < INT32_MIN || trunc(x) > INT32_MAX || trunc(y) < INT32_MIN ||
      trunc(y) > INT32_MAX)
    return 0;
  *point = (BkScreenPoint){{(int32_t)x, (int32_t)y}, depth};
  return 1;
}

/* 0x522922 skips renormalization in a +/-1e-5 band around unit norm. */
static int normalize(float v[3]) {
  double norm = (double)v[1] * v[1] + (double)v[2] * v[2] + (double)v[0] * v[0];
  if (!isfinite(norm) || norm <= (double)1e-10f)
    return 0;
  if (norm - 1 < -(double)1e-5f || norm - 1 > (double)1e-5f) {
    double inverse = 1 / sqrt(norm);
    for (unsigned i = 0; i < 3; i++)
      v[i] = (float)(v[i] * inverse);
  }
  return 1;
}
static void cross(float out[3], const float a[3], const float b[3]) {
  for (unsigned i = 0; i < 3; i++) {
    unsigned j = (i + 1) % 3, k = (i + 2) % 3;
    out[i] = (float)((double)a[j] * b[k] - (double)a[k] * b[j]);
  }
}
int bk_camera_aim(float result[16], const float previous[16],
                  const float target[3]) {
  if (!result || !previous || !target)
    return 0;
  for (unsigned i = 0; i < 16; i++)
    if (!isfinite(previous[i]))
      return 0;
  if (previous[3] != 0 || previous[7] != 0 || previous[11] != 0 ||
      previous[15] != 1)
    return 0;
  float forward[3], right[3], up[3], world[16];
  const float axis[3] = {0, 1, 0};
  for (unsigned i = 0; i < 3; i++) {
    if (!isfinite(target[i]))
      return 0;
    forward[i] = (float)((double)target[i] - previous[12 + i]);
  }
  if (!normalize(forward))
    return 0;
  cross(right, axis, forward);
  if (!normalize(right))
    return 0;
  cross(up, forward, right);
  if (!normalize(up))
    return 0;
  memcpy(world, previous, sizeof(world));
  memcpy(world, right, sizeof(right));
  memcpy(world + 4, up, sizeof(up));
  memcpy(world + 8, forward, sizeof(forward));
  memcpy(result, world, sizeof(world));
  return 1;
}
int bk_camera_follow_pose(BkCameraFollowPose *pose, const float sampled[3],
                          const float target[3], const float *correction,
                          float seconds) {
  if (!pose || !sampled || !isfinite(seconds) || seconds < 0)
    return 0;
  BkCameraFollowPose next = *pose;
  if (!bk_camera_aim(next.world, pose->world, target))
    return 0;
  for (unsigned i = 0; i < 3; i++) {
    if (!isfinite(sampled[i]) || !isfinite(pose->position[i]) ||
        (correction && !isfinite(correction[i])))
      return 0;
    /* The original stores subtraction and multiplication separately. */
    float difference = (float)((double)sampled[i] - pose->position[i]);
    float delta = (float)((double)difference * seconds);
    next.position[i] = (float)((double)delta + pose->position[i]);
    if (correction) {
      difference = (float)((double)correction[i] - next.position[i]);
      delta = (float)((double)difference * (4 * (double)seconds));
      next.position[i] = (float)((double)delta + next.position[i]);
    }
    if (!isfinite(next.position[i]))
      return 0;
    next.world[12 + i] = next.position[i];
  }
  *pose = next;
  return 1;
}

/* 0x42ee68 uses this rounded float pi, including in atan2/asin conversion. */
static float transition_degrees(double radians) {
  float angle = (float)(radians * 180.0 / 3.141592025756836);
  if (angle < -180)
    angle = (float)((double)angle + 360);
  else if (angle > 180)
    angle = (float)((double)angle - 360);
  return angle;
}
static float transition_lerp(float target, float old, float weight) {
  float delta = (float)((double)target - old);
  delta = (float)((double)delta * weight);
  return (float)((double)old + delta);
}
static void transition_sincos(float degrees, float *s, float *c) {
  float radians = (float)((double)degrees * 0.01745329238474369f);
  *s = (float)sin((double)radians);
  *c = (float)cos((double)radians);
}
static float transition_angle(float target, float old, float weight) {
  float ts, tc, os, oc;
  transition_sincos(target, &ts, &tc);
  transition_sincos(old, &os, &oc);
  float s = transition_lerp(ts, os, weight);
  float c = transition_lerp(tc, oc, weight);
  return transition_degrees(atan2((double)s, (double)c));
}
int bk_angle_blend_degrees(float *out, float first, float second,
                           float weight) {
  if (!out || !isfinite(first) || !isfinite(second) || !isfinite(weight))
    return 0;
  float result = transition_angle(first, second, weight);
  if (!isfinite(result))
    return 0;
  *out = result;
  return 1;
}
int bk_camera_blend_matrix(float out[16], const float target[16],
                           const float previous[16], float weight) {
  if (!out || !target || !previous || !isfinite(weight))
    return 0;
  for (unsigned i = 0; i < 16; ++i)
    if (!isfinite(target[i]) || !isfinite(previous[i]))
      return 0;
  float yaw[2], pitch[2];
  const float *matrices[2] = {target, previous};
  for (unsigned i = 0; i < 2; ++i) {
    const float *m = matrices[i];
    yaw[i] = transition_degrees(atan2((double)m[8], (double)m[10]));
    float vertical = -m[9];
    if (vertical < -1)
      vertical = -1;
    if (vertical > 1)
      vertical = 1;
    pitch[i] = transition_degrees(asin((double)vertical));
  }
  float sy, cy, sp, cp;
  transition_sincos(transition_angle(yaw[0], yaw[1], weight), &sy, &cy);
  transition_sincos(transition_angle(pitch[0], pitch[1], weight), &sp, &cp);
  const float x[16] = {1, 0, 0, 0, 0, cp, sp, 0, 0, -sp, cp, 0, 0, 0, 0, 1};
  const float y[16] = {cy, 0, -sy, 0, 0, 1, 0, 0, sy, 0, cy, 0, 0, 0, 0, 1};
  float result[16];
  bk_matrix_multiply(result, x, y);
  for (unsigned i = 0; i < 3; ++i)
    result[12 + i] = transition_lerp(target[12 + i], previous[12 + i], weight);
  for (unsigned i = 0; i < 16; ++i)
    if (!isfinite(result[i]))
      return 0;
  memcpy(out, result, sizeof(result));
  return 1;
}
int bk_camera_transition_pose(BkCameraFollowPose *pose,
                              const float target_position[3], float yaw_degrees,
                              float seconds, int *complete) {
  float inverse[16];
  if (!pose || !target_position || !complete || !isfinite(yaw_degrees) ||
      !isfinite(seconds) || seconds < 0 ||
      !bk_camera_view(inverse, pose->world))
    return 0;
  for (unsigned i = 0; i < 3; i++)
    if (!isfinite(target_position[i]))
      return 0;
  float target_s, target_c;
  transition_sincos(yaw_degrees, &target_s, &target_c);
  float target_yaw =
      transition_degrees(atan2((double)target_s, (double)target_c));
  float old_yaw = transition_degrees(
      atan2((double)pose->world[8], (double)pose->world[10]));
  float vertical = -pose->world[9];
  if (vertical < -1)
    vertical = -1;
  if (vertical > 1)
    vertical = 1;
  float old_pitch = transition_degrees(asin((double)vertical));
  float position_weight = seconds >= .5f ? 1 : (float)((double)seconds * 2);
  float rotation_weight = seconds >= .25f ? 1 : (float)((double)seconds * 4);
  float yaw = transition_angle(target_yaw, old_yaw, rotation_weight);
  float pitch = transition_angle(0, old_pitch, rotation_weight);
  float sy, cy, sp, cp;
  transition_sincos(yaw, &sy, &cy);
  transition_sincos(pitch, &sp, &cp);
  const float x[16] = {1, 0, 0, 0, 0, cp, sp, 0, 0, -sp, cp, 0, 0, 0, 0, 1};
  const float y[16] = {cy, 0, -sy, 0, 0, 1, 0, 0, sy, 0, cy, 0, 0, 0, 0, 1};
  BkCameraFollowPose next = {0};
  bk_matrix_multiply(next.world, x, y);
  int arrived = 1;
  for (unsigned i = 0; i < 3; i++) {
    float p = transition_lerp(target_position[i], pose->world[12 + i],
                              position_weight);
    if (!isfinite(p))
      return 0;
    next.world[12 + i] = next.position[i] = p;
    if ((double)p < (double)target_position[i] - .2 ||
        (double)p > (double)target_position[i] + .2)
      arrived = 0;
  }
  if (!bk_camera_view(inverse, next.world))
    return 0;
  *pose = next;
  *complete = arrived;
  return 1;
}
