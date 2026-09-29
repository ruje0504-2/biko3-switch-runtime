#ifndef BK_GAME_ENDING_SELECTED_AUXILIARY_H
#define BK_GAME_ENDING_SELECTED_AUXILIARY_H
#include "game/ending_auxiliary.h"

/*Independent process fields. The adjacent4965b9 shares scale55469c but has
 *its own byte55696d. Retain this separate55696e countdown across reloads.*/
typedef struct {
  int32_t slow, sampled; /*6ea38c/6ea390*/
  int8_t speech_blocked; /*6ea394*/
  float speech_elapsed; /*6ea398*/
  int8_t countdown; /*55696e, initial8*/
} BkEndingSelectedCycle;
BkEndingSelectedCycle bk_ending_selected_cycle_initial(void);
typedef struct {
  BkEndingFrameState *frame;
  BkEndingAuxiliaryState *auxiliary;
  const uint8_t *event; /*live control.variant721b3d, not action variant721e04*/
  BkEndingSelectedCycle *cycle;
  float *scale; /*existing BkEndingAuxiliaryCycle.scale55469c*/
  int32_t *previous_sound; /*live retained5546a0; initial-1 maps speech1*/
  const int32_t *voice_volume, *effect_volume;
} BkEndingSelectedCycleBindings;
typedef struct {
  void *context;
  int (*active)(void *, int32_t *, char[256]);
  int (*prediction)(void *, unsigned, BkEndingAuxiliaryPrediction *, char[256]);
  int (*audio)(void *, const BkEndingAudioCall *, int *, char[256]);
  int (*random)(void *, int32_t *, char[256]);
} BkEndingSelectedCycleOps;
/*Entire4968cb including49717c. Signed byte wrapping, exact look-ahead,
 *two independently cached clips, six-second speech gate, effect pause/play
 *and voice selection. This function never advances the actor itself.
 *Native undefined cue reads fail at their first actual use, preserving the
 *preceding sound/random prefix. Out-of-owner audio/table reads also fail.
 *Callbacks may change live aliases; subsequent native reads see the change.*/
int bk_ending_selected_cycle_step(const BkEndingSelectedCycleBindings *,
                                   float seconds, const BkEndingSelectedCycleOps *,
                                   char error[256]);
/*The two original5546ac rows. Bounds checking is a portable API contract.*/
int bk_ending_selected_cue(unsigned variant, unsigned index, int32_t *out);

typedef struct {
  void *context;
  int (*active)(void *, int32_t *, char[256]);
  int (*write)(void *, unsigned, BkEndingClipWrite, int32_t, char[256]);
  int (*request_ten)(void *, unsigned, char[256]); /*401b0a, not4018c8*/
  int (*audio)(void *, const BkEndingAudioCall *, int *, char[256]);
} BkEndingSelectedManualOps;
/*Entire4974d1: gate/active rejection, exact eight chain cells, ten-tick
 *request, three conditional effect stops and index. accepted is nativeEAX,
 *separate from portable success. Does not change mode/gate or expressions.*/
int bk_ending_selected_manual(BkEndingAuxiliaryState *, int32_t proposed,
                                const BkEndingSelectedManualOps *,
                                int32_t *accepted, char error[256]);
#endif
