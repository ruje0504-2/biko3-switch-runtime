#include "game/ending_record.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
  char error[256] = {0};
  BkEndingRecords *records = calloc(1, sizeof(*records));
  assert(records);

  records->groups[2].retained[0][7] = 0x12345678;
  assert(bk_ending_record_clear(records, 2, error));
  assert(records->groups[2].count == 0);
  assert(records->groups[2].actions[0] == -1);
  assert(records->groups[2].actions[BK_ENDING_RECORD_CAPACITY - 1] == -1);
  assert(records->groups[2].retained[0][7] == 0x12345678);
  assert(!bk_ending_record_clear(records, BK_ENDING_RECORD_GROUPS, error));

  BkEndingRecord *r = &records->groups[0];
  assert(bk_ending_record_clear(records, 0, error));
  int32_t target = -99;
  assert(bk_ending_record_normal_choice(r, 8, 1, &target, error));
  assert(target == 0 && r->count == 1 && r->actions[0] == 0);
  assert(bk_ending_record_normal_choice(r, 8, 27, &target, error));
  assert(target == 5 && r->count == 2 && r->actions[1] == 5);
  target = 77;
  assert(bk_ending_record_normal_choice(r, 8, 1234, &target, error));
  assert(target == 77 && r->count == 3 && r->actions[2] == -1);
  assert(bk_ending_record_normal_choice(r, 0x18, 12, &target, error));
  assert(target == 2 && r->count == 3);

  r->count = 0;
  r->retained[1][BK_ENDING_RECORD_CAPACITY - 1] = 4;
  assert(bk_ending_record_append_unique(r, 4, error));
  assert(r->count == 0);
  assert(bk_ending_record_append_unique(r, 5, error));
  assert(r->count == 1 && r->actions[0] == 5);
  r->count = BK_ENDING_RECORD_CAPACITY;
  r->actions[BK_ENDING_RECORD_CAPACITY - 1] = 6;
  assert(bk_ending_record_append_unique(r, 6, error));
  assert(r->count == BK_ENDING_RECORD_CAPACITY);
  assert(!bk_ending_record_append_unique(r, 7, error));

  /* A saved lane has its own terminator, independent of the current count
   * and the other lane. Reject empty/truncated data without changing it. */
  memset(r, 0, sizeof(*r));
  BkEndingRecord *snapshot = malloc(sizeof(*snapshot));
  assert(snapshot);
  for (unsigned lane = 0; lane < 2; ++lane) {
    const unsigned ends[] = {0, 1, 4999, BK_ENDING_RECORD_CAPACITY - 1};
    for (unsigned k = 0; k < sizeof(ends) / sizeof(*ends); ++k) {
      memset(r, 0xff, sizeof(*r));
      r->retained[lane][ends[k]] = 0x12340015; /* Native low-byte dispatch. */
      r->count = k & 1 ? -1 : BK_ENDING_RECORD_CAPACITY + 1;
      memcpy(snapshot, r, sizeof(*r));
      assert(bk_ending_record_replay_ready(r, lane, error));
      assert(!bk_ending_record_replay_ready(r, lane ^ 1u, error));
      assert(!memcmp(snapshot, r, sizeof(*r)));
    }
  }
  memset(r, 0, sizeof(*r));
  r->actions[0] = 21; r->count = 1;
  memcpy(snapshot, r, sizeof(*r));
  assert(!bk_ending_record_replay_ready(r, 0, error));
  assert(!bk_ending_record_replay_ready(r, 1, error));
  assert(!bk_ending_record_replay_ready(r, 2, error));
  assert(!bk_ending_record_replay_ready(NULL, 0, error));
  assert(!memcmp(snapshot, r, sizeof(*r)));
  free(snapshot);

  free(records);
  return 0;
}
