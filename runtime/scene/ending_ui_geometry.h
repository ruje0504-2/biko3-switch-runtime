#ifndef BK_SCENE_ENDING_UI_GEOMETRY_H
#define BK_SCENE_ENDING_UI_GEOMETRY_H
#include "scene/ending_stage_ui.h"
/* Original479f15: mutate the existing line63, retaining animation/timer state.
 * Pass the selected7220c8 point; selection and bounds belong to the caller.
 * The47da89 caller passes (float)(400.0 * scale) as length. Each caller owns
 * its independent retained scroll (6afd28 versus6bbe44).
 * No visibility request, animation advance, draw or resource load occurs.
 * Requires a loaded line, positive authored width and finite nonnegative
 * length/dt. Invalid inputs or nonfinite arithmetic leave both owners intact.
 */
int bk_ending_ui_line(BkEndingStageUi *, const int32_t anchor[2],
                      const int32_t pointer[2], float length, float seconds,
                      float *scroll, char error[256]);
/* Original4a777e uses a STRICT radius comparison. A valid miss sets hit=0
 * and preserves distance; return0 means invalid/nonfinite geometry, with
 * both outputs unchanged. Positive infinity is a valid radius when an
 * original target meets the camera. Inputs share the same screen space. */
int bk_ending_ui_circle_hit(const float center[2], float radius,
                            const float point[2], int *hit, float *distance);
#endif
