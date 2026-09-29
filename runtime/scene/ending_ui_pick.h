#ifndef BK_SCENE_ENDING_UI_PICK_H
#define BK_SCENE_ENDING_UI_PICK_H
#include <stddef.h>
#include <stdint.h>
/* 4da3ca/4da4f7 read camera node LOCAL (+80), not cached world (+c0)
 * or the controller's yaw. Outside uses the original open interval.
 * Category: outside+special=1, outside=2, inside+special=3, inside=4;
 * special means cached target26/27. Failure preserves both outputs. */
int bk_ending_ui_camera_sector(const float camera_local[16], int32_t cached,
                               int32_t low, int32_t high, int *outside,
                               int *category);
/* Actual4a7b26: projection onto the finite segment, inclusive endpoints,
 * strict perpendicular radius. A degenerate segment is a valid miss.
 * Positive infinity radius is meaningful when a target meets the camera. */
int bk_ending_ui_segment_hit(const float first[2], const float last[2],
                             const float pointer[2], float radius, int *hit);
typedef struct {
  const float (*world)[16]; /* cached node worlds, original721ef4 index */
  const uint8_t *present;
  size_t count;
  const float *camera_position; /* cached71b40c, three values */
  const float *view;            /* device642fa8 */
  const float *projection;      /* device642830, original Y convention */
  const float *viewport_matrix; /* device642af0 */
  float ring_width;             /* slot50 width, not slot51 */
} BkEndingUiPickBindings;
/* One original42d4b6 projection for the action controller. Uses the same
 * matrix/rounding path as target selection; missing node is an explicit
 * error, unlike the selector's original skip of optional candidates. */
int bk_ending_ui_project_target(const BkEndingUiPickBindings *, unsigned node,
                                 int32_t point[2], char error[256]);
/* Complete4da76f with actual42d4b6 projection and4a7b26 selection math.
 * All required original nodes must exist: missing nodes leave native stack
 * distances/radii uninitialized, so that path is explicitly rejected.
 * Zero clip W projects to original(-10000,-10000); negative W remains valid.
 * Strict closest depth wins, ties keep the previous selection/distance.
 * Returns interface success; selected=-1 is a valid miss. Errors preserve
 * both outputs. No publication, target loading or guessed node bindings. */
int bk_ending_ui_pick_targets(const BkEndingUiPickBindings *,
                              const float pointer[2], float *distance,
                              int32_t *selected, char error[256]);
/* Runtime policy for retail models with absent optional endpoint nodes:
 * exclude only segments whose endpoint is absent. All existing endpoints
 * and defined arithmetic retain the strict query's validation and ordering.
 * No coordinates, radii, depths or hits are invented for missing nodes.
 * This replaces undefined native stack reads, not a proven native result.
 * A truncated binding table or invalid present matrix remains an error. */
int bk_ending_ui_pick_available_targets(const BkEndingUiPickBindings *,
                                        const float pointer[2], float *distance,
                                        int32_t *selected, char error[256]);
#endif
