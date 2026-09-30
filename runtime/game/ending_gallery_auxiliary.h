#ifndef BK_GAME_ENDING_GALLERY_AUXILIARY_H
#define BK_GAME_ENDING_GALLERY_AUXILIARY_H
#include "game/ending_gallery_secondary.h"
/*488674's only additional process owners.48D7F2 does not reset them.*/
typedef struct { float fov; uint8_t view; } BkEndingGalleryAuxiliaryState; /*5545fc,6ddce1*/
BkEndingGalleryAuxiliaryState bk_ending_gallery_auxiliary_initial(void);
typedef struct {
  BkEndingFrameState *frame;
  BkEndingControlState *control;
  BkEndingAuxiliaryState *auxiliary;
  BkMenuCamera *camera;
  BkEndingCameraPresets *presets;
  BkEndingGalleryCameraState *saved;
  uint8_t *substate, *opening; /*6d1c0d,6dde58*/
  int32_t *cursor, *counters, *elapsed_bits; /*6c7f74,6dde24[10],6d1bcc FLOAT*/
  int32_t *expression_override, *face_mode;
  const int32_t *effect_volume;
} BkEndingGalleryAuxiliaryBindings;
typedef struct {
  void *context;
  int (*present)(void *, unsigned, int *, char[256]); /*read-only*/
  int (*status)(void *, unsigned, int *, char[256]);
  int (*voice)(void *, unsigned cue, unsigned slot, int32_t flags, char[256]); /*481e0a*/
  int (*play)(void *, unsigned slot, int32_t flags, int32_t volume, char[256]);
  int (*stop)(void *, unsigned slot, char[256]); /*4ad34a, not direct COMStop*/
  int (*expression)(void *, int32_t, int32_t, int32_t, char[256]);
  int (*fov)(void *, float, char[256]);
  int (*camera)(void *, BkEndingOpeningCamera, int32_t,
                const uint32_t[3], uint32_t, uint32_t *, char[256]);
  /*Queries and descriptor stores do not advance/publish or run callbacks.*/
  int (*target)(void *, unsigned node, uint32_t[3], char[256]);
  int (*active)(void *, int32_t *, char[256]);
  int (*timing)(void *, int32_t, BkEndingClipTiming *, char[256]);
  int (*source)(void *, int32_t, float, char[256]);
  int (*request)(void *, int32_t, char[256]); /*4018c8*/
  int (*restart)(void *, int32_t, char[256]); /*401f71*/
} BkEndingGalleryAuxiliaryOps;
/*Complete488674, six states and all native no-op defaults. Keeps repeated
 *speech/effect calls, saved orbit/focus and three passes in native order.
 *Does not advance/publish assets or substitute the unfinished phase8 scene.
 *A missing service or invalid table access fails at use with its prefix.*/
int bk_ending_gallery_auxiliary_step(BkEndingGalleryAuxiliaryState *,
    const BkEndingGalleryAuxiliaryBindings *, float seconds,
    const BkEndingGalleryAuxiliaryOps *, char error[256]);
#endif
