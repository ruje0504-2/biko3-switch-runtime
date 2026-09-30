#ifndef BK_GAME_ENDING_GALLERY_EFFECT_H
#define BK_GAME_ENDING_GALLERY_EFFECT_H
#include "game/ending_auxiliary.h"

typedef struct {
  int8_t alternating;       /*6dde70*/
  int32_t sampled;          /*6dde78*/
  int8_t speech_blocked;    /*6dde7c*/
  float speech_elapsed;    /*6dde80*/
} BkEndingGalleryEffectState;
typedef struct {
  const BkEndingFrameState *frame;
  const BkEndingAuxiliaryState *auxiliary;
  const uint8_t *event; /*721b3d: control.variant, not the gameplay area*/
  BkEndingGalleryEffectState *state;
  const int32_t *voice_volume, *effect_volume;
} BkEndingGalleryEffectBindings;
typedef struct {
  void *context;
  /*These queries are read-only; no selection, advancement or publication.*/
  int (*active)(void *, int32_t *, char[256]);
  int (*timing)(void *, unsigned, BkEndingClipTiming *, char[256]);
  int (*audio)(void *, const BkEndingAudioCall *, int *playing, char[256]);
  int (*random)(void *, int32_t *, char[256]);
} BkEndingGalleryEffectOps;
/*PE initial values, applied once per process, not on resource reload.*/
BkEndingGalleryEffectState bk_ending_gallery_effect_initial(void);
/*48cc18 including49717c. Audio slot4 aliases shared effect2. Preserve
 *live aliases across callbacks and the original prefix on service failure.
 *A native uninitialized cue is rejected only at its first actual read.*/
int bk_ending_gallery_effect_step(const BkEndingGalleryEffectBindings *,
    float seconds, const BkEndingGalleryEffectOps *, char error[256]);
#endif
