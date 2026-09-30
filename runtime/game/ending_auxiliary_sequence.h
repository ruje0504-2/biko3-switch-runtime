#ifndef BK_GAME_ENDING_AUXILIARY_SEQUENCE_H
#define BK_GAME_ENDING_AUXILIARY_SEQUENCE_H
#include "game/ending_auxiliary.h"
#include "game/ending_control.h"
#include "game/ending_record.h"
#include "model/clip.h"

/* WIP checkpoint: not in CMake or scene dispatch. Native threshold and
 * callback/state ordering verification is still pending; do not treat this
 * draft as a completed controller. State5 is not implemented here.
 * 47DC79 state6/7. These pointers borrow process state, not loader snapshots.
 * The saved orbit/target survive the three state7 camera passes. */
typedef struct {
  BkEndingFrameState *frame;
  BkEndingControlState *control;
  BkEndingAuxiliaryState *auxiliary;
  BkMenuCamera *camera;
  BkEndingRecords *records;
  uint8_t *substate, *latches, *saved_toggle;
  int32_t *voice_latches; /*6c7f44/48: also the presentation expression latch*/
  float *timer;
  int32_t *pass;
  float *saved_orbit; /*6c7f34, four floats*/
  uint32_t *saved_target; /*6bbe50, three raw float words*/
  int32_t *expression_override, *face_mode; /*6bbe48/721dfc*/
  const int8_t *previous_flow;
  const int32_t *voice_volume, *effect_volume;
} BkEndingAuxiliarySequenceBindings;

typedef struct {
  void *context;
  int (*active)(void *, int32_t *, char[256]);
  int (*timing)(void *, unsigned, BkClipTiming *, char[256]);
  int (*request)(void *, unsigned, char[256]); /*4018c8: duplicate is a no-op*/
  int (*restart)(void *, unsigned, char[256]); /*401f71: force configured restart*/
  int (*present)(void *, unsigned, int *, char[256]);
  int (*audio)(void *, const BkEndingAudioCall *, int *, char[256]);
  int (*voice)(void *, int32_t, unsigned, int32_t, int32_t, char[256]);
  int (*expression)(void *, int32_t, int32_t, unsigned, char[256]);
  int (*target)(void *, float[3], char[256]); /*old721f08+f0*/
} BkEndingAuxiliarySequenceOps;

/* Draft state6/7 entry. Intended to retain trailing effects after a state or
 * curtain change without advancing/publishing the actor. Behavior and failure
 * prefixes still require fixed-EXE replay before production integration. */
int bk_ending_auxiliary_sequence_step(const BkEndingAuxiliarySequenceBindings *,
    const BkEndingAuxiliarySequenceOps *, char error[256]);
#endif
