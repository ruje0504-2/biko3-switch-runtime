#ifndef BK_GAME_ENDING_SELECTED_ACTION_H
#define BK_GAME_ENDING_SELECTED_ACTION_H
#include "game/ending_selected_control.h"
#include "game/ending_selected_motion.h"
#include "game/ending_record.h"

/* Additional process owners used by48E75B. Reloading an actor does not
 * initialize these fields. Existing control/UI/retained aliases are borrowed
 * below instead of copied into a second state object. */
typedef struct {
  float end_difference;                       /*6dde88*/
  uint32_t clock_sample, previous_clock;       /*6dde8c/6e9fa0*/
  uint8_t saved_toggle;                        /*6dde9c*/
  int8_t replay_variant;                       /*6ddea5*/
  uint32_t saved_target[3];                    /*6ddeb0/b4/b8*/
  float drag_result;                          /*6ea154*/
  uint32_t saved_camera[4];                    /*6ea15c/160/164/168*/
  float speech_elapsed;                       /*6ea364*/
  int8_t speech_ready, motion_direction;       /*55696c/6ea368*/
  int32_t motion_state, counter;              /*6ea36c/370*/
  float completion_elapsed;                   /*6ea374*/
  int32_t crossings;                          /*6ea378*/
  float previous_motion, previous_progress;   /*6ea37c/380*/
  int32_t diagnostic_crossings, diagnostic_ms; /*725704/70c*/
} BkEndingSelectedActionState;
BkEndingSelectedActionState bk_ending_selected_action_initial(void);

typedef struct {
  BkEndingSelectedControlBindings control;
  BkEndingSelectedControlState *controller;
  BkEndingCameraPresets *presets; /*same owner as control.presets, writable*/
  int32_t *selected;             /*same owner as control.selected*/
  BkEndingRecords *records;
  uint8_t (*working)[8];         /*five721dc6 working rows, not persistent save*/
  int32_t *face_mode;            /*721dfc*/
  int32_t *once, *random_latch, *reset_a; /*6ea310/6ddea0/6ea344*/
  int32_t *small_motion, *large_motion, *fast_motion; /*6dde90/6ddec8/6ea16c*/
  int32_t *reverse, *previous_sound, *stage; /*6ea314/5546a0/6ea348*/
  int32_t *reset_340, *reset_354, *words_318; /*last array has10words*/
  int32_t *group_seen;           /*five fourth words of64-byte6ea180 rows*/
  int32_t (*group_suffix)[12];   /*same rows' last12words*/
  float *sequence_elapsed;       /*6ea350*/
  uint8_t *sequence;             /*6ea34c, not adjacent sequence_count*/
  float *animation_scale;        /*55469c, distinct from output scale721ad0*/
} BkEndingSelectedActionBindings;

/* Native byte offsets within a real stage UI slot. Keeping the byte field
 * explicit prevents confusing +166 with the separate +167 flash request. */
typedef enum {
  BK_ENDING_SELECTED_UI_BYTE_134,
  BK_ENDING_SELECTED_UI_BYTE_166,
  BK_ENDING_SELECTED_UI_BYTE_167
} BkEndingSelectedUiByte;
typedef struct {
  BkEndingSelectedControlOps control;
  /* Views do not advance, request or publish an actor. A captured slot stays
   * the same across callbacks even if the active descriptor changes. */
  int (*clip)(void *, unsigned, BkEndingSelectedMotionClip *, char[256]);
  int (*source)(void *, unsigned, float, char[256]);
  int (*pointer)(void *, const BkEndingFrameInput *, char[256]); /*495469*/
  int (*drag)(void *, const float motion[2], float *native_result, char[256]);
  int (*hit)(void *, unsigned menu, const int32_t pointer[2], int *, char[256]);
  const int32_t *choices; /*two live7220e4 menu labels*/
  int (*clock)(void *, uint32_t *, char[256]);
  /* Direct buffer Stop differs from the nullable4ad34a wrapper. The called
   * slot must have a real owner; absence is an error, including speech0 in
   * the mode4/6 cancellation branch. */
  int (*stop)(void *, unsigned slot, char[256]);
  int (*repeat)(void *, unsigned slot, char[256]); /*401f71, not4018c8*/
  int (*ui_byte)(void *, unsigned slot, BkEndingSelectedUiByte, uint8_t, char[256]);
  int (*ui_uv_reset)(void *, unsigned slot, char[256]);
  int (*ui_fade)(void *, unsigned slot, float, char[256]);
} BkEndingSelectedActionOps;

/* All eight48E75B modes. State/record writes and service calls follow the
 * original order; later failure retains the executed prefix. Missing required
 * services fail when reached. This controller does not sample/publish actors,
 * save persistent unlocks, or register the unfinished4D1025 scene lifecycle. */
int bk_ending_selected_action_step(BkEndingSelectedActionState *,
    const BkEndingSelectedActionBindings *, const BkEndingFrameInput *,
    float seconds, const BkEndingSelectedActionOps *, char error[256]);
#endif
