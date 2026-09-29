#ifndef BK_GAME_ENDING_RELOAD_H
#define BK_GAME_ENDING_RELOAD_H
#include "game/ending_entry.h"
typedef struct {
  BkEndingFrameState *frame;
  BkEndingControlState *control;
  BkEndingAuxiliaryState *auxiliary;
  const int8_t *previous_flow;      /*721ad4, queried again after callbacks*/
  uint8_t *action, *curtain_wanted; /*shared beeb7e/7f*/
  int32_t *selected, *next_mode;    /*721ed8/719b20*/
  uint8_t *saved_toggles; /*four retained70c8d0 bytes, not a local copy*/
} BkEndingReloadBindings;
typedef enum {
  BK_ENDING_LEAVE_4E1C8F,
  BK_ENDING_LEAVE_47DBCC,
  BK_ENDING_LEAVE_49739A,
  BK_ENDING_LEAVE_47A033,
  BK_ENDING_LEAVE_482F91,
  BK_ENDING_LEAVE_48D7F2
} BkEndingLeave;
typedef enum {
  BK_ENDING_LIGHT_RESET,
  BK_ENDING_LIGHT_SELECT_BK3_L,
  BK_ENDING_LIGHT_ENABLE
} BkEndingReloadLight;
typedef struct {
  void *context;
  int (*load)(void *, BkEndingLoader, int32_t argument, char error[256]);
  int (*release)(void *, BkEndingLoader, char error[256]);
  /*Phase7: actual group g01_20..g05_20.bmp in bk3_00, full1280x960 scaled,
   * enter1/exit1/idle0, request1. Destroy corresponds to50e7c1(73a990).
   * Required ownership service, never a successful placeholder. */
  int (*final_image)(void *, int create, unsigned group, char error[256]);
  /* These six original routines reset retained scalars/tables. Actual
   * resource retirement belongs to schedule/release, not these callbacks. */
  int (*leave)(void *, BkEndingLeave, char error[256]);
  int (*lighting)(void *, BkEndingReloadLight, char error[256]);
  int (*schedule)(void *, uint8_t target, uint8_t mode, char error[256]);
  /*Shared ending bank: voices0/1, effects2..46, music47. Queries return
   * booleans. Native GetStatus failure maps to playing0; infrastructure
   * failure returns0 from the service. Pause preserves source position. */
  int (*present)(void *, unsigned slot, int *present, char error[256]);
  int (*status)(void *, unsigned slot, int *playing, char error[256]);
  int (*pause)(void *, unsigned slot, char error[256]);
} BkEndingReloadOps;
const char *bk_ending_final_image(unsigned group);
/*Complete4d9354 and4d9575 respectively. Phase8 selects by live action;
 * release stops only playing effects BEFORE reading the phase. Music and
 * speech are left to the selected real destructor. Unknown phases retain
 * the native no-op dispatch; missing required services fail. */
int bk_ending_reload_load(const BkEndingReloadBindings *,
                          const BkEndingReloadOps *, char error[256]);
int bk_ending_reload_release(const BkEndingReloadBindings *,
                             const BkEndingReloadOps *, char error[256]);
/*Complete4d679d..4d6e6e black-curtain transition segment. Called AFTER
 * curtain advance/draw/request. early_return identifies native action49's
 * jump to4d7432 (skip remaining UI tail). Later failure retains the ordered
 * prefix. This does not implement preceding/following4d499b UI scheduling. */
int bk_ending_reload_transition(const BkEndingReloadBindings *,
                                uint8_t curtain_stage,
                                const BkEndingReloadOps *, int *early_return,
                                char error[256]);
#endif
