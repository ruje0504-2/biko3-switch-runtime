#ifndef BK_GAME_ENDING_SELECTED_CONTROL_H
#define BK_GAME_ENDING_SELECTED_CONTROL_H
#include "game/ending_opening.h"
#include "game/ending_selected_auxiliary.h"

/*Once-owned fields used by48d8e9 and494015. These addresses are outside
 *the4cc582 block and the49739a leave reset; a resource reload retains them.*/
typedef struct {
  float fov; /*556968, initial1*/
  float replay_elapsed; /*6ea360*/
  int32_t replay_after, manual_mode, expression_override; /*6ddebc/c4/98*/
} BkEndingSelectedControlState;
BkEndingSelectedControlState bk_ending_selected_control_initial(void);
typedef struct {
  BkEndingFrameState *frame;
  BkEndingControlState *control;
  BkEndingAuxiliaryState *auxiliary;
  BkMenuCamera *camera;
  const BkEndingCameraPresets *presets;
  uint8_t *substate; /*retained.byte_6ea358*/
  int32_t *mode, *choice, *reset_c; /*UI6dde94/722104/6ddea8*/
  int32_t (*configuration)[6]; /*5x6,6e9fa8*/
  int32_t *camera_words; /*75 raw words6ea028*/
  int32_t (*group_prefix)[3]; /*5x3, first3words of each original64-byte row*/
  int32_t *voice_latches; /*20 words6ea2c0, first of each4-word row*/
  int32_t *inputs, *processed; /*2words each,6ea170/6ea178*/
  const int32_t *selected, *open;
  float *gauge_y; /*721e24*/
  const float *scale; /*721ad0, output scale, not animation55469c*/
  const int32_t *plain_scheduled; /*b53c38, settings705290*/
  const int8_t *previous_flow;
  const int32_t (*targets)[2]; /*39 projected node positions721f90*/
  const int32_t *alternate; /*2words6afd38*/
  const int32_t *voice_volume, *effect_volume;
  char *speech_name; /*at least32bytes, existing722224*/
} BkEndingSelectedControlBindings;
typedef struct {
  void *context;
  int (*key)(void *, unsigned code, unsigned mode, uint32_t *, char[256]);
  int (*raw_key)(void *, unsigned code, uint32_t *, char[256]);
  int (*audio)(void *, const BkEndingAudioCall *, int *, char[256]);
  int (*load)(void *, unsigned speech, const char *, char[256]);
  int (*expression)(void *, int32_t a, int32_t b, int32_t mode, char[256]);
  int (*active)(void *, int32_t *, char[256]);
  int (*request)(void *, unsigned slot, char[256]); /*4018c8*/
  int (*write)(void *, unsigned slot, BkEndingClipWrite, int32_t, char[256]);
  int (*target)(void *, unsigned node, float position[3], char[256]);
  int (*camera)(void *, BkEndingOpeningCamera, int32_t choice,
                 const uint32_t offset[3], uint32_t extra, uint32_t *, char[256]);
  int (*manual)(void *, int32_t proposed, int32_t *accepted, char[256]); /*4974d1*/
  int (*pick)(void *, const float pointer[2], int32_t *, char[256]); /*47c334(...,4)*/
  int (*choose)(void *, const int32_t point[2], int32_t *, char[256]); /*47cb41*/
  int (*begin)(void *, char[256]); /*4949dc*/
  int (*action)(void *, const BkEndingFrameInput *, float, char[256]); /*48e75b*/
  int (*random)(void *, int32_t *, char[256]);
} BkEndingSelectedControlOps;
/*48d8e9: captured gate/substate; live post-callback scalar reads;
 *original FOV/camera ordering, speech loading separate from Play, replay
 *timing, physical/virtual pointer interaction and ordered clip mutations.
 *Unknown gate/substate is a native no-op after the base assignment. Required
 *child services fail when reached; no substitute action/controller is used.
 *The parent neither advances actors nor publishes their world matrices.*/
int bk_ending_selected_control_step(BkEndingSelectedControlState *,
    const BkEndingSelectedControlBindings *, const BkEndingFrameInput *,
    float seconds, const BkEndingSelectedControlOps *, char error[256]);
#endif
