#ifndef BK_GAME_ENDING_GALLERY_SECONDARY_H
#define BK_GAME_ENDING_GALLERY_SECONDARY_H
#include "game/ending_opening.h"
/*Shared by4843aa/4855c9/488674. One process owner, borrowed by each child;
 *48d7f2 and resource reload do not reset these saved camera aliases.*/
typedef struct {
  float orbit[4]; /*6dde14: yaw,pitch,radius,height*/
  uint8_t toggle; /*6c7f7c*/
  uint32_t target[3]; /*6d1bc0: shared4855c9/488674 saved focus*/
} BkEndingGalleryCameraState;
BkEndingGalleryCameraState bk_ending_gallery_camera_initial(void);
typedef struct {
  int32_t remaining, alternate; /*5545e8/ec: delay/cycles/preset share one word*/
  float fov;                   /*5545f0*/
  uint8_t cycles;              /*6dde59, signed comparison*/
} BkEndingGallerySecondaryState;
BkEndingGallerySecondaryState bk_ending_gallery_secondary_initial(void);
typedef struct {
  BkEndingFrameState *frame;
  BkEndingControlState *control;
  BkEndingAuxiliaryState *auxiliary;
  BkMenuCamera *camera;
  BkEndingGalleryCameraState *saved;
  uint8_t *substate, *opening; /*6d1be0/6dde58*/
  int32_t *cursor, *counters; /*6c7f74/6dde24[10]*/
  int32_t *previous_clock, *current_clock, *elapsed; /*6d1bd8/dc,6dde54 raw words*/
} BkEndingGallerySecondaryBindings;
typedef struct {
  float end, source;
  int32_t chain, loop; /*descriptor+200/+190, NOT ended/looped flags*/
  /*Port compatibility: actual next presentation step crosses this loop's
   *time boundary. Zero retains the native fixed source-distance test.*/
  int finish_crossing;
} BkEndingGallerySecondaryClip;
typedef struct {
  void *context;
  int (*clock)(void *, uint32_t *, char[256]);
  int (*random)(void *, int32_t *, char[256]);
  int (*present)(void *, unsigned, int *, char[256]);
  int (*status)(void *, unsigned, int *, char[256]);
  int (*voice)(void *, unsigned cue, unsigned slot, int32_t flags, char[256]); /*47d9ee*/
  int (*expression)(void *, int32_t, int32_t, int32_t, char[256]);
  int (*fov)(void *, float, char[256]);
  int (*camera)(void *, BkEndingOpeningCamera, int32_t,
                const uint32_t[3], uint32_t, uint32_t *, char[256]);
  int (*target)(void *, uint32_t[3], char[256]); /*read-only old721ef4*/
  int (*active)(void *, int32_t *, char[256]);   /*read-only*/
  int (*clip)(void *, int32_t, BkEndingGallerySecondaryClip *, char[256]); /*read-only*/
  int (*request)(void *, int32_t, char[256]); /*4018c8*/
  int (*restart)(void *, int32_t, char[256]); /*401f71*/
} BkEndingGallerySecondaryOps;
/*Complete4843aa, eight dispatch states (6 is native no-op). Uses both
 *timeGetTime call positions, wrapping integer milliseconds, actual chained
 *clip configuration and native ordered/unordered comparisons. Services
 *are mandatory at use; later failures preserve the observed prefix. Does
 *not advance/publish actors, load resources or replace other gallery children.*/
int bk_ending_gallery_secondary_step(BkEndingGallerySecondaryState *,
    const BkEndingGallerySecondaryBindings *, float seconds,
    const BkEndingGallerySecondaryOps *, char error[256]);
#endif
