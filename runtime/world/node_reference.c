#include "world/node_reference.h"
#include "core/matrix.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
static int fail(char *error, const char *why) {
  snprintf(error, 256, "node reference: %s", why);
  return 0;
}
static int finite_values(const float *v, unsigned count) {
  if (!v)
    return 0;
  for (unsigned i = 0; i < count; i++)
    if (!isfinite(v[i]))
      return 0;
  return 1;
}
static int begin(const BkNodeReference *node, const float *reference,
                 float inverse[16], char *error) {
  if (!node || !finite_values(node->local, 16) ||
      !finite_values(node->world, 16) || !finite_values(reference, 16) ||
      !bk_matrix_inverse(inverse, node->parent_world))
    return fail(error, "invalid reference/cached matrices");
  return 1;
}
static int commit(BkNodeReference *node, BkNodeReference *next, char *error) {
  bk_matrix_multiply(next->world, next->local, next->parent_world);
  if (!finite_values(next->local, 16) || !finite_values(next->world, 16))
    return fail(error, "matrix overflow");
  *node = *next;
  return 1;
}
int bk_node_reference_position(BkNodeReference *node, const float reference[16],
                               const float position[3], char error[256]) {
  float inverse[16], translation[16] = {0}, placed[16];
  if (!begin(node, reference, inverse, error) || !finite_values(position, 3))
    return fail(error, "invalid position/matrices");
  translation[0] = translation[5] = translation[10] = translation[15] = 1;
  memcpy(translation + 12, position, 12);
  bk_matrix_multiply(translation, translation, reference);
  memcpy(placed, node->world, 64);
  memcpy(placed + 12, translation + 12, 12);
  bk_matrix_multiply(placed, placed, inverse);
  BkNodeReference next = *node;
  memcpy(next.local + 12, placed + 12, 12);
  return commit(node, &next, error);
}
static void normalize(float p[3]) {
  double norm = (double)p[1] * p[1] + (double)p[2] * p[2] + (double)p[0] * p[0];
  if (norm <= (double)1e-10f)
    memset(p, 0, 12);
  else if (norm - 1 < -(double)1e-5f || norm - 1 > (double)1e-5f) {
    double inverse = 1 / sqrt(norm);
    for (unsigned i = 0; i < 3; i++)
      p[i] = (float)(p[i] * inverse);
  }
}
static void cross(float out[3], const float a[3], const float b[3]) {
  float r[3];
  for (unsigned i = 0; i < 3; i++)
    r[i] = (float)((double)a[(i + 1) % 3] * b[(i + 2) % 3] -
                   (double)a[(i + 2) % 3] * b[(i + 1) % 3]);
  memcpy(out, r, 12);
}
int bk_node_reference_orientation(BkNodeReference *node,
                                  const float reference[16],
                                  const float forward[3], const float up[3],
                                  char error[256]) {
  float inverse[16], f[3], u[3], right[3], rotation[16] = {0};
  if (!begin(node, reference, inverse, error) || !finite_values(forward, 3) ||
      !finite_values(up, 3))
    return fail(error, "invalid direction/matrices");
  memcpy(f, forward, 12);
  memcpy(u, up, 12);
  normalize(f);
  normalize(u);
  cross(right, u, f);
  normalize(right);
  cross(u, f, right);
  normalize(u);
  for (unsigned i = 0; i < 3; i++) {
    rotation[i * 4] = right[i];
    rotation[i * 4 + 1] = u[i];
    rotation[i * 4 + 2] = f[i];
  }
  rotation[15] = 1;
  bk_matrix_multiply(rotation, rotation, reference);
  bk_matrix_multiply(rotation, rotation, inverse);
  BkNodeReference next = *node;
  for (unsigned i = 0; i < 3; i++)
    memcpy(next.local + i * 4, rotation + i * 4, 12);
  return commit(node, &next, error);
}
int bk_node_reference_rotation(BkNodeReference *node, const float reference[16],
                               const float axis[3], float radians,
                               char error[256]) {
  float inv[16], rotation[16], placed[16];
  if (!begin(node, reference, inv, error) ||
      !bk_matrix_axis_rotation(rotation, axis, radians))
    return fail(error, "invalid axis rotation/reference");
  bk_matrix_multiply(placed, rotation, reference);
  memcpy(placed + 12, node->world + 12, 12);
  BkNodeReference next = *node;
  bk_matrix_multiply(next.local, placed, inv);
  return commit(node, &next, error);
}
int bk_node_local_rotation(BkNodeReference *node, int mode, const float axis[3],
                           float radians, char error[256]) {
  float rotation[16], held[16];
  if (!node || !finite_values(node->local, 16) ||
      !finite_values(node->parent_world, 16) ||
      !bk_matrix_axis_rotation(rotation, axis, radians))
    return fail(error, "invalid local rotation");
  BkNodeReference next = *node;
  if (!mode)
    memcpy(next.local, rotation, 64);
  else {
    memcpy(held, node->local, 64);
    memset(held + 12, 0, 12);
    bk_matrix_multiply(next.local, mode == 1 ? rotation : held,
                       mode == 1 ? held : rotation);
    memcpy(next.local + 12, node->local + 12, 12);
  }
  return commit(node, &next, error);
}
int bk_node_reference_offset(float out[3], const float world[16],
                             const float reference[16], char error[256]) {
  float inv[16], local[16];
  if (!out || !finite_values(world, 16) || !bk_matrix_inverse(inv, reference))
    return fail(error, "invalid offset/reference");
  bk_matrix_multiply(local, world, inv);
  if (!finite_values(local, 16))
    return fail(error, "offset overflow");
  memcpy(out, local + 12, 12);
  return 1;
}
int bk_node_reference_aim(BkNodeReference *node, const float target[3],
                          char error[256]) {
  float inverse[16], forward[3], right[3], up[3];
  if (!node || !finite_values(node->world, 16) || !finite_values(target, 3) ||
      !bk_matrix_inverse(inverse, node->parent_world))
    return fail(error, "invalid aim/parent cache");
  for (unsigned i = 0; i < 3; i++)
    forward[i] = (float)((double)target[i] - node->world[12 + i]);
  normalize(forward);
  cross(right, (float[]){0, 1, 0}, forward);
  normalize(right);
  cross(up, forward, right);
  normalize(up);
  BkNodeReference next = *node;
  memcpy(next.world, right, 12);
  memcpy(next.world + 4, up, 12);
  memcpy(next.world + 8, forward, 12);
  bk_matrix_multiply(next.local, next.world, inverse);
  if (!finite_values(next.local, 16) || !finite_values(next.world, 16))
    return fail(error, "aim overflow");
  *node = next;
  return 1;
}
