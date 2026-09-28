#include "game/dialogue_entry.h"
#include <stdio.h>
#include <string.h>
int bk_dialogue_entry_select(BkDialogueEntry *out, uint8_t previous,
                             int32_t group, int32_t area, uint8_t response,
                             char e[256]) {
  if (!out) {
    snprintf(e, 256, "dialogue entry: missing output");
    return 0;
  }
  unsigned section = 0;
  if (previous == 0x38 || previous == 2 || previous == 0x10) {
    if (group < 0 || group >= 5 ||
        (previous != 0x38 && area >= 8 && response != 3 && response != 4)) {
      snprintf(e, 256,
               "dialogue entry: undefined native selector %02x/%d/%d/%u",
               previous, group, area, response);
      return 0;
    }
    if (previous != 0x38)
      section = (area >= 8 && response == 4) ? 1 : 2;
  } else
    group = 1;
  static const int32_t ends[3][5] = {{10200, 20205, 30228, 40144, 50033},
                                     {10121, 20147, 30109, 40105, 50063},
                                     {10007, 20014, 30056, 40013, 50068}};
  static const int32_t ending_ends[2][5] = {
      {10144, 20218, 30150, 40146, 50128}, {10041, 20053, 30152, 40118, 50174}};
  BkDialogueEntry result = {0};
  snprintf(result.file, sizeof(result.file), "i%02d_%02u.txt", group + 1,
           section ? section + 1 : 0);
  memcpy(result.font, "Type_S.FTT", sizeof("Type_S.FTT"));
  result.first = (group + 1) * 10000;
  result.last = ends[section][group];
  if (previous == 0x10) {
    result.first = result.last + 1;
    result.last = ending_ends[section - 1][group];
  }
  *out = result;
  return 1;
}
