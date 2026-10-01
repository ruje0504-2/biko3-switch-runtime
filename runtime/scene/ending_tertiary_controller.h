#ifndef BK_SCENE_ENDING_TERTIARY_CONTROLLER_H
#define BK_SCENE_ENDING_TERTIARY_CONTROLLER_H
#include "game/ending_tertiary_action.h"
#include "game/ending_tertiary_motion.h"
#include "scene/ending_audio.h"
#include "scene/ending_secondary_ui.h"
#include "scene/ending_tertiary_assets.h"

/* Only the third controller's independent process fields. Shared721e/6afd
 * aliases remain in the caller's existing state and arrive through bindings. */
typedef struct {
  BkEndingTertiaryControlState control;
  BkEndingTertiaryActionState action;
  BkEndingTertiaryMotionState motion;
} BkEndingTertiaryControllerRetained;
void bk_ending_tertiary_controller_initialize(BkEndingTertiaryControllerRetained *);
typedef struct {
  BkEndingTertiaryAssets *assets;
  BkEndingAudio *audio;
  BkEndingTertiaryControllerRetained *retained;
  BkEndingCameraTransitions *transitions;
  const BkEndingCameraPresets *presets;
  BkEndingUi *ui;
  const BkEndingUiPickBindings *geometry; /*39 old published node worlds*/
  uint32_t width, height; /*logical content extent, no letterbox origin*/
  char (*speech_names)[32]; /*same two retained722224/722344 names*/
  int32_t (*targets)[2], (*points)[2]; /*same39 targets and three menus*/
  int32_t *action_kind, *action_column; /*shared with normal/secondary*/
  const uint32_t *follow_target; /*actual live719448 forest node*/
  const int32_t *selected, *next_mode; /*721ed8/719b20 camera gates*/
  void *input_context;
  int (*key)(void *, unsigned code, unsigned mode, uint32_t *, char[256]);
} BkEndingTertiaryControllerScene;
/*476720 plus47811c/478eab, actual target/menu geometry, PCM, clock edits,
 * configured requests, eye selection and both camera services. Bindings
 * borrow the existing live state; this adapter creates no replacement state.
 * Read-only actions/initial_targets are bound from the actual resource table:
 * picker uses its first row, the third controller starts at its second row.
 *479bc9/479cc2 modify only clocks;402e18 is a separate real effect submission.
 * Later479137 remains a separate presentation stage. Missing services fail,
 * and later failures retain the already executed original prefix. */
int bk_ending_tertiary_controller_scene_step(
    const BkEndingTertiaryControllerScene *,
    const BkEndingTertiaryActionBindings *, const BkEndingFrameInput *,
    float seconds, char error[256]);
#endif
