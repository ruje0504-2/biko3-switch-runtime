#ifndef BK_GAME_ENDING_SECONDARY_CONTROL_H
#define BK_GAME_ENDING_SECONDARY_CONTROL_H
#include "game/ending_opening.h"
/* Process-retained47A5D0 fields. The parent's counter/alternation are
 *54e284/88, distinct from47D3CB's54e290/94/98. Its rate54cd04 is borrowed
 * from the presentation owner. Resource reload is not initialization. */
typedef struct {
  int32_t remaining, alternate; /*54e284/54e288*/
  float fov;                   /*54e28c*/
  int32_t automatic, chance;    /*6afd40/6afd34*/
  float saved_camera[4];        /*6bbe1c..28: yaw/pitch/radius/height*/
  uint8_t saved_toggle;         /*6afd30, shared with later ending stages*/
} BkEndingSecondaryControlState;
BkEndingSecondaryControlState bk_ending_secondary_control_initial(void);
/* Original table54e194: five groups, three yaw/pitch/radius/height rows. */
const float *bk_ending_secondary_control_camera(unsigned group, unsigned row);
typedef struct {
  BkEndingFrameState *frame;
  BkEndingControlState *control;
  BkEndingAuxiliaryState *auxiliary;
  BkMenuCamera *camera;
  uint8_t *substate;       /*existing retained.stage3.byte_6bbe34*/
  int32_t *voice_latches;  /*existing retained.stage3.words_6bbe2c[2]*/
  float *rate;            /*same54cd04 used by47D3CB*/
  int32_t *next_mode;     /*719b20*/
  const int8_t *previous_flow;
  const int32_t *open;    /*72210c*/
  const int32_t (*targets)[2]; /*39 actual projected points721f90*/
  const int32_t (*points)[2];  /*three menu centers7220c8*/
  const int32_t *choices, *alternate_point; /*7220e4[3]/6afd38[2]*/
  const float *menu_width; /*7389e8; circle radius is this value/2*/
  uint32_t *random;
} BkEndingSecondaryControlBindings;
typedef struct {
  float end, source;
} BkEndingSecondaryControlTiming;
typedef struct {
  void *context;
  int (*key)(void *, unsigned code, unsigned mode, uint32_t *, char[256]);
  int (*present)(void *, unsigned speech, int *, char[256]);
  int (*status)(void *, unsigned speech, int *, char[256]);
  int (*voice)(void *, unsigned cue, unsigned speech, int32_t flags, char[256]); /*47d9ee*/
  int (*eyes)(void *, unsigned slot, char[256]); /*4dfb96's actual4a07a9*/
  int (*request)(void *, int32_t clip, char[256]); /*4018c8*/
  int (*restart)(void *, int32_t clip, char[256]); /*401f71, force reselect*/
  int (*active)(void *, int32_t *, char[256]);
  int (*timing)(void *, int32_t clip, BkEndingSecondaryControlTiming *, char[256]);
  int (*target)(void *, float position[3], char[256]); /*old721ef4 world*/
  int (*camera)(void *, BkEndingOpeningCamera, int32_t choice,
                 const uint32_t offset[3], uint32_t extra, uint32_t *, char[256]);
  int (*pick)(void *, const float pointer[2], int32_t mode, int32_t *, char[256]); /*47c334*/
  int (*choose)(void *, const int32_t point[2], int32_t *, char[256]); /*47cb41*/
  int (*action)(void *, char[256]); /*47cd36*/
} BkEndingSecondaryControlOps;
/* Entire47A5D0, all eight states. Captures dispatch/input but rereads shared
 * aliases after callbacks. Includes native4a777e strict circle hit, low-AL
 * keys, audio-presence/status distinction, original CRT randomness, old-world
 * targets and three-view save/restore. Never advances actors or publishes a
 * forest. Required child services must be real or fail; this rule component
 * alone is not a playable secondary entry. Failure retains executed prefix.
 * Unknown states/substates are original no-ops. Undefined native table reads
 * are rejected at the access, not replaced with a camera or target guess. */
int bk_ending_secondary_control_step(BkEndingSecondaryControlState *,
    const BkEndingSecondaryControlBindings *, const BkEndingFrameInput *,
    float seconds, const BkEndingSecondaryControlOps *, char error[256]);
#endif
