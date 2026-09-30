#ifndef BK_PROBE_ENDING_RECORD_NATURAL_INPUT_H
#define BK_PROBE_ENDING_RECORD_NATURAL_INPUT_H
#include "scene/scene.h"
/* Test driver state only. The observer reads the loaded scene and searches
 * using scratch pick outputs; all live changes go through returned BkInput. */
typedef struct {
  unsigned frames, held_frames, phases;
  uint32_t previous_buttons;
  BkInput aimed;
  int has_aim, phase, secondary, gate, ready;
  float progress;
} BkRecordNaturalInput;
/* Ordinary route phases1/2/7 up to the real phase5 loader. The incoming
 * story boundary remains explicit. Variant1 phases3/4 are not implemented
 * by this test driver and are rejected, not bypassed. */
int record_probe_early_input(BkScene *, BkRecordNaturalInput *, BkInput *,
                             char error[256]);
/* Third route phase3 until its actual phase4/6 transition. All changes use
 * input; an entry inventory fixture is separate from natural item pickup. */
int record_probe_third_input(BkScene *, BkRecordNaturalInput *, BkInput *,
                             char error[256]);
int record_probe_inventory(BkScene *, const uint8_t inventory[5],
                            int check_cursor, char error[256]);
int record_probe_inventory_lifecycle(BkScene *, char error[256]);
#endif
