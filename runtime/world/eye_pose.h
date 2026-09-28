#ifndef BK_WORLD_EYE_POSE_H
#define BK_WORLD_EYE_POSE_H
#include <stdint.h>
typedef struct {
  float local[16], world[16], parent_world[16];
} BkEyePoseFrame;
/* Original 4a0823. Input world/parent_world are the PREVIOUSLY published
 * caches, independently of submitted locals. Both eyes' local and world
 * are replaced; parent caches stay intact. No child traversal is performed.
 * Texture mode is boolean; variant is 0/1 and unrelated to texture choice.
 * Limits are nonnegative radians. Degenerate target direction becomes 0.
 * Finite/singular/invalid arguments fail atomically. Caller skips this when
 * either named eye is absent, matching the original early return. */
int bk_eye_pose_aim(BkEyePoseFrame eyes[2], const float target_world[16],
                    uint32_t texture_mode, uint32_t variant, float pitch_limit,
                    float yaw_limit, char error[256]);
#endif
