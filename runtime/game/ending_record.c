#include "game/ending_record.h"
#include <stdio.h>
#include <string.h>
static int fail(char e[256], const char *why) {
  if (e)
    snprintf(e, 256, "ending record: %s", why);
  return 0;
}
int bk_ending_record_clear(BkEndingRecords *records, unsigned group,
                           char e[256]) {
  if (!records || group >= BK_ENDING_RECORD_GROUPS)
    return fail(e, "invalid record group");
  BkEndingRecord *r = &records->groups[group];
  for (unsigned i = 0; i < BK_ENDING_RECORD_CAPACITY; ++i)
    r->actions[i] = -1;
  r->count = 0;
  return 1;
}
int bk_ending_record_normal_choice(BkEndingRecord *r, int8_t previous,
                                   int32_t choice, int32_t *target,
                                   char e[256]) {
  if (!target || (previous == 8 && !r))
    return fail(e, "missing choice target or story record");
  if (previous == 8 && (r->count < 0 || r->count >= BK_ENDING_RECORD_CAPACITY))
    return fail(e, "story record capacity exceeded");
  int32_t selected = -1, action = -1;
  switch (choice) {
  case 1: selected = 0; action = 0; break;
  case 11: selected = 1; action = 1; break;
  case 12: selected = 2; action = 2; break;
  case 9: selected = 3; action = 3; break;
  case 10: selected = 4; action = 4; break;
  case 26:
  case 27: selected = 5; action = 5; break;
  case 5: selected = 6; action = 7; break;
  }
  if (selected >= 0) {
    *target = selected;
    if (previous == 8)
      r->actions[r->count] = action;
  }
  if (previous == 8)
    ++r->count;
  return 1;
}
int bk_ending_record_append_unique(BkEndingRecord *r, int32_t action,
                                   char e[256]) {
  if (!r || r->count < 0 || r->count > BK_ENDING_RECORD_CAPACITY)
    return fail(e, "invalid record cursor");
  uint32_t prior = r->count ? (uint32_t)r->actions[r->count - 1]
                           : r->retained[1][BK_ENDING_RECORD_CAPACITY - 1];
  if (prior == (uint32_t)action)
    return 1;
  if (r->count == BK_ENDING_RECORD_CAPACITY)
    return fail(e, "story record capacity exceeded");
  r->actions[r->count++] = action;
  return 1;
}
int bk_ending_record_replay_ready(const BkEndingRecord *r, unsigned lane,
                                  char e[256]) {
  if (!r || lane >= 2)
    return fail(e, "invalid replay lane");
  for (unsigned i = 0; i < BK_ENDING_RECORD_CAPACITY; ++i)
    if ((r->retained[lane][i] & 255u) == 21u)
      return 1;
  return fail(e, "saved replay is missing or incomplete (no end marker)");
}
