#ifndef BK_SCENE_ENDING_STATE_H
#define BK_SCENE_ENDING_STATE_H
#include "game/ending_special.h"
#include "scene/ending_ui_frame.h"
/* Process-retained scalar/table ownership for the recovered ending modules.
 * No GPU/model/audio handles: their previous owners must be retired before
 * entry. This type represents known fields, NOT the raw x86 memory block.
 * Fields outside the original clear interval retain their actual prior state;
 * a zero-filled instance is not a replacement for process initialization. */
typedef struct {
  BkEndingFrameState frame;
  BkEndingControlState control;
  BkEndingAuxiliaryState auxiliary;
  BkEndingUiController ui_controller;
  int32_t selected, stage3_state, open,
      contact_index;      /*721ed8/ee8,72210c,721ed4*/
  float gauge_y;          /*721e24*/
  int32_t node_state[39]; /*721e28, distinct from node resource references*/
  int32_t targets[39][2], points[3][2], choices[3]; /*721f90,7220c8,7220e4*/
  uint8_t working[5][8];    /*721dc6; NOT persistent saved progress*/
  char speech_names[2][32]; /*modeled names at722224/722344*/
  int32_t normal_inputs[14], normal_processed[14]; /*709c70/719b64*/
  int32_t normal_ready, normal_target, next_mode;  /*719b0c/719444/719b20*/
  int8_t normal_side, final_state;                 /*719b4c/6d1c0c*/
  uint8_t saved_toggles[4];                        /*70c8d0*/
  int32_t aux_inputs[2], aux_config[5][6];         /*6ea170/6e9fa8*/
  int32_t alternate[2], unavailable[2];            /*6afd38/6afd0c*/
  char model_paths[10][260]; /*70c8fc: original leading backslash retained*/
  float special_cameras[BK_ENDING_SPECIAL_CAMERAS][4]; /*71944c*/
} BkEndingState;
typedef struct {
  void *context;
  int (*warp)(void *, float x, float y, char error[256]); /*actual4b768d*/
} BkEndingStateOps;
/* Scalar/table portion4cc582..4cc7e6. Same-precision client-origin center warp
 * precedes reset. External prior overlay b537e8 receives request0 AFTER the
 * block/processed clear, BEFORE selected/cached/request and input writes.
 * Failed warp leaves all state; later failure retains the native prefix.
 * group0..4 only. Nonzero signed-byte variant normalizes to1.
 * Does not clear unrelated retained fields, create or pretend to release
 * resources, initialize UI/audio, dispatch loaders, or enter application flow.
 */
int bk_ending_state_begin(BkEndingState *, BkFadeSprite *previous_overlay,
                          unsigned group, int8_t variant, float scale,
                          const int32_t window_origin[2],
                          const BkEndingStateOps *, char error[256]);
/* Bind the existing entry/reload dispatch to this actual state, with process
 * preferences/common fields still borrowed. No copied flags or new curtain. */
int bk_ending_state_entry_bindings(BkEndingState *, uint8_t *option_a,
                                   uint8_t *option_b, int32_t *selected_group,
                                   BkEndingEntryBindings *);
int bk_ending_state_reload_bindings(BkEndingState *, BkCommonHudState *,
                                    const int8_t *previous_flow,
                                    BkEndingReloadBindings *);
/* Resource/input views stay external; scalar target tables and notices borrow
 * the same live owners used by entry and the recovered controllers. */
typedef struct {
  const int32_t *active_clip;
  const BkClipTiming *active_timing;
  const int32_t *actions;
  const float *camera_local;
  const BkEndingUiPickBindings *pick;
  BkEndingUiNoticeState *notices;
  uint8_t *flash_wanted;
  const uint8_t *item;
  const int8_t *previous_flow;
  const int32_t *voice_volume;
  uint32_t *random;
} BkEndingStateUiViews;
int bk_ending_state_ui_bindings(BkEndingState *, BkCommonHudState *,
                                const BkEndingStateUiViews *,
                                BkEndingUiFrameBindings *);
/* Adapter for the independently recovered frame dispatcher. Load live alias
 * fields just before its call; commit only its writable request bytes at the
 * actual call boundary. Never copy its diagnostic fade stages back to UI.
 * These are explicit adapter primitives, not a whole-frame synchronization
 * wrapper: callbacks that access common must receive preceding writes and
 * publish their changes before subsequent dispatcher reads. */
int bk_ending_state_import_frame_aliases(BkEndingState *,
                                         const BkCommonHudState *,
                                         const BkEndingStageUi *);
int bk_ending_state_export_frame_requests(const BkEndingState *,
                                          BkCommonHudState *);
#endif
