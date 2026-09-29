#ifndef BK_SCENE_ENDING_TERTIARY_PRESENTATION_H
#define BK_SCENE_ENDING_TERTIARY_PRESENTATION_H
#include "scene/ending_audio.h"
#include "scene/ending_state.h"
#include "scene/ending_tertiary_assets.h"
typedef struct {
  BkEndingTertiaryAssets *assets;
  BkEndingAudio *audio;
  BkEndingVoiceEnvelope *voice; /*existing process4ad5a4 filter*/
  uint32_t *random;
  int32_t *bom_disabled; /*actual renderer's independent binding cells*/
  size_t bom_count;
  void *clock_context;
  int (*clock)(void *, uint32_t *, char error[256]);
} BkEndingTertiaryPresentationScene;
/*479137 using the actual4d2320 resources, shared frame/face state, consumed
 * PCM, per-actor ANIM/MATA/MORP and two complete forest publications. The
 * existing721dfc/721df8 owners are borrowed separately; never reset here.
 * Background and special-node roles are resolved explicitly. Each BOM child
 * position and orientation commits separately against the current reference.
 * No GPU allocation, loader initialization or synthetic clock/audio input. */
int bk_ending_tertiary_presentation_scene_step(
    const BkEndingTertiaryPresentationScene *, BkEndingState *,
    const int32_t *face_mode, int32_t *eye_lower, float seconds,
    char error[256]);
#endif
