#ifndef BK_SCENE_ENDING_PROCESS_H
#define BK_SCENE_ENDING_PROCESS_H
#include "game/ending_selected_action.h"
#include "game/ending_gallery_control.h"
#include "game/ending_gallery_normal.h"
#include "game/ending_gallery_secondary.h"
#include "game/ending_gallery_selected.h"
#include "game/ending_gallery_auxiliary.h"
#include "game/ending_gallery_effect.h"
/*CPU process fields outside4CC582 and48D7F2. Owned once by the application;
 *resource snapshots only borrow this aggregate. No asset/PCM/GPU handles.
 *Interactive6DDE98 and gallery6C7F78 are DIFFERENT expression overrides.
 *725704 is shared via selected_action.diagnostic_crossings; do not copy it.*/
typedef struct {
  BkEndingSelectedControlState selected_control;
  BkEndingSelectedActionState selected_action;
  BkEndingSelectedCycle selected_cycle;
  BkEndingGalleryControlState gallery_control;
  BkEndingGalleryNormalState gallery_normal;
  BkEndingGallerySecondaryState gallery_secondary;
  BkEndingGallerySelectedState gallery_selected;
  BkEndingGalleryAuxiliaryState gallery_auxiliary;
  BkEndingGalleryEffectState gallery_effect;
  BkEndingGalleryCameraState gallery_camera;
  int32_t gallery_override;             /*6C7F78, PE0*/
  uint8_t gallery_expression_latch;     /*6DDE52, PE0*/
  uint8_t gallery_mouth_falling;        /*6DDE68, PE0*/
  float gallery_mouth_level;           /*6DDE6C, PE0*/
} BkEndingProcess;
void bk_ending_process_initialize(BkEndingProcess *);
#endif
