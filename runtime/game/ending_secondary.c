#include "game/ending_secondary.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
int bk_ending_secondary_config(BkEndingSecondaryConfig *out, unsigned group) {
  if (!out || group >= 5)
    return 0;
  /*564B10, camera variant1; empty names still take the original lookup. */
  static const char *const visible[5][3] = {
      {"O_body", "otoko_j1", "O_bo"}, {"otoko", "", ""},
      {"otoko", "", ""}, {"O_body", "otoko_j1", "O_bo"},
      {"_O_body", "otoko_j1", "_O_bo"}};
  BkEndingSecondaryConfig next = {0};
  snprintf(next.primary, sizeof(next.primary), "h%02u_02.xan", group + 1);
  snprintf(next.face, sizeof(next.face), "h%02u_02.fam", group + 1);
  for (unsigned i = 0; i < 3; ++i)
    snprintf(next.visible_nodes[i], sizeof(next.visible_nodes[i]), "%s",
               visible[group][i]);
  /*570D6C + group*120,570C9C + group*40,570C24 + group*24. */
  if (group == 0) {
    next.position[0] = -15;
    next.position[2] = -0x1.851eb8p-1f;
    next.camera_yaw = 90;
  }
  next.expression_a = 6;
  next.expression_b = group < 2 ? 3 : 1;
  for (unsigned i = 0; i < 5; ++i)
    for (unsigned j = 0; j < 4; ++j)
      next.camera_table[i][j] = UINT32_C(0x40000000); /*571008*/
  *out = next;
  return 1;
}
int bk_ending_secondary_targets(float out[3][3], const float node[3],
                                 const float anchor[3], char error[256]) {
  if (!out || !node || !anchor) {
    snprintf(error, 256, "secondary ending targets: missing input/output");
    return 0;
  }
  for (unsigned i = 0; i < 3; ++i)
    if (!isfinite(node[i]) || !isfinite(anchor[i])) {
      snprintf(error, 256, "secondary ending targets: nonfinite coordinate");
      return 0;
    }
  float next[3][3];
  memcpy(next[0], node, sizeof(next[0]));
  memcpy(next[1], anchor, sizeof(next[1]));
  next[2][0] = anchor[0];
  /*The x87 subtraction is not rounded to float before division. */
  next[2][1] = (float)(((double)node[1] - anchor[1]) * .5);
  next[2][2] = (float)(((double)node[2] - anchor[2]) * .5);
  memcpy(out, next, sizeof(next));
  return 1;
}
int bk_ending_secondary_speech(unsigned group, int32_t cue, unsigned slot,
                                uint32_t flags, char names[2][32],
                                const int32_t *volume,
                                const BkEndingSecondarySpeechOps *ops,
                                char error[256]) {
  if (group >= 5 || slot >= 2 || !names || !volume || !ops) {
    snprintf(error, 256, "secondary speech: invalid live owner/group/slot");
    return 0;
  }
  char name[32];
  int count = snprintf(name, sizeof(name), "PH%u03%02d.wav", group + 1, cue);
  if (count < 0 || (unsigned)count >= sizeof(name)) {
    snprintf(error, 256, "secondary speech: formatted name exceeds capacity");
    return 0;
  }
  memcpy(names[slot], name, (size_t)count + 1);
  if (!ops->load) {
    snprintf(error, 256, "secondary speech: missing load service");
    return 0;
  }
  if (!ops->load(ops->context, slot, name, error)) return 0;
  if (!ops->play) {
    snprintf(error, 256, "secondary speech: missing play service");
    return 0;
  }
  int32_t playback_flags = (int32_t)(flags & 255u);
  if (playback_flags >= 128) playback_flags -= 256;
  return ops->play(ops->context, slot, playback_flags, *volume, error);
}
