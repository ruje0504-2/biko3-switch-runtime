#include "core/arm64_math.h"
#include "core/matrix.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>

#ifndef BK_ARM64_NEON
#error This regression must execute the ARM64 NEON implementation.
#endif

static void equal(const float *a, const float *b, unsigned count) {
  for (unsigned i = 0; i < count; ++i)
    assert(a[i] == b[i]);
}

int main(void) {
  /* original_skin_weights_oracle.py, seed 0x522b0d, case 1032.
   * Expected values come from the fixed EXE's 522d9a/522b0d/522bd9.
   * Single-precision FMA perturbed W and exceeded the existing 3e-6 bound. */
  const float p[3] = {0x1.26e35p+4f, -0x1.19572cp+4f, 0x1.1ebaa8p+4f};
  const float m[16] = {
      -0x1.5dc61cp+0f, 0x1.3b103cp+1f, 0x1.fe02b4p+1f, 0x1.65ed3p-3f,
      -0x1.f7792ap+1f, -0x1.fd78a4p-3f, -0x1.76fd8p+1f, 0x1.3a4fc8p-3f,
      -0x1.d216cap+1f, -0x1.0830a2p+1f, -0x1.2d1526p-3f, -0x1.5dca22p-4f,
      0x1.993e76p+0f, 0x1.1130aap+1f, -0x1.cf6608p+0f, 0x1.fffebp-1f};
  const float point[4] = {-0x1.3ac7eap+4f, 0x1.dc5ba8p+3f,
                           0x1.e20492p+6f, -0x1.042592p-7f};
  const float coord[3] = {0x1.35c366p+11f, -0x1.d4c3cep+10f, -0x1.da559ep+13f};
  const float linear[3] = {-0x1.545bdp+4f, 0x1.980f7ep+3f, 0x1.e9422cp+6f};
  float a[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
  memcpy(a + 12, p, sizeof(p));
  float expected[16], out[16], alias[16];
  memcpy(expected, m, sizeof(m));
  memcpy(expected + 12, point, sizeof(point));
  bk_matrix_multiply(out, a, m);
  equal(out, expected, 16);
  memcpy(alias, a, sizeof(a));
  bk_matrix_multiply(alias, alias, m);
  equal(alias, expected, 16);
  memcpy(alias, m, sizeof(m));
  bk_matrix_multiply(alias, a, alias);
  equal(alias, expected, 16);

  bk_matrix_point(out, p, m);
  equal(out, point, 4);
  memcpy(alias, p, sizeof(p));
  bk_matrix_point(alias, alias, m);
  equal(alias, point, 4);
  assert(bk_matrix_transform_coord(out, p, m));
  equal(out, coord, 3);
  assert(bk_arm64_matrix_transform_coord(out, p, m));
  equal(out, coord, 3);
  memcpy(alias, p, sizeof(p));
  assert(bk_arm64_matrix_transform_coord(alias, alias, m));
  equal(alias, coord, 3);
  bk_arm64_matrix_linear3(out, p, m);
  equal(out, linear, 3);
  memcpy(alias, p, sizeof(p));
  bk_arm64_matrix_linear3(alias, alias, m);
  equal(alias, linear, 3);

  memcpy(alias, m, sizeof(m));
  alias[3] = alias[7] = alias[11] = alias[15] = 0;
  memcpy(out, coord, sizeof(coord));
  assert(!bk_arm64_matrix_transform_coord(out, p, alias));
  equal(out, coord, 3);
  puts("PASS ARM64 NEON: original cancellation, matrix aliases, W rejection");
  return 0;
}
