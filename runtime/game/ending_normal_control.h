#ifndef BK_GAME_ENDING_NORMAL_CONTROL_H
#define BK_GAME_ENDING_NORMAL_CONTROL_H
#include "game/ending_auxiliary.h"
#include "game/ending_control.h"
#include "game/ending_record.h"
/* Additional process fields used by4db608. Resource reload is not an
 * initialization: the selected clip and replay delay start at1 and20 in
 * the fixed executable, while the manual choice starts at-1. */
typedef struct {
  int32_t action_kind, action_column; /*721e10/7220f0, shared with4dac12*/
  int32_t selected_clip, hover_voice, replay_delay; /*57563c/719c54/575640*/
  float replay_elapsed; /*719c58*/
  int8_t manual_choice, manual_action; /*575634/719b60*/
} BkEndingNormalControlState;
BkEndingNormalControlState bk_ending_normal_control_initial(void);
typedef struct {
  BkEndingFrameState *frame;
  BkEndingControlState *control;
  BkEndingAuxiliaryState *auxiliary;
  int32_t *substate, *voice_wait, *hover_latches; /*719b50/24/54[3]*/
  int32_t *normal_ready, *normal_target; /*719b0c/719444*/
  int32_t *inputs, *processed; /*709c70/719b64, fourteen words each*/
  const int32_t *next_mode, *open, *voice_volume;
  const int8_t *previous_flow;
  const int32_t *actions; /*709db8, eighty actual loader words*/
  char *speech_name; /*722224, at least32 bytes*/
  uint32_t *random;
  BkEndingRecords *records; /*process recording lanes, optional until used*/
} BkEndingNormalControlBindings;
typedef enum {
  BK_ENDING_NORMAL_LOOP_NAME,   /*4dfbbd*/
  BK_ENDING_NORMAL_ACTION_NAME /*4dfca9*/
} BkEndingNormalVoice;
typedef struct {
  void *context;
  int (*key)(void *, unsigned code, unsigned mode, uint32_t *, char[256]);
  int (*raw_key)(void *, unsigned code, uint32_t *, char[256]);
  int (*target)(void *, const float pointer[2], int32_t *, char[256]);
  int (*present)(void *, unsigned speech, int *, char[256]);
  int (*status)(void *, unsigned speech, int *, char[256]);
  int (*pause)(void *, unsigned speech, char[256]);
  int (*load)(void *, unsigned speech, const char *, char[256]);
  int (*play)(void *, unsigned speech, int32_t flags, int32_t volume, char[256]);
  int (*name)(void *, BkEndingNormalVoice, char name[32], char[256]);
  int (*eyes)(void *, unsigned slot, char[256]);
  int (*request)(void *, unsigned actor, int32_t clip, char[256]); /*4018c8*/
  int (*actor_present)(void *, unsigned actor, int *, char[256]);
  int (*root_flag)(void *, unsigned actor, int32_t flag, char[256]); /*423a99*/
  int (*manual_camera)(void *, char[256]); /*4e252b, reads shared two bytes*/
  int (*action)(void *, int32_t clip, const BkEndingFrameInput *, char[256]);
  int (*opening)(void *, float seconds, char[256]); /*actual state4 component*/
} BkEndingNormalControlOps;
/* Entire4db608 parent. Reads live aliases after callbacks, captures the
 * recording lane before dispatch, and never advances animation/forest.
 * The opening and4dd280 remain required independently implemented children.
 * Preserve both native table strides25 and15, low-AL input checks, raw-key
 * bit15, asynchronous speech completion, and count0's preceding record.
 * Unknown state is the original no-op. Missing services and out-of-range
 * native table/record accesses fail explicitly; later failures keep prefix.
 */
int bk_ending_normal_control_step(BkEndingNormalControlState *,
                                  const BkEndingNormalControlBindings *,
                                  const BkEndingFrameInput *, float seconds,
                                  const BkEndingNormalControlOps *, char[256]);
#endif
