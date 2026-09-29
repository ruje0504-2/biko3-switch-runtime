#ifndef BK_SCENE_ENDING_NORMAL_CONTROLLER_H
#define BK_SCENE_ENDING_NORMAL_CONTROLLER_H
#include "game/ending_normal_action.h"
#include "scene/ending_opening.h"
#include "scene/ending_state.h"
/* Additional process globals, not scene resources. Init once at process
 * boot, retain across resource reloads. The existing BkEndingState owns the
 * shared719bxx/UI aliases; neither this structure nor a child duplicates it. */
typedef struct {
  BkEndingNormalControlState control;
  BkEndingNormalActionState action;
  int32_t manual_clip;
  BkEndingOpeningRetained opening;
  BkBomReturn recoil[2]; /*single49b28f and multiple49b900 are independent*/
  int32_t flip; /*714fa8, process-initialized to0*/
} BkEndingNormalControllerRetained;
void bk_ending_normal_controller_initialize(BkEndingNormalControllerRetained *);
typedef struct {
  BkEndingNormalAssets *assets;
  BkEndingAudio *audio;
  BkEndingState *state;
  BkEndingNormalControllerRetained *retained;
  BkEndingRecords *records;
  BkMenuCamera *camera;
  BkEndingCameraTransitions *transitions;
  const BkEndingCameraPresets *presets;
  BkEndingUiSprite *ring;
  const BkEndingUiPickBindings *geometry;
  const int8_t *previous_flow;
  const int32_t *voice_volume, *effect_volume, *flip;
  const float *scene_world; /*global645600 cached world;714fac is the BOM table*/
  uint32_t *random;
  void *input_context;
  int (*key)(void *, unsigned code, unsigned mode, uint32_t *, char[256]);
  int (*raw_key)(void *, unsigned code, uint32_t *, char[256]);
  int (*warp)(void *, float x, float y, char[256]);
} BkEndingNormalControllerScene;
/* Complete4db608 + real4dd280/4e252b/opening children, target selection,
 * forest-node identities, configured/instant clip edits and six-buffer PCM.
 * Geometry must be an old-world snapshot prepared at the native call stage;
 * no sampling/publication occurs except the original opening track update.
 * Face/4df6c0, common camera and draw remain subsequent separate stages.
 * Required services/resources fail explicitly. Later failure keeps prefix.
 * No loader initialization, process clear, fake speech duration or transition
 * completion is inferred here. State3's native result is internal to parent.
 */
int bk_ending_normal_controller_scene_step(const BkEndingNormalControllerScene *,
                                           const BkEndingFrameInput *,
                                           float seconds, char error[256]);
#endif
