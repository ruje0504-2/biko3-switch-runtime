#ifndef BK_GAME_ENDING_GALLERY_CONTROL_H
#define BK_GAME_ENDING_GALLERY_CONTROL_H
#include "game/ending_opening.h"
#include "game/ending_record.h"

typedef struct { float fov; /*5545e0*/ } BkEndingGalleryControlState;
BkEndingGalleryControlState bk_ending_gallery_control_initial(void);
typedef struct {
  BkEndingFrameState *frame;
  BkEndingControlState *control;
  BkEndingAuxiliaryState *auxiliary;
  const BkEndingRecords *records;
  uint8_t *action, *previous; /*6ddce0/6d1be1*/
  uint8_t *opening;           /*6dde58*/
  uint8_t *normal, *secondary, *tertiary, *special; /*6c7f70/6d1be0/6d1bd4/6d1c0d*/
  uint8_t *transition_latch; /*6dde50*/
  int8_t *selected;          /*6d1c0c*/
  int32_t *cursor, *workspace; /*6c7f74/6c7f80*/
  uint32_t workspace_capacity;
  int32_t *counters;         /*ten words6dde24*/
  int32_t *opening_counter, *step_counter, *next_mode; /*6d1bcc/6dde4c/719b20*/
  char *speech_name;         /*at least32 bytes722224, no clear before strcpy*/
  const int32_t *voice_volume;
} BkEndingGalleryControlBindings;
typedef struct {
  void *context;
  int (*expression)(void *, int32_t, int32_t, int32_t, char[256]);
  int (*fov)(void *, float, char[256]); /*42cf0e before reducing retained FOV*/
  int (*camera)(void *, BkEndingOpeningCamera, int32_t choice,
                 const uint32_t offset[3], uint32_t extra, uint32_t *, char[256]);
  int (*target)(void *, uint32_t position[3], char[256]); /*read-only old721f08*/
  int (*present)(void *, unsigned, int *, char[256]); /*read-only pointer query*/
  int (*status)(void *, unsigned, int *, char[256]);
  int (*load)(void *, unsigned, const char *, char[256]);
  int (*play)(void *, unsigned, int32_t volume, char[256]);
  int (*voice_name)(void *, char name[32], char[256]); /*4e0818(0,0,0)*/
  int (*voice)(void *, int32_t cue, unsigned slot, int32_t bank, int32_t select,
                char[256]); /*479739, separate from play*/
  int (*cue)(void *, int32_t cue, int32_t bank, unsigned slot, int32_t flags,
              char[256]); /*49490a*/
  /*Main states4..8 call483af0/4843aa/4855c9/48758c/488674 respectively.
   *Missing/unimplemented children must fail, never return empty success.*/
  int (*child)(void *, unsigned main_state, char[256]);
} BkEndingGalleryControlOps;
/*Entire48302b parent and483a36 classification. The five child controllers
 *remain required services. Borrow existing process aliases; reset only at
 *native assignments, retain callback prefixes on failure, and capture the
 *record group at entry (not after a camera callback). Bounded workspace,
 *record and camera-table reads fail at their first actual access. Does not
 *load a scene, publish actors, save unlocks or manufacture recorded actions.*/
int bk_ending_gallery_control_step(BkEndingGalleryControlState *,
    const BkEndingGalleryControlBindings *, float seconds,
    const BkEndingGalleryControlOps *, char error[256]);
#endif
