#include "scene/inspection_camera.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
int bk_inspection_world(const BkInspectionCamera *c, float world[16]) {
  if (!isfinite(c->yaw) || !isfinite(c->pitch) || !isfinite(c->eye[0]) ||
      !isfinite(c->eye[1]) || !isfinite(c->eye[2]))
    return 0;
  float sy = sinf(c->yaw), cy = cosf(c->yaw), sp = sinf(c->pitch),
        cp = cosf(c->pitch);
  const float matrix[16] = {cy, 0, -sy, 0, -sy * sp, cp, -cy * sp, 0,
                            sy * cp, sp, cy * cp, 0,
                            c->eye[0], c->eye[1], c->eye[2], 1};
  memcpy(world, matrix, sizeof(matrix));
  return 1;
}
void bk_inspection_reset(BkInspectionCamera *c) {
  *c = (BkInspectionCamera){{-48, 24, -45}, 0.830f, -0.152f};
}
static float axis(float v) {
  v = fmaxf(-1, fminf(1, v));
  return fabsf(v) <= 0.15f ? 0 : copysignf((fabsf(v) - 0.15f) / 0.85f, v);
}
int bk_inspection_step(BkInspectionCamera *c, double seconds,
                       const BkInput *input, char error[256]) {
  if (!isfinite(seconds) || seconds <= 0 || seconds > 1 ||
      !isfinite(input->move_x) || !isfinite(input->move_y) ||
      !isfinite(input->look_x) || !isfinite(input->look_y)) {
    snprintf(error, 256, "invalid inspection camera step");
    return 0;
  }
  if (input->pressed & BK_BUTTON_CONFIRM) {
    bk_inspection_reset(c);
    return 1;
  }
  float dt = (float)seconds;
  c->yaw = remainderf(c->yaw + axis(input->look_x) * 1.6f * dt, 6.283185307f);
  c->pitch =
      fmaxf(-1.4f, fminf(1.4f, c->pitch + axis(input->look_y) * 1.3f * dt));
  float x = axis(input->move_x), z = axis(input->move_y), length = hypotf(x, z);
  if (length > 1) {
    x /= length;
    z /= length;
  }
  c->eye[0] += (x * cosf(c->yaw) + z * sinf(c->yaw)) * 30 * dt;
  c->eye[2] += (-x * sinf(c->yaw) + z * cosf(c->yaw)) * 30 * dt;
  c->eye[1] +=
      ((!!(input->held & BK_BUTTON_UP)) - (!!(input->held & BK_BUTTON_DOWN))) *
      20 * dt;
  for (unsigned i = 0; i < 3; i++)
    c->eye[i] = fmaxf(-2000, fminf(2000, c->eye[i]));
  return 1;
}
