#ifndef BK_SCENE_ENDING_SELECTED_PRESENTATION_H
#define BK_SCENE_ENDING_SELECTED_PRESENTATION_H
#include "game/ending_selected_control.h"
#include "game/ending_selected_presentation.h"
#include "scene/ending_audio.h"
#include "scene/ending_selected_assets.h"
#include "scene/ending_state.h"
typedef struct {
  BkEndingSelectedAssets *assets;
  BkEndingAudio *audio;
  BkEndingVoiceEnvelope *voice;
  BkEndingSelectedControlState *controller;
  BkEndingSelectedCycle *cycle;
  BkEndingAuxiliaryCycle *shared_cycle; /*same55469c, not another scale*/
  uint32_t *random;
  const int32_t *plain_scheduled, *voice_volume, *effect_volume;
  const uint32_t *secondary_node; /*live719b48, never719b44; zero means absent*/
  void *clock_context;
  int (*clock)(void *, uint32_t *, char error[256]);
} BkEndingSelectedPresentationScene;
/*494015 with actual4D1025 ANIM/MATA/MORP,4968CB, consumed speech and face
 * bindings. Background remains independent, primary/tracks/bg are0/1,2/3.
 * No camera-track advance, input emulation, synthetic level or actor reset.
 * Node IDs supplied by the loader must belong to this forest; a previous
 * stage's719b44 must not be substituted for the distinct719b48 reference.
 * Process state is borrowed from its existing owners. A later service failure
 * retains the executed prefix. Loading/UI/GPU/frame dispatch are separate. */
int bk_ending_selected_presentation_scene_step(
    const BkEndingSelectedPresentationScene *, BkEndingState *,
    float seconds, char error[256]);
#endif
