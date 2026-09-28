#ifndef BK_WORLD_PLAYER_WALL_H
#define BK_WORLD_PLAYER_WALL_H
#include <stdint.h>
typedef struct {
  float position[3], camera_distance, normal[3];
  int32_t rays_blocked[7], near_wall;
  /* Diagnostics for original undefined/nonfinite geometry, accumulated.
   * Singular camera intersections leave the best distance unchanged.
   * A first zero-length XZ edge has no projection: its endpoint correction
   * is skipped, instead of reading original uninitialized stack memory.
   * Later zero-length edges retain an earlier valid projection, as native. */
  uint32_t singular_camera, missing_projection;
} BkPlayerWall;
typedef struct {
  float previous[3];
  float motion[3]; /* velocity X, velocity Z, yaw delta (not velocity Y) */
  float camera[3], rays[7][3];
  float height;
} BkPlayerWallInput;
int bk_player_wall_validate(const BkPlayerWall *state,
                            const BkPlayerWallInput *input);
/* 4b4f31: one ordered triangle response, including camera/sensor queries
 * before position correction. near is the native return (radius+1), distinct
 * from near_wall (projected radius6 or actual projected contact). Does not
 * clear accumulated flags/normal. Finite overflow/invalid input fails with
 * both outputs unchanged. See explicit undefined-geometry policy above. */
int bk_player_wall_triangle(BkPlayerWall *state, int *near,
                            const BkPlayerWallInput *input,
                            const float triangle[3][3], char error[256]);
/* 4b5f21: second pass; overlapping projected edge resets X/Z to previous.
 * Vertical lower gate is position.Y+15, versus +10 in first pass. */
int bk_player_wall_restore(float position[3], const float previous[3],
                           float height, const float triangle[3][3],
                           char error[256]);
#endif
