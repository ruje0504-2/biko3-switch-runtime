#ifndef BK_SCENE_DIALOGUE_BACKDROP_H
#define BK_SCENE_DIALOGUE_BACKDROP_H
#include "ui/fade_sprite.h"
typedef struct {
  BkFadeSprite image;                /*73449c*/
  int32_t saved_expression;          /*bf00f0*/
  uint8_t curtain_cycle, image_kind; /*bf00f4/f5*/
  uint8_t image_wanted;              /*734603*/
} BkDialogueBackdrop;
typedef struct {
  uint8_t *phase;          /*bef798, shared with actor*/
  int32_t *expression;     /*734034, same actor rules*/
  BkFadeSprite *curtain;   /*729c10, NOT common beea18*/
  uint8_t *curtain_wanted; /*729d77*/
  const char *image;       /*befeec, bounded256 name*/
} BkDialogueBackdropBindings;
typedef struct {
  void *context;
  /*4f19ec actual image replacement; required, failure terminates frame.
   * State resets the newly constructed fade only after this succeeds. */
  int (*replace)(void *, const char *name, char error[256]);
} BkDialogueBackdropOps;
typedef struct {
  float image_alpha, curtain_alpha;
  int replaced;
} BkDialogueBackdropFrame;
/*4f1503: actor hide/restore and native background phase1..5/10..11.
 * Image request precedes advancement and phase checks. Curtain advances
 * before its request. Frame captures those draw-time alpha values.
 * Shared state is never implicitly initialized on entry. Invalid input
 * rejects before mutation; service failure retains the executed prefix. */
int bk_dialogue_backdrop_step(BkDialogueBackdrop *,
                              const BkDialogueBackdropBindings *,
                              const BkDialogueBackdropOps *, float seconds,
                              BkDialogueBackdropFrame *, char error[256]);
/*4ef6a0 initial image/curtain uses1280/960;4f19ec replacement uses1024/768.
 * Both heights intentionally use the width scale. Preserve float rounding. */
int bk_dialogue_backdrop_extent(float out[2], unsigned viewport_width,
                                int replacement);
#endif
