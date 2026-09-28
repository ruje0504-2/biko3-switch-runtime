#ifndef BK_MATRIX_H
#define BK_MATRIX_H
/* Row-major, row-vector convention throughout CPU code; output may alias input.
 * GLSL column-major loading of these same bytes implicitly transposes W*V*P.
 * No extra transpose, position mirroring, or UV flip belongs at upload. */
void bk_matrix_multiply(float out[16], const float a[16], const float b[16]);
void bk_matrix_point(float out[4], const float point[3],
                     const float matrix[16]);
/* Finite nonsingular 4x4 inverse; failure leaves output unchanged. Unlike
 * camera_view this accepts authored constant-W and projective matrices. */
int bk_matrix_inverse(float out[16], const float matrix[16]);
/* Original 0x5235e6 float intermediates; quaternion is NOT normalized.
 * Caller validates finite inputs/results. Translation=0, homogeneous W=1. */
void bk_matrix_quaternion(float out[16], const float quaternion[4]);
/*5234e5: normalized axis, float sine/cosine and native mixed intermediates.
 * Degenerate axis follows the original zero-axis result. Failure holds out. */
int bk_matrix_axis_rotation(float out[16], const float axis[3], float radians);
/* Left-handed +Z forward, +Y up world; positive-height Vulkan viewport.
 * Projection flips clip Y once and maps near/far to Vulkan Z=0/1. */
int bk_matrix_view(float out[16], const float eye[3], float yaw, float pitch);
int bk_matrix_projection(float out[16], float fov_y, float aspect, float near_z,
                         float far_z);
#endif
