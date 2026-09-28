#ifndef BK_SCENE_ENDING_AUXILIARY_H
#define BK_SCENE_ENDING_AUXILIARY_H
#include "scene/ending_audio.h"
#include "scene/ending_ui_tail.h"
#include "scene/eye_assets.h"
typedef struct {
  BkActorPose *primary;
  BkEyeAssets *eyes;
  BkEndingAudio *audio;
} BkEndingAuxiliaryServices;
/* Borrow loaded real resources. Dynamic edits and configured requests do not
 * advance local animation or publish world caches. Expression scalars are
 * retained in state until the actual ending face update consumes them. */
int bk_ending_auxiliary_apply(const BkEndingAuxiliaryServices *,
                              BkEndingAuxiliaryState *, BkEndingFrameState *,
                              int32_t proposed, int32_t voice_volume,
                              int32_t effect_volume, int32_t *result,
                              char error[256]);
/* Same real resource adapter for the UI tail. Neither clip requests nor
 * edits advance/publish the actor. Names and UI states are borrowed live. */
int bk_ending_ui_tail_apply(const BkEndingAuxiliaryServices *, BkEndingUi *,
                            BkEndingStageUi *, BkEndingUiNormalNotice *,
                            BkEndingUiAuxNotice *,
                            const BkEndingUiTailBindings *, float scale,
                            float seconds, BkEndingUiFrame *, char error[256]);
#endif
