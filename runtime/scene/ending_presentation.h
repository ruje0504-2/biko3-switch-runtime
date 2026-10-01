#ifndef BK_SCENE_ENDING_PRESENTATION_H
#define BK_SCENE_ENDING_PRESENTATION_H
#include "game/ending_presentation.h"
#include "scene/ending_audio.h"
#include "scene/ending_normal_assets.h"
/* Independent process states. Zero once on boot, retain across model reloads
 * and calls; never share the three manual controllers or gameplay envelope. */
typedef struct {
  BkBomManual manual[3];
  BkEndingVoiceEnvelope voice;
} BkEndingPresentationRetained;
typedef struct {
  BkEndingNormalAssets *assets;
  BkEndingAudio *audio;
  BkMaterialPose *primary_materials;
  int32_t *bom_disabled;
  size_t bom_count;
  BkEndingPresentationRetained *retained;
  uint32_t *random;
  void *clock_context;
  int (*clock)(void *, uint32_t *, char error[256]);
  unsigned stick_motion_gain; /*0/1 keeps native drag,6 for the Switch stick*/
} BkEndingPresentationScene;
/* Bind complete4df6c0 to actual loaded normal assets. State references are
 * portable forest IDs, never x86 addresses or casts of retained words.
 * Original registry names used by this function all belong to the primary
 * normal model; encountering one in another owner fails rather than editing
 * immutable materials or silently losing the change. Missing names remain
 * native no-ops. Clocks/voice are read at their actual stage, face changes
 * retain blink-before-mouth ordering and current eye variant. No GPU draw,
 * video, parent controller, camera advance, or artificial input is included.
 * A failure retains prior effects and terminates the caller's frame. */
int bk_ending_presentation_scene_step(const BkEndingPresentationScene *,
                                       const BkEndingPresentationBindings *,
                                       const BkEndingFrameInput *, float seconds,
                                       char error[256]);
#endif
