#include "world/eye_pose.h"
#include "core/matrix.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
static int fail(char error[256], const char *reason) {
  snprintf(error, 256, "eye pose: %s", reason);
  return 0;
}
static int finite_matrix(const float m[16]) {
  for (unsigned i = 0; i < 16; i++)
    if (!isfinite(m[i]))
      return 0;
  return 1;
}
static void identity(float m[16]) {
  memset(m, 0, 64);
  m[0] = m[5] = m[10] = m[15] = 1;
}
static float euler_radians(double angle) {
  /* 4a74c0 uses a less precise literal pi, then wraps as stored float
   * degrees. Preserve this round trip before the caller's radian clamp. */
  float degrees = (float)(angle / (double)0x1.921fbp+1f * 180);
  if (degrees < 0)
    degrees = (float)((double)degrees + 360);
  if (degrees >= 360)
    degrees = (float)((double)degrees - 360);
  float radians = (float)((double)degrees * 0.01745329238474369f);
  if (radians >= 3.1415927410125732f)
    radians = (float)((double)radians - 6.2831854820251465f);
  return radians;
}
static void rotate(BkEyePoseFrame *eye, const float inverse_parent[16],
                   unsigned axis, float angle) {
  float r[16], next[16];
  identity(r);
  float c = (float)cos((double)angle), s = (float)sin((double)angle);
  if (axis == 1) {
    r[0] = r[10] = c;
    r[2] = -s;
    r[8] = s;
  } else {
    r[5] = r[10] = c;
    r[6] = s;
    r[9] = -s;
  }
  bk_matrix_multiply(next, r, eye->world);
  memcpy(next + 12, eye->world + 12, 12);
  bk_matrix_multiply(eye->local, next, inverse_parent);
  bk_matrix_multiply(eye->world, eye->local, eye->parent_world);
}
int bk_eye_pose_aim(BkEyePoseFrame eyes[2], const float target[16],
                    uint32_t mode, uint32_t variant, float pitch_limit,
                    float yaw_limit, char error[256]) {
  if (!eyes || !target || variant > 1 || !isfinite(pitch_limit) ||
      !isfinite(yaw_limit) || pitch_limit < 0 || yaw_limit < 0 ||
      !finite_matrix(target))
    return fail(error, "invalid target, variant or limits");
  BkEyePoseFrame next[2];
  memcpy(next, eyes, sizeof(next));
  float inverse_parent[2][16], pitch[2], yaw[2];
  unsigned basis = mode ? variant : 1 - variant;
  for (unsigned i = 0; i < 2; i++) {
    BkEyePoseFrame *e = &next[i];
    float inverse_eye[16], relative[16];
    if (!finite_matrix(e->local) || !bk_matrix_inverse(inverse_eye, e->world) ||
        !bk_matrix_inverse(inverse_parent[i], e->parent_world))
      return fail(error, "invalid or singular eye matrix");
    bk_matrix_multiply(relative, target, inverse_eye);
    float p[3];
    memcpy(p, relative + 12, 12);
    double norm =
        (double)p[1] * p[1] + (double)p[2] * p[2] + (double)p[0] * p[0];
    if (!isfinite(norm))
      return fail(error, "target transform overflow");
    if (norm <= (double)1e-10f)
      memset(p, 0, sizeof(p));
    else if (norm - 1 < -(double)1e-5f || norm - 1 > (double)1e-5f) {
      double inverse = 1 / sqrt(norm);
      for (unsigned j = 0; j < 3; j++)
        p[j] = (float)(p[j] * inverse);
    }
    float x_scale = basis ? (mode ? 20 : 30) : (mode ? -30 : -20);
    float y_scale = basis ? (mode ? -10 : -30) : (mode ? 30 : 10);
    float a = (float)((double)p[0] * x_scale),
          b = (float)((double)p[1] * y_scale);
    a = (float)((double)a * 0.01745329238474369f);
    b = (float)((double)b * 0.01745329238474369f);
    float ah = a * .5f, bh = b * .5f;
    double cy = (float)cos((double)ah), sy = (float)sin((double)ah);
    double cp = (float)cos((double)bh), sp = (float)sin((double)bh);
    float q[4] = {(float)(cy * sp), (float)(sy * cp), (float)(-sy * sp),
                  (float)(cy * cp)},
          rotation[16];
    bk_matrix_quaternion(rotation, q);
    pitch[i] = euler_radians(asin(-(double)rotation[9]));
    yaw[i] = euler_radians(atan2((double)rotation[8], rotation[10]));
    if (!isfinite(pitch[i]) || !isfinite(yaw[i]))
      return fail(error, "angle overflow");
    pitch[i] = fminf(pitch_limit, fmaxf(-pitch_limit, pitch[i]));
    yaw[i] = fminf(yaw_limit, fmaxf(-yaw_limit, yaw[i]));
  }
  for (unsigned i = 0; i < 2; i++) {
    BkEyePoseFrame *e = &next[i];
    float translation[3];
    memcpy(translation, e->local + 12, 12);
    identity(e->local);
    memcpy(e->local + 12, translation, 12);
    bk_matrix_multiply(e->world, e->local, e->parent_world);
    rotate(e, inverse_parent[i], 1, yaw[i]);
    rotate(e, inverse_parent[i], 0, pitch[i]);
    if (!finite_matrix(e->local) || !finite_matrix(e->world))
      return fail(error, "pose overflow");
  }
  memcpy(eyes, next, sizeof(next));
  return 1;
}
