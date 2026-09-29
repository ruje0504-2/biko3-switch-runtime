#ifndef BK_SCENE_ENDING_TARGET_H
#define BK_SCENE_ENDING_TARGET_H
#include "game/ending_auxiliary.h"
#include "scene/ending_ui.h"
#include "scene/ending_ui_pick.h"
/*4da594: independent action-table variant721e04 and scene byte721b3d.
 * Other scene bytes have no eligible targets. Original unknown group rows
 * remain the zero-initialized table. The progress comparison is ordered. */
int bk_ending_target_eligible(uint8_t group, int32_t action_variant,
                               uint8_t scene_variant, float progress,
                               int32_t node);
typedef struct {
  BkEndingFrameState *frame;
  const BkEndingControlState *control;
  const BkEndingAuxiliaryState *auxiliary;
  const int32_t *active_clip;
  const int32_t *actions; /*709db8,16 rows of5 live words*/
  int32_t (*targets)[2];  /*721f90,39 entries; missing nodes retain old XY*/
  int32_t *action_kind, *action_column; /*721e10/7220f0*/
  BkEndingUiSprite *ring; /*live slot50,738770*/
  const float *camera_local;
  const BkEndingUiPickBindings *geometry;
} BkEndingTargetBindings;
/* Complete4dac12: project existing nodes, choose by screen-circle distance
 * with original eligibility and camera-side gates, update action row/ring.
 * Does not sample or publish worlds. Equal screen distances choose the later
 * eligible node; this differs from the separate4da76f line fallback.
 * result is original EAX, not interface success:0 can still publish a cached
 * target with action_kind3. All39 bindings must be supplied; absent nodes
 * are the original explicit skip. Matrices remain held across the query, so
 * common view/projection/viewport products are computed once.
 * Original matched actions with scene_variant outside0/5 read an undefined
 * stack flag. Reject at that boundary, preserving prior target updates.
 */
int bk_ending_target_step(const BkEndingTargetBindings *,
                            const float pointer[2], int *result,
                            char error[256]);
#endif
