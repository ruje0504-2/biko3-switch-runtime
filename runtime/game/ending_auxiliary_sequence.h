#ifndef BK_GAME_ENDING_AUXILIARY_SEQUENCE_H
#define BK_GAME_ENDING_AUXILIARY_SEQUENCE_H
#include "game/ending_auxiliary.h"
#include "game/ending_control.h"
#include "game/ending_record.h"

/* 47DC79 states5/6/7. These pointers borrow process state, not loader
 * snapshots. The implementation keeps the native callback order. Native
 * 722d54 is the eighth loaded ending effect (se207.wav); the scene adapter
 * keeps its direct-play callback boundary while resolving it through the
 * shared effect bank. */
typedef struct {
  BkEndingFrameState *frame;
  BkEndingControlState *control;
  BkEndingAuxiliaryState *auxiliary;
  BkMenuCamera *camera;
  BkEndingRecords *records;
  uint8_t *substate, *latches, *saved_toggle;
  int32_t *voice_latches; /*6c7f44/48*/
  float *timer;
  int32_t *pass;
  float *saved_orbit; /*6c7f34, four floats*/
  uint32_t *saved_target; /*6bbe50, three raw float words*/
  int32_t *expression_override, *face_mode; /*6bbe48/721dfc*/
  const int8_t *previous_flow;
  const int32_t *voice_volume, *effect_volume;
  float seconds;
} BkEndingAuxiliarySequenceBindings;

typedef struct {
  void *context;
  int (*active)(void *, int32_t *, char[256]);
  int (*timing)(void *, unsigned, BkEndingClipTiming *, char[256]);
  int (*request)(void *, unsigned, char[256]); /*4018c8: duplicate is a no-op*/
  int (*restart)(void *, unsigned, char[256]); /*401f71: force configured restart*/
  int (*present)(void *, unsigned, int *, char[256]);
  int (*audio)(void *, const BkEndingAudioCall *, int *, char[256]);
  int (*voice)(void *, int32_t, unsigned, int32_t, int32_t, char[256]);
  int (*expression)(void *, int32_t, int32_t, unsigned, char[256]);
  int (*target)(void *, float[3], char[256]); /*old721f08+f0*/
  int (*special_audio)(void *, int32_t, char[256]); /*722d54*/
} BkEndingAuxiliarySequenceOps;

/* Advance one native 47DC79 state5/6/7 tick. This owns no actor clock or
 * renderer publication; animation and audio remain callback services. */
int bk_ending_auxiliary_sequence_step(const BkEndingAuxiliarySequenceBindings *,
    const BkEndingAuxiliarySequenceOps *, char error[256]);
#endif
