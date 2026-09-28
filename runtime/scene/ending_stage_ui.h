#ifndef BK_SCENE_ENDING_STAGE_UI_H
#define BK_SCENE_ENDING_STAGE_UI_H
#include "scene/ending_ui.h"
#define BK_ENDING_STAGE_UI_SPRITES 12
#define BK_ENDING_UI_TOTAL_SPRITES 75
typedef enum {
  BK_ENDING_UI_NORMAL,
  BK_ENDING_UI_SECONDARY,
  BK_ENDING_UI_AUXILIARY,
  BK_ENDING_UI_THIRD,
  BK_ENDING_UI_FOURTH,
  BK_ENDING_UI_FINAL
} BkEndingUiStageKind;
/* Extra globals63..74; cursor8 belongs to the existing base, not a copy.
 * Loaded bit0 names cursor8, bits1..12 name63..74. Names are static strings.
 * Retain this state across resource reloads: timers/motion/direction survive.
 */
typedef struct {
  BkEndingUiSprite sprites[BK_ENDING_STAGE_UI_SPRITES];
  const char *images[BK_ENDING_STAGE_UI_SPRITES + 1];
  uint16_t loaded;
} BkEndingStageUi;
/* Actual UI portions of the five stage loaders and final image constructor.
 * Only touched slots change. variant is721e04 (0/1), used by AUXILIARY.
 * Pure layout: resource acquisition/retirement remains the owner's job.
 * No game/camera/phase initialization is implied. Atomic on invalid input. */
int bk_ending_stage_ui_initialize(BkEndingUi *, BkEndingStageUi *,
                                  BkEndingUiStageKind, unsigned group,
                                  int32_t variant, unsigned width,
                                  char error[256]);
/* Mirrors their50e7c1 UI release subsets, retaining all scalar state.
 * Does not destroy GPU data; old snapshots must keep their render owner. */
int bk_ending_stage_ui_release(BkEndingUi *, BkEndingStageUi *,
                               BkEndingUiStageKind, char error[256]);
const char *bk_ending_stage_ui_image(const BkEndingStageUi *, unsigned slot);
int bk_ending_stage_ui_sprite_step(BkEndingUi *, BkEndingStageUi *,
                                   unsigned slot, float seconds,
                                   BkEndingUiDraw *, char error[256]);
/* One primary50e6ba call across all75 globals. Appends only if an image
 * exists, but also advances unconstructed/released states. Does NOT request
 * visibility: the caller preserves native request/advance ordering.
 * Capacity is checked before advancing a drawn element. Unknown slot or
 * inconsistent loaded/name metadata fails; missing actual assets must still
 * fail at resource creation, never be disguised as an unconstructed slot. */
int bk_ending_ui_dispatch_sprite(BkEndingUi *, BkEndingStageUi *, unsigned slot,
                                 float seconds, BkEndingUiFrame *,
                                 char error[256]);
#endif
