#ifndef BK_GAME_ENDING_AUXILIARY_STATE3_H
#define BK_GAME_ENDING_AUXILIARY_STATE3_H

#include "game/ending_auxiliary.h"
#include "game/ending_control.h"
#include "model/clip.h"

/* 47DC79 state3 is the parent around the recoverable 481EA5 child prefix.
 * Keep the child as a required service: the native parent invokes it before
 * reading input, so a scene cannot silently turn this branch into a UI-only
 * demo. */
typedef struct {
  BkEndingFrameState *frame;
  BkEndingControlState *control;
  BkEndingAuxiliaryState *auxiliary;
  const int32_t *point_7220c8; /* first live projected center, X/Y */
  const float *menu_width_7389e8; /* slot51 width; native divisor is 2 */
} BkEndingAuxiliaryState3Bindings;

typedef struct {
  void *context;
  int (*child)(void *, const BkEndingFrameInput *, char[256]); /*481EA5*/
  int (*key)(void *, unsigned code, unsigned mode, uint32_t *, char[256]);
  int (*hit)(void *, const float center[2], float radius,
             const float pointer[2], int *hit, char[256]); /*4A777E*/
  int (*expression)(void *, int32_t a, int32_t b, unsigned eye, char[256]);
  int (*request)(void *, unsigned slot, char[256]); /*4018C8*/
  int (*voice)(void *, int32_t cue, unsigned slot, int32_t flags,
               int32_t volume, char[256]); /*481E0A*/
  int32_t voice_volume;
} BkEndingAuxiliaryState3Ops;

/* Execute the recovered 47DC79 state3 parent prefix. The child is called
 * first and owns the actor/clip/effect work at 481EA5. State3 itself does not
 * synthesize that work and returns failure while the child is unavailable. */
int bk_ending_auxiliary_state3_step(
    const BkEndingAuxiliaryState3Bindings *, const BkEndingFrameInput *,
    const BkEndingAuxiliaryState3Ops *, char error[256]);

/* Recover the first 481EA5 branch. It handles the same strict circle hit as
 * the parent, requests clip4 and the group-specific expression, waits for
 * speech owner0, then applies the cue5/cue6 prefix. A miss with an active
 * clip other than4 applies the recovered group2/3/4 effect thresholds; active
 * clip4 also has its distance-based source interpolation and failure-reset
 * prefix. The following successful interpolation media/state transition
 * remains an explicit boundary. */
typedef struct {
  BkEndingFrameState *frame;
  BkEndingControlState *control;
  BkEndingAuxiliaryState *auxiliary;
  const int32_t *point_7220c8;
  const int32_t (*targets_721f90)[2];
  const float *menu_width_7389e8;
  const int32_t *offset_mode_54e2f8;
  int32_t *voice_latch_6c7f44;
  uint8_t *effect_latches_6c7f60; /* native ten-byte latch block */
} BkEndingAuxiliaryChildBindings;

typedef struct {
  void *context;
  int (*active)(void *, int32_t *, char[256]);
  int (*source)(void *, unsigned slot, float source, char[256]);
  int (*rewind)(void *, unsigned slot, char[256]);
  int (*present)(void *, unsigned owner, int *, char[256]);
  int (*status)(void *, unsigned owner, int *, char[256]);
  int (*hit)(void *, const float center[2], float radius,
             const float pointer[2], int *hit, char[256]);
  int (*request)(void *, unsigned slot, char[256]);
  int (*expression)(void *, int32_t a, int32_t b, unsigned eye, char[256]);
  int (*voice)(void *, int32_t cue, unsigned slot, int32_t flags,
               int32_t volume, char[256]);
  int (*random)(void *, int32_t *, char[256]);
  int (*timing)(void *, unsigned slot, BkClipTiming *, char[256]);
  int (*effect_present)(void *, unsigned effect, int *, char[256]);
  int (*effect_status)(void *, unsigned effect, int *, char[256]);
  int (*effect)(void *, unsigned effect, unsigned flags, int32_t volume,
                char[256]);
  int (*effect_stop)(void *, unsigned effect, char[256]);
  int32_t voice_volume, effect_volume;
} BkEndingAuxiliaryChildOps;

int bk_ending_auxiliary_state3_child_step(
    const BkEndingAuxiliaryChildBindings *, const BkEndingFrameInput *,
    const BkEndingAuxiliaryChildOps *, char error[256]);

#endif
