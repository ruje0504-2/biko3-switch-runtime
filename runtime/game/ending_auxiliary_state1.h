#ifndef BK_GAME_ENDING_AUXILIARY_STATE1_H
#define BK_GAME_ENDING_AUXILIARY_STATE1_H

#include "game/ending_auxiliary.h"
#include "game/ending_control.h"

/* 47DC79 state1 owns the pointer/menu transition after state4 has finished.
 * Scene services remain callbacks so this module has no UI, audio or Vulkan
 * dependency. A callback failure retains the already executed native prefix. */
typedef struct {
  BkEndingFrameState *frame;
  BkEndingControlState *control;
  BkEndingAuxiliaryState *auxiliary;
  float *timer_6c7f6c;
  int32_t *delay_54f8e0;
} BkEndingAuxiliaryState1Bindings;

typedef struct {
  void *context;
  int (*key)(void *, unsigned code, unsigned mode, uint32_t *, char[256]);
  int (*present)(void *, unsigned owner, int *, char[256]);
  int (*status)(void *, unsigned owner, int *, char[256]);
  int (*audio)(void *, const BkEndingAudioCall *, int *, char[256]);
  int (*random)(void *, int32_t *, char[256]);
  int (*pick)(void *, const float pointer[2], int32_t preferred,
              int32_t *result, char[256]); /*47C334*/
  int (*menu)(void *, int32_t selected, int32_t *zone, char[256]); /*481C2C*/
  int (*voice)(void *, int32_t cue, unsigned slot, int32_t flags,
               int32_t volume, char[256]); /*481E0A*/
  int (*request)(void *, unsigned slot, char[256]); /*4018C8*/
  int (*expression)(void *, int32_t a, int32_t b, unsigned eye, char[256]);
} BkEndingAuxiliaryState1Ops;

int bk_ending_auxiliary_state1_step(
    const BkEndingAuxiliaryState1Bindings *, const float pointer[2],
    float seconds, int32_t voice_volume,
    const BkEndingAuxiliaryState1Ops *, char error[256]);

/* State2 waits for both low-AL idle queries before returning to state1. */
int bk_ending_auxiliary_state2_step(BkEndingControlState *,
                                    const BkEndingAuxiliaryState1Ops *,
                                    char error[256]);

#endif
