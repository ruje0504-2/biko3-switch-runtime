#include "game/ending_record.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdlib.h>

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

  free(records);
  return 0;
}
