#ifndef BK_SCENE_ENDING_OPENING_H
#define BK_SCENE_ENDING_OPENING_H
#include "game/ending_opening.h"
#include "scene/ending_audio.h"
#include "scene/ending_normal_assets.h"
typedef struct {
  BkEndingNormalAssets *assets;
  BkEndingAudio *audio;
  BkEndingCameraTransitions *transitions;
  const BkEndingCameraPresets *presets;
  const int32_t *selected; /*721ed8, existing loader/control owner*/
  BkEndingCameraOpeningInput input;
} BkEndingOpeningScene;
/* Actual state4 parent services. Only4e0b7e advances the secondary track;
 *4e0ecb holds both tracks. Bind719448 to the real A_kuch lookup (node1),
 *721f08 to old published node5. Never refresh the actor forest before either
 * query or substitute elapsed-time guesses for consumed speech status.
 * Caller retains575644 and transition state, then runs4df6c0/draw in order.
 * This adapter does not implement the other4db608 parent states or loaders. */
int bk_ending_opening_scene_step(const BkEndingOpeningScene *,
                                  const BkEndingOpeningBindings *,
                                  float seconds, char error[256]);
#endif
