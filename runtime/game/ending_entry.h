#ifndef BK_GAME_ENDING_ENTRY_H
#define BK_GAME_ENDING_ENTRY_H
#include "game/ending_auxiliary.h"
#include "game/ending_control.h"
typedef enum {
  BK_ENDING_LOAD_4CF318,
  BK_ENDING_LOAD_4D00FA,
  BK_ENDING_LOAD_4D1025,
  BK_ENDING_LOAD_4D2320,
  BK_ENDING_LOAD_4D39E6
} BkEndingLoader;
typedef struct {
  BkEndingFrameState *frame;
  BkEndingControlState *control;
  BkEndingAuxiliaryState *auxiliary;
  uint8_t *option_a, *option_b; /*71bcdd/de, shared retained preferences*/
  int32_t *selected_group;      /*7219a8*/
  float *gauge_y;               /*721e24*/
} BkEndingEntryBindings;
typedef struct {
  void *context;
  /* These are required actual resource loaders, not success placeholders.
   * Only4d1025 has an argument; other calls receive -1. May mutate live state.
   */
  int (*load)(void *, BkEndingLoader, int32_t argument, char error[256]);
  /* Original40,000-byte record fill FF and its counter0, AFTER normal load.
   * Outer owner manages the real per-group recording, not a progress unlock. */
  int (*clear_record)(void *, unsigned group, char error[256]);
  int (*prepare_final)(void *, char error[256]); /*48d7f2, before final load*/
} BkEndingEntryOps;
/* Original4cc7e6..4ccabd entry dispatch. The earlier process reset/table copy
 * and later UI/audio initialization remain separate mandatory loader stages.
 * Requires live preinitialized fields; it does not zero them on entry.
 * Previous flow8 selects normal entry; flow18(hex) uses gallery selection.
 * Other previous flows and unsigned selection>6 retain original no-op.
 * Preserves ordered prefix on failure and rereads state after services. */
int bk_ending_entry_dispatch(const BkEndingEntryBindings *,
                             int8_t previous_flow, uint32_t gallery_selection,
                             float scale, const BkEndingEntryOps *,
                             char error[256]);
#endif
