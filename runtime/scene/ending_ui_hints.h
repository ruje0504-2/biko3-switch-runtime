#ifndef BK_SCENE_ENDING_UI_HINTS_H
#define BK_SCENE_ENDING_UI_HINTS_H
#include "game/ending_auxiliary.h"
#include "model/clip.h"
#include "scene/ending_ui_geometry.h"
#include <stddef.h>
typedef struct {
  float length;             /*719b18*/
  float fixed_scroll;       /*6bbe44*/
  float variable_scroll;    /*6afd28*/
  int32_t movement_ready;   /*6afd14,47a026 computes;47a033 resets*/
  uint8_t once_flags;       /*70c8cc; preserve all but bit0*/
  float meter_x;            /*6e9fa4*/
  uint8_t meter_once_flags; /*6ddea4*/
} BkEndingUiHintsState;
typedef struct {
  const BkEndingFrameState *frame;
  const BkEndingControlState *control;
  const BkEndingAuxiliaryState *auxiliary;
  const int32_t *stage3_state; /*721ee8*/
  const int32_t *active_clip;  /*actual primary actor+140*/
  const int32_t (*points)[2];  /*three7220c8 points*/
  const int32_t *choices;      /*three7220e4 values; -1 skips the point*/
  const int32_t (*targets)[2]; /*live721f90 table; caller supplies its extent*/
  size_t target_count;
  const int32_t *alternate;          /*two6afd38/3c values*/
  const float *gauge_y;              /*live721e24*/
  const BkClipTiming *active_timing; /*current active clip start/source,
                                    read only by4971bd selected motions*/
} BkEndingUiHintsBindings;
/* Original4d49a4..4d49d3, called BEFORE pointer capture/toolbar. One-time
 * initialization does not reset scroll/ready and retains other flag bits. */
int bk_ending_ui_hints_start(BkEndingUiHintsState *, float scale,
                             char error[256]);
/* Complete4d4ed8..4d5e60, including actual4971bd: four branch-specific
 * point/line/ring/image loops, movement readiness, gauge and motion meter.
 * Motion is the float output of
 *4b757e, NOT raw integer bit patterns or pointer coordinates. Growth uses
 * motion once per call without an extra seconds factor.
 * Only reads required bindings at their original branch. Resource/geometry
 * or capacity failure retains the ordered prefix and terminates the frame.
 * No actor/target projections, input acquisition, cursor or reload implied. */
int bk_ending_ui_hints(BkEndingUi *, BkEndingStageUi *, BkEndingUiHintsState *,
                       const BkEndingUiHintsBindings *, const float pointer[2],
                       const float motion[2], float scale, float seconds,
                       BkEndingUiFrame *, char error[256]);
#endif
