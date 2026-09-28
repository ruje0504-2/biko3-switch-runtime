#include "core/matrix.h"
#include "world/orbit_internal.h"
#include <math.h>
#include <string.h>
int bk_orbit_matrix(float out[16], float yaw_degrees, float pitch_degrees,
                    float radius, float height, const float *offset) {
  if (!out || !isfinite(yaw_degrees) || !isfinite(pitch_degrees) ||
      !isfinite(radius) || !isfinite(height))
    return 0;
  if (offset)
    for (unsigned i = 0; i < 3; ++i)
      if (!isfinite(offset[i]))
        return 0;
  const float degrees = .01745329238474369f;
  /* Positional trig consumes the unrounded degree product; D3DX rotation
   * receives a rounded float-radian argument. Preserve both stages. */
  double pitch = (double)pitch_degrees * degrees;
  double yaw = (double)yaw_degrees * degrees;
  float cp = (float)cos(pitch), sp = (float)sin(pitch);
  float x = (float)((double)cp * radius * sin(yaw));
  float z = (float)((double)cp * radius * cos(yaw));
  float position[3] = {-x, (float)((double)sp * radius + height), -z};
  float rx = (float)pitch, ry = (float)yaw;
  float sx = (float)sin((double)rx), cx = (float)cos((double)rx);
  float sy = (float)sin((double)ry), cy = (float)cos((double)ry);
  const float a[16] = {1, 0, 0, 0, 0, cx, sx, 0, 0, -sx, cx, 0, 0, 0, 0, 1};
  const float b[16] = {cy, 0, -sy, 0, 0, 1, 0, 0, sy, 0, cy, 0, 0, 0, 0, 1};
  const float t[16] = {1, 0, 0, 0, 0,           1,           0,           0,
                       0, 0, 1, 0, position[0], position[1], position[2], 1};
  float translation[16], rotation[16], result[16];
  if (offset) {
    const float o[16] = {1, 0, 0, 0, 0,         1,         0,         0,
                         0, 0, 1, 0, offset[0], offset[1], offset[2], 1};
    bk_matrix_multiply(translation, o, t);
  } else
    memcpy(translation, t, sizeof(t));
  bk_matrix_multiply(rotation, a, b);
  bk_matrix_multiply(result, rotation, translation);
  for (unsigned i = 0; i < 16; ++i)
    if (!isfinite(result[i]))
      return 0;
  memcpy(out, result, sizeof(result));
  return 1;
}
