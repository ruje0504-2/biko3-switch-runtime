#include "core/matrix.h"
#include <math.h>
#include <string.h>
int bk_matrix_axis_rotation(float out[16], const float axis[3], float radians) {
  if (!out || !axis || !isfinite(radians))
    return 0;
  float v[3];
  for (unsigned i = 0; i < 3; i++) {
    if (!isfinite(axis[i]))
      return 0;
    v[i] = axis[i];
  }
  double n = (double)v[1] * v[1] + (double)v[2] * v[2] + (double)v[0] * v[0];
  if (n <= (double)1e-10f)
    memset(v, 0, sizeof(v));
  else if (n - 1 < -(double)1e-5f || n - 1 > (double)1e-5f) {
    double inv = 1 / sqrt(n);
    for (unsigned i = 0; i < 3; i++)
      v[i] = (float)(v[i] * inv);
  }
  float c = (float)cos((double)radians), s = (float)sin((double)radians),
        k = (float)(1 - (double)c);
  double x = v[0], y = v[1], z = v[2], xy = x * y * k, xs = x * s, zs = z * s;
  float yz = (float)(y * z * k), xz = (float)(x * z * k), ys = (float)(y * s);
  float m[16] = {(float)(x * x * k + c),
                 (float)(xy + zs),
                 (float)(xz - y * s),
                 0,
                 (float)(xy - zs),
                 (float)(y * y * k + c),
                 (float)(yz + xs),
                 0,
                 (float)((double)xz + ys),
                 (float)(yz - xs),
                 (float)(z * z * k + c),
                 0,
                 0,
                 0,
                 0,
                 1};
  memcpy(out, m, sizeof(m));
  return 1;
}
int bk_matrix_inverse(float out[16], const float matrix[16]) {
  if (!out || !matrix)
    return 0;
  double m[16];
  for (unsigned i = 0; i < 16; i++) {
    if (!isfinite(matrix[i]))
      return 0;
    m[i] = matrix[i];
  }
  /* 0x522eea: adjugate with deliberately mixed stored-float and live x87
   * minors. A generic double inverse changes near-zero target directions. */
  float s01 = (float)(m[0] * m[5] - m[4] * m[1]);
  float s02 = (float)(m[0] * m[9] - m[8] * m[1]);
  float s03 = (float)(m[0] * m[13] - m[12] * m[1]);
  float s12 = (float)(m[4] * m[9] - m[8] * m[5]);
  float s13 = (float)(m[4] * m[13] - m[12] * m[5]);
  float s23 = (float)(m[8] * m[13] - m[12] * m[9]);
  float c[16];
  c[15] = (float)(s12 * m[2] - s02 * m[6] + s01 * m[10]);
  c[14] = (float)(s03 * m[6] - s01 * m[14] - s13 * m[2]);
  c[13] = (float)(s23 * m[2] - s03 * m[10] + s02 * m[14]);
  c[12] = (float)(s13 * m[10] - s12 * m[14] - s23 * m[6]);
  c[11] = (float)(s02 * m[7] - s01 * m[11] - s12 * m[3]);
  c[10] = (float)(s13 * m[3] - s03 * m[7] + s01 * m[15]);
  c[9] = (float)(s03 * m[11] - s02 * m[15] - s23 * m[3]);
  c[8] = (float)(s23 * m[7] - s13 * m[11] + s12 * m[15]);
  float t01 = (float)(m[2] * m[7] - m[6] * m[3]);
  float t02 = (float)(m[2] * m[11] - m[10] * m[3]);
  double t03 = m[2] * m[15] - m[14] * m[3];
  double t12 = m[6] * m[11] - m[10] * m[7];
  double t13 = m[6] * m[15] - m[14] * m[7];
  double t23 = m[10] * m[15] - m[14] * m[11];
  c[3] = (float)(t02 * m[5] - t01 * m[9] - t12 * m[1]);
  c[2] = (float)(t13 * m[1] - t03 * m[5] + t01 * m[13]);
  c[1] = (float)(t03 * m[9] - t02 * m[13] - t23 * m[1]);
  c[0] = (float)(t23 * m[5] - t13 * m[9] + t12 * m[13]);
  c[7] = (float)(t12 * m[0] - t02 * m[4] + t01 * m[8]);
  c[6] = (float)(t03 * m[4] - t01 * m[12] - (double)(float)t13 * m[0]);
  c[5] = (float)((double)(float)t23 * m[0] - t03 * m[8] + t02 * m[12]);
  c[4] = (float)((double)(float)t13 * m[8] - (double)(float)t12 * m[12] -
                 (double)(float)t23 * m[4]);
  double determinant = c[2] * m[8] + c[1] * m[4] + c[3] * m[12] + c[0] * m[0];
  if (!isfinite(determinant) || determinant == 0)
    return 0;
  float inverse = (float)(1 / determinant), result[16];
  if (!isfinite(inverse) || inverse == 0)
    return 0;
  for (unsigned i = 0; i < 16; i++) {
    result[i] = (float)((double)inverse * c[i]);
    if (!isfinite(result[i]))
      return 0;
  }
  memcpy(out, result, sizeof(result));
  return 1;
}
void bk_matrix_quaternion(float out[16], const float q[4]) {
  double x = q[0], y = q[1], z = q[2], w = q[3];
  float xx = (float)(2 * x * x), yy = (float)(2 * y * y),
        xy = (float)(2 * x * y);
  float xz = (float)(2 * x * z), yz = (float)(2 * y * z),
        xw = (float)(2 * x * w), yw = (float)(2 * y * w),
        zw = (float)(2 * z * w);
  double zz = 2 * z * z;
  float diagonal = (float)(1.0 - xx);
  float result[16] = {(float)(1.0 - yy - zz),
                      (float)((double)xy + zw),
                      (float)((double)xz - yw),
                      0,
                      (float)((double)xy - zw),
                      (float)(1.0 - xx - zz),
                      (float)((double)yz + xw),
                      0,
                      (float)((double)xz + yw),
                      (float)((double)yz - xw),
                      (float)((double)diagonal - yy),
                      0,
                      0,
                      0,
                      0,
                      1};
  memcpy(out, result, sizeof(result));
}
void bk_matrix_multiply(float out[16], const float a[16], const float b[16]) {
  float result[16];
  for (unsigned i = 0; i < 4; i++)
    for (unsigned j = 0; j < 4; j++) {
      double value = 0;
      for (unsigned k = 0; k < 4; k++)
        value += (double)a[i * 4 + k] * b[k * 4 + j];
      result[i * 4 + j] = (float)value;
    }
  memcpy(out, result, sizeof(result));
}
void bk_matrix_point(float out[4], const float p[3], const float m[16]) {
  float result[4];
  for (unsigned j = 0; j < 4; j++)
    result[j] = (float)((double)p[0] * m[j] + (double)p[1] * m[4 + j] +
                        (double)p[2] * m[8 + j] + m[12 + j]);
  memcpy(out, result, sizeof(result));
}
int bk_matrix_transform_coord(float out[3], const float p[3], const float m[16]) {
  if (!out || !p || !m)
    return 0;
  float result[4];
  bk_matrix_point(result, p, m);
  if (!isfinite(result[3]) || result[3] == 0)
    return 0;
  double delta = (double)result[3] - 1.0;
  if (delta < -(double)1e-5f || delta > (double)1e-5f) {
    double inverse = 1.0 / result[3];
    for (unsigned i = 0; i < 3; i++)
      result[i] = (float)(result[i] * inverse);
  }
  for (unsigned i = 0; i < 3; i++)
    if (!isfinite(result[i]))
      return 0;
  memcpy(out, result, 3 * sizeof(float));
  return 1;
}
int bk_matrix_view(float out[16], const float eye[3], float yaw, float pitch) {
  if (!isfinite(yaw) || !isfinite(pitch) || !isfinite(eye[0]) ||
      !isfinite(eye[1]) || !isfinite(eye[2]))
    return 0;
  float sy = sinf(yaw), cy = cosf(yaw), sp = sinf(pitch), cp = cosf(pitch);
  float r[3] = {cy, 0, -sy}, u[3] = {-sy * sp, cp, -cy * sp},
        f[3] = {sy * cp, sp, cy * cp};
  memset(out, 0, 16 * sizeof(float));
  for (unsigned i = 0; i < 3; i++) {
    out[i * 4] = r[i];
    out[i * 4 + 1] = u[i];
    out[i * 4 + 2] = f[i];
  }
  for (unsigned j = 0; j < 3; j++)
    for (unsigned i = 0; i < 3; i++)
      out[12 + j] -= eye[i] * out[i * 4 + j];
  out[15] = 1;
  for (unsigned i = 0; i < 16; i++)
    if (!isfinite(out[i]))
      return 0;
  return 1;
}
int bk_matrix_projection(float out[16], float fov, float aspect, float near_z,
                         float far_z) {
  if (!isfinite(fov) || !isfinite(aspect) || !isfinite(near_z) ||
      !isfinite(far_z) || fov <= 0 || fov >= 3.13f || aspect <= 0 ||
      near_z <= 0 || far_z <= near_z)
    return 0;
  memset(out, 0, 16 * sizeof(float));
  float scale = 1 / tanf(fov / 2);
  out[0] = scale / aspect;
  out[5] = -scale;
  out[10] = far_z / (far_z - near_z);
  out[11] = 1;
  out[14] = -near_z * out[10];
  for (unsigned i = 0; i < 16; i++)
    if (!isfinite(out[i]))
      return 0;
  return 1;
}
