#ifndef BK_ARM64_MATH_H
#define BK_ARM64_MATH_H

/* The Switch toolchain targets ARMv8-A, where NEON is part of the base
 * architecture. Keep this header inactive on host/ASan builds so the
 * reference double-precision path remains byte-for-byte stable there. */
#if defined(__aarch64__) && defined(__ARM_NEON) && defined(BK_ARM64_FAST)

#include <arm_neon.h>
#include <math.h>
#include <string.h>

#define BK_ARM64_NEON 1

/* A matrix parent is often the next cache line in a hierarchy walk. This is
 * a hint only; it has no architectural effect if the line is already hot. */
static inline void bk_arm64_prefetch_l1(const void *address) {
  __asm__ volatile("prfm pldl1keep, [%0]" : : "r"(address));
}

static inline void bk_arm64_matrix_multiply(float out[16], const float a[16],
                                            const float b[16]) {
  float result[16];
  float32x4_t b0 = vld1q_f32(b + 0);
  float32x4_t b1 = vld1q_f32(b + 4);
  float32x4_t b2 = vld1q_f32(b + 8);
  float32x4_t b3 = vld1q_f32(b + 12);
  for (unsigned i = 0; i < 4; i++) {
    const float *row = a + i * 4;
    float32x4_t value = vmulq_n_f32(b0, row[0]);
    value = vfmaq_n_f32(value, b1, row[1]);
    value = vfmaq_n_f32(value, b2, row[2]);
    value = vfmaq_n_f32(value, b3, row[3]);
    vst1q_f32(result + i * 4, value);
  }
  memcpy(out, result, sizeof(result));
}

static inline void bk_arm64_matrix_point(float out[4], const float p[3],
                                         const float m[16]) {
  float result[4];
  float32x4_t value = vmulq_n_f32(vld1q_f32(m + 0), p[0]);
  value = vfmaq_n_f32(value, vld1q_f32(m + 4), p[1]);
  value = vfmaq_n_f32(value, vld1q_f32(m + 8), p[2]);
  value = vaddq_f32(value, vld1q_f32(m + 12));
  vst1q_f32(result, value);
  memcpy(out, result, sizeof(result));
}

/* ENVL normals use the authored linear 3x3 rows, without an inverse
 * transpose or normalization. Keep that rule, but do all three lanes in one
 * NEON accumulator while the CPU fallback is active. */
static inline void bk_arm64_matrix_linear3(float out[3], const float v[3],
                                           const float m[16]) {
  float32x4_t value = vmulq_n_f32(vld1q_f32(m + 0), v[0]);
  value = vfmaq_n_f32(value, vld1q_f32(m + 4), v[1]);
  value = vfmaq_n_f32(value, vld1q_f32(m + 8), v[2]);
  float result[4];
  vst1q_f32(result, value);
  out[0] = result[0];
  out[1] = result[1];
  out[2] = result[2];
}

/* The homogeneous divide remains scalar/double so the existing W tolerance
 * and failure policy stay identical. Only the matrix products use NEON. */
static inline int bk_arm64_matrix_transform_coord(float out[3],
                                                  const float p[3],
                                                  const float m[16]) {
  float result[4];
  bk_arm64_matrix_point(result, p, m);
  if (!isfinite(result[3]) || result[3] == 0)
    return 0;
  double delta = (double)result[3] - 1.0;
  if (delta < -(double)1e-5f || delta > (double)1e-5f) {
    double inverse = 1.0 / result[3];
    for (unsigned i = 0; i < 3; ++i)
      result[i] = (float)((double)result[i] * inverse);
  }
  for (unsigned i = 0; i < 3; ++i)
    if (!isfinite(result[i]))
      return 0;
  memcpy(out, result, 3 * sizeof(float));
  return 1;
}

#endif

#endif
