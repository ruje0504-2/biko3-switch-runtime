#ifndef BK_SCENE_ENDING_SECONDARY_UI_H
#define BK_SCENE_ENDING_SECONDARY_UI_H
#include "scene/ending_ui.h"
#include "scene/ending_ui_pick.h"
typedef struct {
  BkEndingFrameState *frame;
  int32_t *kind, *column; /*same721e10/7220f0 retained by normal controller*/
  int32_t (*targets)[2]; /*39 projected target points721f90*/
  int32_t *alternate;    /*two projected coordinates6afd38*/
  BkEndingUiSprite *ring; /*slot50, not menu51*/
  const BkEndingUiPickBindings *geometry; /*39 old published worlds*/
  const float *alternate_world; /*719b40; NULL means absent*/
} BkEndingSecondaryPickBindings;
/*47c334 with real42d4b6 projection, distance and strict4a777e circles.
 * Preferred index wins over closer nonpreferred nodes; the optional target's
 * comparison radius150 differs from its final hit/draw radius100. Missing
 * nodes keep their old point slots. One common matrix product per call.
 * Updates shared selection/ring geometry, never draws or publishes poses.
 * Later errors retain earlier point/selection writes. */
int bk_ending_secondary_pick(const BkEndingSecondaryPickBindings *,
                              const float pointer[2], int32_t preferred,
                              int32_t *result, char error[256]);
typedef struct {
  uint32_t width, height; /*actual logical client extent*/
  float scale, menu_width; /*721ad0 and slot51 width7389e8*/
} BkEndingSecondaryMenuGeometry;
/*47cb41: -1 is an ordinary outside point,0 center,1..8 edge/corner zones.
 * Its half-menu-width conversion and rounded positive extents differ from
 * the strict per-element fit in47d0e6. Failure preserves result. */
int bk_ending_secondary_menu_zone(const BkEndingSecondaryMenuGeometry *,
                                   const int32_t point[2], int32_t *result,
                                   char error[256]);
/*47cd36/47d268/47d0e6: choices and three actual centers. Uses authored
 * .01745 radians/degree, sin forX/cos forY, truncation and accumulated angle
 * corrections. Centers/tables are live caller-owned state, no UI allocation.
 * Invalid inputs reject; later failure retains native candidate writes. The
 * original can loop forever for an impossible placement. A search exceeding
 *4096 increments fails explicitly (not a claim that no mathematical fit
 * exists). Supported retail viewport/scale pairs do not reach this guard. */
int bk_ending_secondary_menu(const BkEndingSecondaryMenuGeometry *,
                              int32_t event, int32_t automatic, int32_t selected,
                              const int32_t targets[39][2],
                              const int32_t alternate[2], int32_t choices[3],
                              int32_t points[3][2], char error[256]);
/*495125 shared three-point placement. Original signed32 angle arithmetic,
 * signed remainder and accumulated per-point search corrections. Centers
 * are captured by value; later failure retains already attempted points. */
int bk_ending_radial_menu_place(const BkEndingSecondaryMenuGeometry *,
                                 int32_t angle, int32_t step,
                                 const int32_t center[2], int32_t points[3][2],
                                 char error[256]);
/*4949dc selected-stage setup. Always refresh projected node4 first, even
 *for an unhandled event. Event1 omits the current selection from0..2;
 *event2 offers3 and uses the native bearing and4951b5 fit before placing
 *the10-degree fan. Preserve event2's center-zone order1,0,2 and carried
 *angle/correction; it is not the secondary stage's120-degree layout.
 *Uses the same old-world projection and bounded native placement search.
 *Later failures retain already projected targets/choices/point candidates.*/
int bk_ending_selected_menu(const BkEndingSecondaryMenuGeometry *,
                              const BkEndingUiPickBindings *projection,
                              int32_t event, int32_t selection,
                              int32_t selected_target, int32_t targets[39][2],
                              const int32_t alternate[2], int32_t choices[3],
                              int32_t points[3][2], char error[256]);
#endif
