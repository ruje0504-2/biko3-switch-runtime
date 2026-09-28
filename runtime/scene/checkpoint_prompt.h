#ifndef BK_SCENE_CHECKPOINT_PROMPT_H
#define BK_SCENE_CHECKPOINT_PROMPT_H
#include "scene/pause.h"
typedef struct {
  BkPauseSprite sprites[5];
  int32_t selected; /*BFBBBC keyboard warp index*/
  uint8_t loaded;
} BkCheckpointPrompt;
const char *bk_checkpoint_prompt_image(unsigned slot);
/*4e9210 sprite/warp portion only. Owner must first perform the original
 * dialogue/item/prop/background/track releases in order. Actors are retained.
 */
int bk_checkpoint_prompt_initialize(BkCheckpointPrompt *, unsigned width,
                                    const BkPauseOps *, char error[256]);
/*51a7a2. Original release20 advances area and rebuilds its resource set BEFORE
 * schedule returns. Required release service must implement that actual work.
 * Yes -> save menu28/mode0; no -> already-loaded game2/mode3. This component
 * neither writes a save nor claims a scene has been loaded. Restores709c64
 * HUD reserve through the caller's live field after the transition service. */
int bk_checkpoint_prompt_step(BkCheckpointPrompt *, const BkPauseBindings *,
                              uint8_t *camera_phase, float *hud_reserve,
                              const BkPauseInput *, const BkPauseOps *,
                              BkPauseFrame *, char error[256]);
#endif
