#ifndef BK_SCENE_ENDING_SECONDARY_PRESENTATION_H
#define BK_SCENE_ENDING_SECONDARY_PRESENTATION_H
#include "game/ending_secondary_presentation.h"
#include "scene/ending_audio.h"
#include "scene/ending_secondary_assets.h"
typedef struct {
  BkEndingSecondaryAssets *assets;
  BkEndingAudio *audio;
  BkEndingSecondaryPresentationState *state; /*process-owned*/
  BkEndingVoiceEnvelope *voice; /*shared4ad5a4 filter, not6bbe40*/
  uint32_t *random;
  void *clock_context;
  int (*clock)(void *, uint32_t *, char error[256]);
} BkEndingSecondaryPresentationScene;
/* Actual47d3cb on the independent4D00FA forest. The caller must load the
 * outer background; the renderer borrows asset-owned materials and meshes.
 * No construction-time state reset or borrowed normal-ending topology.
 * The voice filter/clock are live services; no simulated cursor/timestamps.
 * Does not implement the parent47a5d0, UI, video or draw-stage publication. */
int bk_ending_secondary_presentation_scene_step(
    const BkEndingSecondaryPresentationScene *, BkEndingFrameState *,
    BkEndingAuxiliaryState *, const int32_t *automatic,
    const uint8_t toggles[8], float seconds, char error[256]);
#endif
