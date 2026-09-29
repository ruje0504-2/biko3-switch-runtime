#ifndef BK_GAME_ENDING_SECONDARY_PRESENTATION_H
#define BK_GAME_ENDING_SECONDARY_PRESENTATION_H
#include "game/ending_presentation.h"
/* Process state; rate is shared with47a5d0. Initialize once, not on resource
 * creation. The speech-envelope filter is separate from mouth_level. */
typedef struct {
  float rate; /*54cd04*/
  int32_t remaining, active_clip, slow_phase; /*54e290/94/98*/
  float mouth_level; /*6bbe40*/
  uint8_t mouth_descending; /*6bbe3c*/
} BkEndingSecondaryPresentationState;
BkEndingSecondaryPresentationState bk_ending_secondary_presentation_initial(void);
typedef struct {
  BkEndingFrameState *frame;
  BkEndingAuxiliaryState *auxiliary;
  const int32_t *automatic; /*6afd40*/
  const uint8_t *toggles; /*7220f8..ff*/
  const uint32_t *primary_root, *background_root;
  const uint32_t *hidden_nodes; /*709ef8[3], zero means absent*/
  uint32_t *random;
} BkEndingSecondaryPresentationBindings;
typedef struct { float end, source, rate; } BkEndingSecondaryTiming;
typedef struct {
  void *context;
  int (*clock)(void *, uint32_t *, char[256]);
  int (*advance)(void *, BkEndingPresentationActor, float, char[256]);
  int (*find)(void *, uint32_t, const char *, uint32_t *, char[256]);
  int (*hide)(void *, uint32_t, uint32_t, char[256]);
  int (*material)(void *, const char *, uint32_t, float, char[256]);
  int (*publish)(void *, char[256]);
  int (*face)(void *, BkEndingPresentationFace, float, int32_t, uint32_t, char[256]);
  int (*level)(void *, float *, char[256]);
  /* Pure reads of the actual current active slot. */
  int (*active)(void *, int32_t *, char[256]);
  int (*timing)(void *, unsigned, BkEndingSecondaryTiming *, char[256]);
} BkEndingSecondaryPresentationOps;
/* Entire47d3cb: full-speed background before hiding; variable primary rate;
 * four material edits, three optional hidden nodes, one publication, then
 * range/expression/blink/mouth. No gaze/manual/BOM or movie advance occurs.
 * Active clip is read again AFTER animation and face callbacks. Counters
 * count calls near the end, not loop events. Failures retain ordered prefix. */
int bk_ending_secondary_presentation_step(
    BkEndingSecondaryPresentationState *,
    const BkEndingSecondaryPresentationBindings *, float seconds,
    const BkEndingSecondaryPresentationOps *, char error[256]);
#endif
