#ifndef BK_SCENE_RETRY_H
#define BK_SCENE_RETRY_H
#include "scene/pause.h"
typedef struct {
  BkPauseSprite sprites[6]; /*734058..734774 shared menu resource slots*/
  int32_t selected; /*BFBBB0, keyboard warp index; mouse hit is independent*/
  uint8_t phase, loaded; /*BEEB7D, resource residency*/
} BkRetryState;
const char *bk_retry_image(unsigned slot);
/*4eb9d4: only six sprites and pointer warp. Retain selection/phase, common
 * curtain/action, cursor timer and hover sound latch. */
int bk_retry_initialize(BkRetryState *, unsigned width, const BkPauseOps *,
                        char error[256]);
/*51bbe9. Uses CONFIRM/LEFT/RIGHT from BkPauseInput. No screenshot/remove.
 * Draw slots0..5 and shared20/21 cursor/curtain, captured before release68.
 * Restart target2 mode1; return target1 mode0. Original ordered side effects
 * remain on callback failure; malformed basic input fails before mutation. */
int bk_retry_step(BkRetryState *, const BkPauseBindings *, const BkPauseInput *,
                  const BkPauseOps *, BkPauseFrame *, char error[256]);
#endif
