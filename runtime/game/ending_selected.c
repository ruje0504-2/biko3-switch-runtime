#include "game/ending_selected.h"
#include <stdio.h>
#include <string.h>
int bk_ending_selected_config(BkEndingSelectedConfig *out, unsigned group,
                               unsigned variant, unsigned selection) {
  if (!out || group >= 5 || variant > 1 || selection >= 3) return 0;
  /*564B10, thirty names per group, three names per camera/event variant.
   *Empty names remain actual absent lookups; they are not fabricated nodes. */
  static const char *const visible[5][2][3][3] = {
      {{{"body1", "j1", "O_bo"}, {"O_body", "otoko_j1", "O_bo"},
        {"body1", "otoko_j1", "O_bo"}},
       {{"O_body", "j1", "O_bo"}, {"O_body", "j1", "O_bo"},
        {"O_body", "j1", "O_bo"}}},
      {{{"otoko", "", ""}, {"otoko", "", ""}, {"otoko", "", ""}},
       {{"otoko", "", ""}, {"otoko", "", ""}, {"otoko", "", ""}}},
      {{{"otoko", "", ""}, {"otoko", "", ""}, {"otoko", "", ""}},
       {{"otokoA", "otokoB", ""}, {"otokoB", "", ""},
        {"otokoA", "otokoC", ""}}},
      {{{"O_body", "otoko_j1", "O_bo"}, {"O_body", "otoko_j1", "O_bo"},
        {"O_body", "otoko_j1", "O_bo"}},
       {{"O_body", "j1", "O_bo"}, {"O_body", "j1", "O_bo"},
        {"O_body", "j1", "O_bo"}}},
      {{{"_O_body", "O_skl1", ""}, {"_O_body", "O_skl1", ""},
        {"_O_body", "O_skl1", "O_bo1_O_bo"}},
       {{"body1", "j1", "O_bo"}, {"O_body", "otoko_j1", "O_bo"},
        {"body1", "O_skl1", "O_bo1"}}}};
  BkEndingSelectedConfig next = {0};
  next.event = 2 + selection + variant * 5;
  next.phase = 5 + variant;
  snprintf(next.pack, sizeof(next.pack), "bk3_%02u", variant ? 13 : 10);
  unsigned suffix = next.event + (variant ? 6 : 1);
  snprintf(next.primary, sizeof(next.primary), "h%02u_%02u.xan", group + 1, suffix);
  snprintf(next.face, sizeof(next.face), "h%02u_%02u.fam", group + 1, suffix);
  for (unsigned i = 0; i < 3; ++i)
    snprintf(next.visible_nodes[i], sizeof(next.visible_nodes[i]), "%s",
               visible[group][variant][selection][i]);
  /*570D60/group*120/event*12;570C98/group*40/event*4. Yaw is a signed
   *integer converted by fild, not a float reinterpretation. */
  if (group == 0) {
    if (selection == 1) next.yaw = 270;
    if (!variant && selection == 0) next.position[2] = 5;
    if (!variant && selection == 1) {
      next.position[0] = -11.5f;
      next.position[2] = -0x1.851eb8p-1f;
    }
  } else if (group == 1 && variant && selection == 2)
    next.yaw = 180;
  /*570C28/group*24 or570C34/group*24. */
  if (group == 0 || (group == 1 && variant)) next.camera_yaw = 90;
  next.expression_a = 6;
  next.expression_b = 3;
  next.expression_mode = 1;
  /*4D1D22 copies80 bytes from571058+group*16. These overlapping source
   *ranges happen to contain the same twenty raw2.f words in the pinned EXE. */
  for (unsigned i = 0; i < 5; ++i)
    for (unsigned j = 0; j < 4; ++j)
      next.camera_table[i][j] = UINT32_C(0x40000000);
  *out = next;
  return 1;
}
int bk_ending_selected_replaces_background(unsigned group, unsigned variant,
                                           int32_t selected) {
  if (group >= 5 || variant > 1) return -1;
  return group == 1 && variant == 0 && selected != 2 && selected != 3 && selected != 4;
}
