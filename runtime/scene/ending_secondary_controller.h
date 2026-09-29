#ifndef BK_SCENE_ENDING_SECONDARY_CONTROLLER_H
#define BK_SCENE_ENDING_SECONDARY_CONTROLLER_H
#include "game/ending_secondary_control.h"
#include "scene/ending_audio.h"
#include "scene/ending_secondary_assets.h"
#include "scene/ending_secondary_ui.h"
#include "scene/ending_state.h"
typedef struct {
  BkEndingSecondaryAssets *assets;
  BkEndingAudio *audio;
  BkEndingState *state;
  BkEndingSecondaryControlState *retained;
  float *rate; /*same54cd04 owned by secondary presentation*/
  int32_t *action_kind, *action_column; /*existing721e10/7220f0 owners*/
  BkMenuCamera *camera;
  BkEndingCameraTransitions *transitions;
  const BkEndingCameraPresets *presets;
  BkEndingUi *ui;
  const BkEndingUiPickBindings *geometry; /*old39 published worlds*/
  uint32_t width, height; /*logical content extent, without letterbox origin*/
  const int8_t *previous_flow;
  const int32_t *voice_volume;
  uint32_t *random;
  void *input_context;
  int (*key)(void *, unsigned code, unsigned mode, uint32_t *, char[256]);
} BkEndingSecondaryControllerScene;
/* Complete47A5D0 with real47D9EE PCM,47C334 picking,47CB41/47CD36 menus,
 * configured requests, force-reselection, eyes and two original cameras.
 * The scene owns the independent4D00FA topology: primary0/tracks1,2/bg3.
 *719448 and optional719b40 are live retained forest IDs supplied by its
 * loader, never guessed target positions or alternate normal-model nodes.
 * No primary/background sampling or publication:47D3CB/draw run later.
 * Process counters, voice names, RNG and UI aliases remain caller-owned.
 * This adapter does not create resources or complete the outer lifecycle.
 * Required missing services fail; later failure retains executed prefix. */
int bk_ending_secondary_controller_scene_step(
    const BkEndingSecondaryControllerScene *, const BkEndingFrameInput *,
    float seconds, char error[256]);
#endif
