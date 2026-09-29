#ifndef BK_GAME_ENDING_AUXILIARY_STATE3_H
#define BK_GAME_ENDING_AUXILIARY_STATE3_H

#include "game/ending_auxiliary.h"
#include "game/ending_control.h"

/* 47DC79 state3 is the parent around the still-unrecovered 481EA5 child.
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

#endif
