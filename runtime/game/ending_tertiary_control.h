#ifndef BK_GAME_ENDING_TERTIARY_CONTROL_H
#define BK_GAME_ENDING_TERTIARY_CONTROL_H
#include "game/ending_opening.h"
#include "game/ending_record.h"

/*476720 owns these additional process fields. The other scalar aliases
 * already have owners in ending state/UI and are borrowed below. Resource
 * replacement is not process initialization. */
typedef struct {
  int32_t last_cue;      /*6afd1c*/
  float replay_elapsed; /*6afd20*/
  int32_t replay_after;  /*54ccd4*/
} BkEndingTertiaryControlState;
BkEndingTertiaryControlState bk_ending_tertiary_control_initial(void);

typedef struct {
  BkEndingFrameState *frame;
  BkEndingControlState *control;
  BkEndingAuxiliaryState *auxiliary;
  BkMenuCamera *camera;
  BkEndingRecords *records;
  int32_t *state;               /*721ee8*/
  int32_t *face_mode;           /*721dfc*/
  int32_t *part_mode;           /*6a3c20*/
  int32_t *voice_latches;       /*6afcfc/6afd00/6afd04, three words*/
  int32_t *return_ready;        /*6afd08*/
  int32_t *unavailable;         /*6afd0c/6afd10, two words*/
  int32_t *movement_ready;      /*6afd14*/
  uint8_t *substate;            /*6afd18*/
  int32_t *expression_override; /*6a3c24*/
  int32_t *pending_effect;      /*54ccc8*/
  float *fov;                  /*54ccd0*/
  const int32_t *open;          /*72210c*/
  const int8_t *previous_flow;  /*721ad4*/
  const int8_t *finish_setting; /*71bcdc*/
  const int32_t *actions;      /*709dcc: second row of the709db8 action table;
                               * this controller reads its first15 words*/
  const int32_t (*targets)[2]; /*39 projected points721f90*/
  const int32_t *initial_targets; /*five words54ccb4*/
  const int32_t *voice_volume, *effect_volume;
  uint32_t *random;
} BkEndingTertiaryControlBindings;

typedef enum {
  BK_ENDING_TERTIARY_APPEARANCE_PAIR, /*54ae00, two names/group*/
  BK_ENDING_TERTIARY_APPEARANCE_SIX   /*548f88, six names/group*/
} BkEndingTertiaryAppearance;
typedef struct {
  void *context;
  int (*key)(void *, unsigned code, unsigned mode, uint32_t *, char[256]);
  int (*present)(void *, unsigned speech, int *, char[256]);
  int (*status)(void *, unsigned speech, int *, char[256]);
  /*479739 loads/chooses a voice; the caller separately invokes4ad2bf.
   * Do not combine them: callbacks and the volume are live between calls. */
  int (*voice)(void *, int32_t cue, unsigned speech, int32_t bank,
               int32_t select, char[256]);
  int (*play)(void *, unsigned speech, int32_t volume, char[256]);
  int (*effect)(void *, int32_t volume, char[256]); /*725034, non-looping*/
  int (*expression)(void *, int32_t a, int32_t b, int32_t mode, char[256]);
  int (*active)(void *, int32_t *, char[256]); /*primary+140*/
  int (*source)(void *, unsigned clip, float *, char[256]);
  int (*request)(void *, unsigned actor, int32_t clip, char[256]);
  int (*hidden)(void *, unsigned actor, int hidden, char[256]);
  int (*appearance)(void *, BkEndingTertiaryAppearance, unsigned group,
                     unsigned slot, int hidden, char[256]); /*4a7d10, alpha1*/
  int (*target)(void *, unsigned node, float position[3], char[256]);
  int (*camera)(void *, BkEndingOpeningCamera, int32_t choice,
                 const uint32_t offset[3], uint32_t extra, uint32_t *, char[256]);
  int (*pick)(void *, const float pointer[2], int32_t *, char[256]); /*4dac12*/
  int (*choose)(void *, const int32_t point[2], int32_t *, char[256]); /*47cb41*/
  int (*begin)(void *, char[256]); /*478eab*/
  int (*action)(void *, const BkEndingFrameInput *, float, char[256]); /*47811c*/
} BkEndingTertiaryControlOps;

/* Entire476720 parent. Six dispatch states, original callback order, audio
 * presence/status distinction, AL key results and retained timers. The
 * group record is captured at entry; callback changes to group cannot move
 * a later write into a different record. Undefined native hover-cue reads
 * and out-of-bounds target/record access explicitly fail at the access.
 * Missing required services fail, retaining the executed prefix. This CPU
 * controller alone does not register a playable entry or advance actors. */
int bk_ending_tertiary_control_step(BkEndingTertiaryControlState *,
    const BkEndingTertiaryControlBindings *, const BkEndingFrameInput *,
    float seconds, const BkEndingTertiaryControlOps *, char error[256]);
/* Production compatibility for the reachable disabled-target branch:
 * skip its uninitialized hover-cue read/voice request, preserving target,
 * expression/latches and subsequent confirm/choose. All defined branches
 * are identical to step; other undefined/bounds failures still reject. */
int bk_ending_tertiary_control_play(BkEndingTertiaryControlState *,
    const BkEndingTertiaryControlBindings *, const BkEndingFrameInput *,
    float seconds, const BkEndingTertiaryControlOps *, char error[256]);
#endif
