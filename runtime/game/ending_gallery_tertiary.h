#ifndef BK_GAME_ENDING_GALLERY_TERTIARY_H
#define BK_GAME_ENDING_GALLERY_TERTIARY_H
#include "game/ending_opening.h"
/*48758C adds no process owner. Its clip byte belongs to483AF0, while
 *reverse/expression aliases are shared with48BCBB and the selected stage.*/
typedef struct {
  BkEndingFrameState *frame;
  BkEndingControlState *control;
  BkEndingAuxiliaryState *auxiliary;
  uint8_t *substate, *clip, *transition_latch, *cycles, *expression_latch;
  /*6d1bd4,6c7f7e,6dde50/51/52*/
  int32_t *cursor, *workspace;
  uint32_t workspace_capacity;
  int32_t *counters, *elapsed_bits; /*6dde24[10],6d1bcc FLOAT alias*/
  int32_t *reverse, *face_mode, *expression_override; /*725704,721dfc,6c7f78*/
  const int32_t *voice_volume, *effect_volume;
} BkEndingGalleryTertiaryBindings;
typedef struct {
  void *context;
  int (*random)(void *, int32_t *, char[256]);
  int (*present)(void *, unsigned slot, int *, char[256]); /*read-only*/
  int (*status)(void *, unsigned slot, int *, char[256]);
  int (*voice)(void *, int32_t cue, unsigned slot, int32_t bank,
                int32_t select, char[256]); /*479739; load only*/
  int (*play)(void *, unsigned slot, int32_t volume, char[256]); /*4ad2bf flags0*/
  int (*expression)(void *, int32_t, int32_t, int32_t, char[256]);
  int (*material)(void *, const char *name, uint32_t hidden, float alpha, char[256]);
  int (*hidden)(void *, unsigned actor, int hidden, char[256]); /*actor+160 root*/
  int (*request)(void *, unsigned actor, int32_t clip, char[256]); /*4018c8*/
  int (*active)(void *, int32_t *, char[256]); /*read-only primary*/
  int (*timing)(void *, int32_t, BkEndingClipTiming *, char[256]); /*read-only*/
  int (*target)(void *, uint32_t position[3], char[256]); /*read-only old721f08*/
} BkEndingGalleryTertiaryOps;
/*Complete48758C, all four states and native no-op defaults. Does not sample
 *or publish animations; reverse is consumed by the actual presentation pass.
 *Separate voice loading/Play, live callback reads, byte/word wrapping and
 *unordered comparisons retain original order. Invalid table accesses and
 *the conditionally uninitialized effect selector fail at use with the prefix
 *preserved. No substituted actor/audio/material services or phase8 entry.*/
int bk_ending_gallery_tertiary_step(const BkEndingGalleryTertiaryBindings *,
    const BkEndingGalleryTertiaryOps *, char error[256]);
#endif
