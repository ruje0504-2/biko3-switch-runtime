#ifndef BK_GAME_ENDING_GALLERY_SELECTED_H
#define BK_GAME_ENDING_GALLERY_SELECTED_H
#include "game/ending_gallery_secondary.h"
/* Process fields outside48D7F2's reset. Saved orbit/toggle and expression
 * override belong to the same owners used by the other gallery children. */
typedef struct {
  float fov;                         /*5545f4*/
  int32_t base, counter, reset, alternate; /*6d1bd0,6dde5c/60/64*/
  int32_t view;                      /*6d1be4*/
  uint8_t countdown;                 /*5545f8, initial5*/
  int8_t view_kind;                  /*6d1be2*/
} BkEndingGallerySelectedState;
BkEndingGallerySelectedState bk_ending_gallery_selected_initial(void);
typedef struct {
  BkEndingFrameState *frame;
  BkEndingControlState *control;
  BkEndingAuxiliaryState *auxiliary;
  BkMenuCamera *camera;
  BkEndingCameraPresets *presets;
  BkEndingGalleryCameraState *saved;
  int8_t *substate;                  /*6d1c0c*/
  uint8_t *opening;                  /*6dde58*/
  int32_t *cursor, *workspace;
  uint32_t workspace_capacity;
  int32_t *counters, *camera_words;   /*6dde24[10],6ddce4[75]*/
  int32_t *previous_clock, *current_clock, *elapsed;
  int32_t *open, *expression_override, *face_mode;
  uint8_t *fade_stage, *flash_wanted; /*actual UI slot52+134/+167*/
  uint8_t *action, *curtain_wanted;   /*actual shared common owner*/
  char *speech_name;                 /*722224, >=32 bytes*/
  const int32_t *voice_volume, *effect_volume;
  /*Opt-in port policy: short/disabled dialogue must not bypass intro clip3
   *and strand playback in clip4. Zero keeps native4855C9's exact branch.*/
  int wait_for_intro_clip;
} BkEndingGallerySelectedBindings;
typedef struct {
  float start, end, source;
  int32_t chain;
  /*Port compatibility: the real scheduler finished and chained this slot
   *before source reached end. Zero retains the exact native comparison.*/
  int completed_chain;
} BkEndingGallerySelectedClip;
typedef struct {
  void *context;
  int (*clock)(void *, uint32_t *, char[256]);
  int (*random)(void *, int32_t *, char[256]);
  int (*present)(void *, unsigned, int *, char[256]);
  int (*status)(void *, unsigned, int *, char[256]);
  int (*voice)(void *, unsigned cue, unsigned slot, int32_t flags, char[256]); /*4946b4*/
  int (*load)(void *, unsigned, const char *, char[256]);
  int (*play)(void *, unsigned, int32_t flags, int32_t volume, char[256]);
  int (*stop)(void *, unsigned, char[256]); /*direct COMStop*/
  int (*expression)(void *, int32_t, int32_t, int32_t, char[256]);
  int (*fov)(void *, float, char[256]);
  int (*camera)(void *, BkEndingOpeningCamera, int32_t,
                const uint32_t[3], uint32_t, uint32_t *, char[256]);
  /* Queries and direct descriptor writes must not advance, publish, dispatch
   * callbacks or otherwise mutate the shared control state. */
  int (*target)(void *, unsigned node, uint32_t[3], char[256]);
  int (*active)(void *, int32_t *, char[256]);
  int (*clip)(void *, int32_t, BkEndingGallerySelectedClip *, char[256]);
  int (*source)(void *, int32_t, float, char[256]);
  int (*chain)(void *, int32_t, int32_t, char[256]);
  int (*request)(void *, int32_t, char[256]); /*4018c8*/
  int (*restart)(void *, int32_t, char[256]); /*401f71*/
  int (*fade)(void *, uint8_t, char[256]); /*50e633, slot52*/
} BkEndingGallerySelectedOps;
/* Complete4855C9, including the native early returns that omit the tail
 * timeGetTime. Signed state3/10/unknown still execute both clocks. Required
 * services and table bounds fail at use, preserving the executed prefix.
 * Does not stand in for presentation, resources or unfinished phase8 entry. */
int bk_ending_gallery_selected_step(BkEndingGallerySelectedState *,
    const BkEndingGallerySelectedBindings *, float seconds,
    const BkEndingGallerySelectedOps *, char error[256]);
#endif
