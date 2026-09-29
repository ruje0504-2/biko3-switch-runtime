#include "game/ending_4d39e6_config.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

int bk_ending_4d39e6_config(BkEnding4d39Config *out, unsigned group,
                            unsigned variant) {
  if (!out || group >= 5 || variant > 1)
    return 0;

  /* 564B10: the second table is selected by the retail language branch. */
  static const char *const visible[5][2][3] = {
      {{"", "", ""}, {"O_body", "otoko_j1", "O_bo"}},
      {{"", "", ""}, {"otoko", "", ""}},
      {{"", "", ""}, {"otoko", "", ""}},
      {{"", "", ""}, {"O_body", "otoko_j1", "O_bo"}},
      {{"", "", ""}, {"_O_body", "otoko_j1", "_O_bo"}}};
  static const int32_t yaw[5] = {90, 0, 0, 0, 0}; /* 570CB0 */

  BkEnding4d39Config next = {0};
  snprintf(next.primary, sizeof(next.primary), "h%02u_12.xan", group + 1);
  snprintf(next.face, sizeof(next.face), "h%02u_12.fam", group + 1);
  for (unsigned i = 0; i < 3; ++i)
    snprintf(next.visible_nodes[i], sizeof(next.visible_nodes[i]), "%s",
             visible[group][variant][i]);
  next.yaw = yaw[group];
  next.camera_yaw = yaw[group];
  next.expression_a = 6;
  next.expression_b = group < 2 ? 3 : 1;
  /* 571008 is an authored float table.  Preserve its raw representation. */
  for (unsigned i = 0; i < 5; ++i)
    for (unsigned j = 0; j < 4; ++j)
      next.camera_table[i][j] = UINT32_C(0x40000000);
  *out = next;
  return 1;
}

int bk_ending_4d39e6_targets(float out[3][3], const float node[3],
                             const float anchor[3], char error[256]) {
  if (!out || !node || !anchor) {
    snprintf(error, 256, "4D39E6 targets: missing input/output");
    return 0;
  }
  for (unsigned i = 0; i < 3; ++i) {
    if (!isfinite(node[i]) || !isfinite(anchor[i])) {
      snprintf(error, 256, "4D39E6 targets: nonfinite coordinate");
      return 0;
    }
  }
  float next[3][3];
  memcpy(next[0], node, sizeof(next[0]));
  memcpy(next[1], anchor, sizeof(next[1]));
  next[2][0] = anchor[0];
  /* The original x87 path subtracts in extended precision before halving. */
  next[2][1] = (float)(((double)node[1] - anchor[1]) * .5);
  next[2][2] = (float)(((double)node[2] - anchor[2]) * .5);
  memcpy(out, next, sizeof(next));
  return 1;
}
