#ifndef BK_GAME_ENDING_RECORD_H
#define BK_GAME_ENDING_RECORD_H
#include <stdint.h>
enum { BK_ENDING_RECORD_GROUPS = 5, BK_ENDING_RECORD_CAPACITY = 10000 };
/* Native B550B0 + group*1D4C4. The first two word arrays are retained
 * recording data, not spare padding: native count==0 deduplication reads
 * their last word. Their interpretation belongs to the remaining recorder.
 * This process-owned data is separate from the eight gallery unlock flags. */
typedef struct {
  uint32_t retained[2][BK_ENDING_RECORD_CAPACITY];
  int32_t actions[BK_ENDING_RECORD_CAPACITY];
  int32_t count;
} BkEndingRecord;
typedef struct {
  BkEndingRecord groups[BK_ENDING_RECORD_GROUPS];
} BkEndingRecords;
/*4cc829..4cc863: clear only the selected action lane to FF and its count to0.
 * Retain the other lanes and other groups, including their unused data. */
int bk_ending_record_clear(BkEndingRecords *, unsigned group, char error[256]);
/*4dc803..4dca1a: map cached choice to719444 and write at current count only
 * for previous flow8. Both26 and27 record5. An unmatched choice retains the
 * target/action but STILL increments the story count. No count change for
 * gallery or other previous flows. Preflight native out-of-bounds writes. */
int bk_ending_record_normal_choice(BkEndingRecord *, int8_t previous_flow,
                                   int32_t cached_choice, int32_t *target,
                                   char error[256]);
/* Repeated tail pattern4dce3e..4dce92 (and later group branches): append only
 * when the preceding word differs. At count0 the native preceding word is
 * retained[1][9999], not a synthesized sentinel. Capacity errors preserve
 * this record. Allows a duplicate at full capacity, which performs no write.
 * Caller owns flow gating, transition state and the actual action number. */
int bk_ending_record_append_unique(BkEndingRecord *, int32_t action,
                                   char error[256]);
#endif
