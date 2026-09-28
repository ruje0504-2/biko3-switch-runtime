#ifndef BK_CAMERA_H
#define BK_CAMERA_H
#include <stdint.h>
/* 4adcd9: blend the two Y-rotation matrices and recover degrees via42ee68.
 * weight is not clamped; negative/extrapolated weights remain meaningful.
 * Does not normalize input degrees or linearly interpolate their numbers. */
int bk_angle_blend_degrees(float *out, float first, float second, float weight);
/* 4ae043: interpolate XYZ and extracted yaw/pitch, discarding roll/scale.
 * Uses42ee68's clamped asin and4adcd9's angular blend. Weight is not clamped;
 * input/output may alias. Invalid arithmetic preserves output. */
int bk_camera_blend_matrix(float out[16], const float target[16],
                           const float previous[16], float weight);
typedef struct {
  float fov_y, height_over_width, near_z, far_z;
} BkCameraLens;
typedef struct {
  uint32_t x, y, width, height;
} BkViewport;
/* Row-vector camera frame -> view. Affine, invertible input only; supports
 * scale/shear as well as rigid poses. Failure leaves output unchanged. */
int bk_camera_view(float view[16], const float world[16]);
/* Original projection convention uses height/width. Flip clip Y exactly once
 * for a positive-height Vulkan viewport; Z remains [0,1]. */
int bk_camera_projection(float projection[16], const BkCameraLens *lens);
/* Largest centered integer-pixel rectangle, with at most one-pixel rounding.
 * Does not stretch the lens or crop the scene to fill a different aspect. */
int bk_camera_fit(BkViewport *viewport, uint32_t width, uint32_t height,
                  uint32_t aspect_width, uint32_t aspect_height);
typedef struct {
  int32_t position[2];
  float depth;
} BkScreenPoint;
/* 42d56c: cached frame origin to original screen coordinates. Compose
 * view*original projection, then viewport, then frame; preserve each float
 * matrix intermediate. Truncate X/Y toward zero, keep finite negative W.
 * lens uses native convention; Vulkan Y flip is removed internally before
 * the screen viewport's own Y inversion. Zero W/out-of-range int32 rejects
 * atomically instead of original undefined float-to-integer results. */
int bk_camera_project_frame(BkScreenPoint *point, const float frame[16],
                            const float view[16], const BkCameraLens *lens,
                            const BkViewport *viewport);
/* World-space mode-0 aim from 0x425196. Retains the input translation; uses
 * Y-up and original normalization thresholds. Degenerate/vertical targets
 * fail without changing output. Input/output may alias. */
int bk_camera_aim(float result[16], const float previous[16],
                  const float target[3]);
typedef struct {
  float world[16], position[3];
} BkCameraFollowPose;
/* CPU pose arithmetic from 0x4bdc12: smooth position toward sampled world
 * point, optionally toward correction at 4*seconds, aim from PREVIOUS world
 * position, then install the smoothed position. Target/track/correction are
 * supplied world points. Does not resolve actors, run collision queries,
 * advance animation, or select a gameplay camera. Failure is transactional. */
int bk_camera_follow_pose(BkCameraFollowPose *pose, const float sampled[3],
                          const float target[3], const float *correction,
                          float seconds);
/* Original 0x4ade8f/0x4be290 handover: position follows at min(2*dt,1),
 * yaw/pitch use the original matrix-angle interpolation at min(4*dt,1).
 * Target has zero pitch and supplied yaw in degrees. Completion is the
 * original inclusive 0.2-unit box around target position, independent of
 * remaining angular error. Failure preserves pose and completion output. */
int bk_camera_transition_pose(BkCameraFollowPose *pose,
                              const float target_position[3], float yaw_degrees,
                              float seconds, int *complete);
#endif
