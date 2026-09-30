#ifndef BK_GAME_ENDING_GALLERY_NORMAL_H
#define BK_GAME_ENDING_GALLERY_NORMAL_H
#include "game/ending_opening.h"
#include "game/ending_gallery_voice.h"
typedef struct {
  float fov;       /*5545e4, independent of48302b's5545e0*/
  uint8_t delta, clip; /*6c7f7d/e; byte wrapping, signed clip reads*/
} BkEndingGalleryNormalState;
BkEndingGalleryNormalState bk_ending_gallery_normal_initial(void);
typedef struct {
  BkEndingFrameState *frame;
  BkEndingControlState *control;
  BkEndingAuxiliaryState *auxiliary;
  uint8_t *substate, *opening; /*6c7f70/6dde58*/
  int32_t *cursor, *workspace;
  uint32_t workspace_capacity;
  int32_t *elapsed_bits; /*same6d1bcc cleared by parent; FLOAT in483af0*/
  char (*speech_names)[32];
  const int32_t *voice_volume, *effect_volume;
} BkEndingGalleryNormalBindings;
typedef struct {
  void *context;
  int (*random)(void *, int32_t *, char[256]);
  int (*load)(void *, unsigned, const char *, char[256]);
  int (*play)(void *, unsigned, int32_t flags, int32_t volume, char[256]);
  int (*expression)(void *, int32_t, int32_t, int32_t, char[256]);
  int (*fov)(void *, float, char[256]);
  int (*camera)(void *, BkEndingOpeningCamera, int32_t,
                const uint32_t[3], uint32_t, uint32_t *, char[256]);
  int (*target)(void *, uint32_t[3], char[256]); /*read-only old721f08*/
  int (*hidden)(void *, unsigned actor, int hidden, char[256]);
  int (*request)(void *, unsigned actor, int32_t clip, char[256]); /*4018c8*/
  int (*timing)(void *, unsigned clip, BkEndingClipTiming *, char[256]); /*read-only primary*/
  int (*present)(void *, unsigned slot, int *, char[256]); /*read-only*/
  int (*status)(void *, unsigned slot, int *, char[256]);
  int (*pause)(void *, unsigned slot, char[256]); /*4ad34a, even absent*/
  int (*stop)(void *, unsigned slot, char[256]); /*COM Stop, only when playing*/
} BkEndingGalleryNormalOps;
/*Whole483af0, including48c8c2. Actor0/1 are primary/secondary; slot5 is
 *shared effect3. Retain the random/unhide prefix for unknown substates.
 *Substates2/5 test equality OR unordered, not source>=end. Callbacks may
 *change shared aliases; subsequent reads observe those writes. Later
 *failure preserves the native prefix. Does not advance/publish actors,
 *load a scene or implement the other four gallery child controllers.*/
int bk_ending_gallery_normal_step(BkEndingGalleryNormalState *,
    const BkEndingGalleryNormalBindings *, float seconds,
    const BkEndingGalleryNormalOps *, char error[256]);
#endif
