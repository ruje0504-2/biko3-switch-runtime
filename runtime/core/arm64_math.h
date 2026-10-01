#ifndef BK_ARM64_MATH_H
#define BK_ARM64_MATH_H

/* The Switch toolchain enables this ARM64 path. Host validation can enable
 * the same code with BK_ARM64_FAST; other builds retain the scalar path. */
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
  /* Float products are exact in double. Keep the scalar accumulation order
   * and narrow only at the end, including cancellation in the W column. */
  for (unsigned j = 0; j < 4; j += 2) {
    float64x2_t b0 = vcvt_f64_f32(vld1_f32(b + j));
    float64x2_t b1 = vcvt_f64_f32(vld1_f32(b + 4 + j));
    float64x2_t b2 = vcvt_f64_f32(vld1_f32(b + 8 + j));
    float64x2_t b3 = vcvt_f64_f32(vld1_f32(b + 12 + j));
    for (unsigned i = 0; i < 4; i++) {
      const float *row = a + i * 4;
      float64x2_t value = vfmaq_n_f64(vdupq_n_f64(0), b0, row[0]);
      value = vfmaq_n_f64(value, b1, row[1]);
      value = vfmaq_n_f64(value, b2, row[2]);
      value = vfmaq_n_f64(value, b3, row[3]);
      vst1_f32(result + i * 4 + j, vcvt_f32_f64(value));
    }
  }
  memcpy(out, result, sizeof(result));
}

static inline float64x2_t bk_arm64_linear_pair(const float v[3],
                                              const float m[16], unsigned j) {
  float64x2_t value = vmulq_n_f64(vcvt_f64_f32(vld1_f32(m + j)), v[0]);
  value = vfmaq_n_f64(value, vcvt_f64_f32(vld1_f32(m + 4 + j)), v[1]);
  return vfmaq_n_f64(value, vcvt_f64_f32(vld1_f32(m + 8 + j)), v[2]);
}

static inline void bk_arm64_matrix_point(float out[4], const float p[3],
                                         const float m[16]) {
  float result[4];
  for (unsigned j = 0; j < 4; j += 2) {
    float64x2_t value = bk_arm64_linear_pair(p, m, j);
    value = vaddq_f64(value, vcvt_f64_f32(vld1_f32(m + 12 + j)));
    vst1_f32(result + j, vcvt_f32_f64(value));
  }
  memcpy(out, result, sizeof(result));
}

/* ENVL normals use the authored linear 3x3 rows, without an inverse
 * transpose or normalization. Accumulate each pair in double before storing
 * the float result, as in the reference position and normal functions. */
static inline void bk_arm64_matrix_linear3(float out[3], const float v[3],
                                           const float m[16]) {
  float result[4];
  for (unsigned j = 0; j < 4; j += 2)
    vst1_f32(result + j, vcvt_f32_f64(bk_arm64_linear_pair(v, m, j)));
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
