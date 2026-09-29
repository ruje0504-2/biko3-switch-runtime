#include "game/ending_normal.h"
#include <stdio.h>
#include <string.h>
int bk_ending_normal_preference(unsigned group, unsigned index) {
  static const uint8_t config[5][14] = {
      {0, 0, 0, 0, 0, 1, 0, 1, 1, 0, 0, 0, 1, 0},
      {0, 0, 0, 0, 0, 1, 1, 0, 1, 0, 0, 0, 0, 1},
      {0, 0, 0, 1, 0, 0, 0, 1, 0, 1, 0, 0, 1, 0},
      {0, 0, 1, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 1},
      {0, 0, 0, 0, 1, 0, 0, 1, 1, 0, 0, 0, 1, 0}};
  return group < 5 && index < 14 ? config[group][index] : -1;
}
const char *bk_ending_normal_background(unsigned group, unsigned variant) {
  static const char *const names[5][2] = {{"m01_90.xan", "m01_91.xan"},
                                          {"m02_90.xan", "m02_91.xan"},
                                          {"m03_90.xan", "m03_91.xan"},
                                          {"m04_90.xan", "m04_91.xan"},
                                          {"m05_90.xan", "m05_91.xan"}};
  return group < 5 && variant < 2 ? names[group][variant] : NULL;
}
/* Fixed-EXE56f7f4..570474: five groups, two normal-ending variants. */
static const int32_t actions[5][2][80] = {
    {
        {
            12, 12, 12, 3,  1,  11, 7,  7,  6,  0,  27, 30, 30, -1, 0,  26,
            27, 27, -1, 0,  5,  33, 33, -1, 1,  9,  17, 17, -1, 1,  -1, -1,
            -1, -1, 0,  -1, -1, -1, -1, 0,  -1, -1, -1, -1, 0,  -1, -1, -1,
            -1, 0,  1,  2,  2,  -1, 0,  10, 22, 22, 5,  1,  -1, -1, -1, -1,
            0,  -1, -1, -1, -1, 0,  -1, -1, -1, -1, 0,  6,  -1, -1, -1, -1,
        },
        {
            17, 2,  -1, -1, 0,  0,  5,  -1, -1, 0,  13, 7,  -1, -1, 0,  5,
            9,  -1, -1, 0,  -1, -1, -1, -1, 0,  -1, -1, -1, -1, 0,  -1, -1,
            -1, -1, 0,  -1, -1, -1, -1, 0,  -1, -1, -1, -1, 0,  -1, -1, -1,
            -1, 0,  -1, -1, -1, -1, 0,  -1, -1, -1, -1, 0,  -1, -1, -1, -1,
            0,  -1, -1, -1, -1, 0,  -1, -1, -1, -1, 0,  6,  -1, -1, -1, -1,
        },
    },
    {
        {
            12, 12, 12, 3,  1,  11, 7,  7,  6,  0,  -1, -1, -1, -1, 0,  -1,
            -1, -1, -1, 0,  -1, -1, -1, -1, 0,  9,  17, 0,  -1, 1,  -1, -1,
            -1, -1, 0,  -1, -1, -1, -1, 0,  -1, -1, -1, -1, 0,  -1, -1, -1,
            -1, 0,  1,  2,  2,  -1, 0,  10, 22, 22, 5,  1,  5,  27, 27, -1,
            1,  -1, -1, -1, -1, 0,  -1, -1, -1, -1, 0,  6,  -1, -1, -1, -1,
        },
        {
            32, 2,  -1, -1, 0,  0,  5,  -1, -1, 0,  13, 7,  -1, -1, 0,  5,
            9,  -1, -1, 0,  -1, -1, -1, -1, 0,  -1, -1, -1, -1, 0,  -1, -1,
            -1, -1, 0,  -1, -1, -1, -1, 0,  -1, -1, -1, -1, 0,  -1, -1, -1,
            -1, 0,  -1, -1, -1, -1, 0,  -1, -1, -1, -1, 0,  -1, -1, -1, -1,
            0,  -1, -1, -1, -1, 0,  -1, -1, -1, -1, 0,  6,  -1, -1, -1, -1,
        },
    },
    {
        {
            12, 12, 12, 3,  1,  11, 7,  7,  6,  0,  -1, -1, -1, -1, 0,  -1,
            -1, -1, -1, 0,  -1, -1, -1, -1, 1,  9,  17, 17, -1, 1,  5,  33,
            33, -1, 0,  -1, -1, -1, -1, 0,  -1, -1, -1, -1, 0,  -1, -1, -1,
            -1, 0,  1,  2,  2,  -1, 0,  10, 22, 22, 5,  1,  -1, -1, -1, -1,
            0,  -1, -1, -1, -1, 0,  -1, -1, -1, -1, 0,  6,  -1, -1, -1, -1,
        },
        {
            17, 2,  -1, -1, 0,  0,  5,  -1, -1, 0,  13, 7,  -1, -1, 0,  5,
            9,  -1, -1, 0,  -1, -1, -1, -1, 0,  -1, -1, -1, -1, 0,  -1, -1,
            -1, -1, 0,  -1, -1, -1, -1, 0,  -1, -1, -1, -1, 0,  -1, -1, -1,
            -1, 0,  -1, -1, -1, -1, 0,  -1, -1, -1, -1, 0,  -1, -1, -1, -1,
            0,  -1, -1, -1, -1, 0,  -1, -1, -1, -1, 0,  6,  -1, -1, -1, -1,
        },
    },
    {
        {
            12, 12, 12, 3,  1,  11, 7,  7,  6,  0,  27, 30, 30, -1, 0,  26,
            27, 27, -1, 0,  5,  33, 33, -1, 1,  9,  17, 17, -1, 1,  -1, -1,
            -1, -1, 0,  -1, -1, -1, -1, 0,  -1, -1, -1, -1, 0,  -1, -1, -1,
            -1, 0,  1,  2,  2,  -1, 0,  10, 22, 22, 5,  1,  -1, -1, -1, -1,
            0,  -1, -1, -1, -1, 0,  -1, -1, -1, -1, 0,  6,  -1, -1, -1, -1,
        },
        {
            17, 2,  -1, -1, 0,  0,  5,  -1, -1, 0,  7,  7,  -1, -1, 0,  5,
            9,  -1, -1, 0,  -1, -1, -1, -1, 0,  -1, -1, -1, -1, 0,  -1, -1,
            -1, -1, 0,  -1, -1, -1, -1, 0,  -1, -1, -1, -1, 0,  -1, -1, -1,
            -1, 0,  -1, -1, -1, -1, 0,  -1, -1, -1, -1, 0,  -1, -1, -1, -1,
            0,  -1, -1, -1, -1, 0,  -1, -1, -1, -1, 0,  6,  -1, -1, -1, -1,
        },
    },
    {
        {
            12, 12, 12, 3,  1,  11, 7,  7,  6,  0,  27, 30, 30, -1, 0,  26,
            27, 27, -1, 0,  5,  33, 33, -1, 1,  10, 22, 22, -1, 1,  -1, -1,
            -1, -1, 0,  -1, -1, -1, -1, 0,  -1, -1, -1, -1, 0,  -1, -1, -1,
            -1, 0,  1,  2,  2,  -1, 0,  9,  17, 17, 5,  1,  -1, -1, -1, -1,
            0,  -1, -1, -1, -1, 0,  -1, -1, -1, -1, 0,  6,  -1, -1, -1, -1,
        },
        {
            32, 2,  -1, -1, 0,  0,  5,  -1, -1, 0,  12, 7,  -1, -1, 0,  5,
            9,  -1, -1, 0,  -1, -1, -1, -1, 0,  -1, -1, -1, -1, 0,  -1, -1,
            -1, -1, 0,  -1, -1, -1, -1, 0,  -1, -1, -1, -1, 0,  -1, -1, -1,
            -1, 0,  -1, -1, -1, -1, 0,  -1, -1, -1, -1, 0,  -1, -1, -1, -1,
            0,  -1, -1, -1, -1, 0,  -1, -1, -1, -1, 0,  6,  -1, -1, -1, -1,
        },
    },
};
static const char *const names[BK_ENDING_NORMAL_NODES] = {
    "A_kao",    "A_kuch",   "A_kubi",    "A_se",      "A_kosi",   "A_kokan",
    "A_asoko",  "A_hara",   "A_ana",     "A_nip_L",   "A_nip_R",  "A_mune_L",
    "A_mune_R", "A_mune_C", "A_mune_LC", "A_mune_RC", "A_te_L",   "A_te_R",
    "A_ude_L",  "A_ude_R",  "A_hiji_L",  "A_hiji_R",  "A_sode_L", "A_sode_R",
    "A_kata_L", "A_kata_R", "A_siri_L",  "A_siri_R",  "A_siri_C", "A_tuma_L",
    "A_tuma_R", "A_hiza_L", "A_hiza_R",  "A_mata_L",  "A_mata_R", "A_momo_L",
    "A_momo_R", "A_sune_L", "A_sune_R",
};
int bk_ending_normal_config(BkEndingNormalConfig *out, unsigned group,
                            unsigned variant) {
  if (!out || group >= 5 || variant > 1)
    return 0;
  static const float positions[5][3] = {
      {-0x1.6000000000000p+3f, 0x0.0p+0f, -0x1.851eb80000000p-1f},
      {0x0.0p+0f, 0x0.0p+0f, 0x0.0p+0f},
      {0x0.0p+0f, 0x0.0p+0f, 0x0.0p+0f},
      {0x0.0p+0f, 0x0.0p+0f, 0x0.0p+0f},
      {0x0.0p+0f, 0x0.0p+0f, 0x0.0p+0f},
  };
  static const int32_t yaws[5] = {270, 90, 0, 0, 0};
  static const int32_t camera_yaws[5] = {90, 90, 0, 0, 0};
  BkEndingNormalConfig next = {0};
  snprintf(next.primary, sizeof(next.primary), "h%02u_00.xan", group + 1);
  snprintf(next.auxiliary, sizeof(next.auxiliary), "h%02u_01.xan", group + 1);
  snprintf(next.face, sizeof(next.face), "h%02u_00.fam", group + 1);
  snprintf(next.bom, sizeof(next.bom), "h%02u_00.bom", group + 1);
  memcpy(next.position, positions[group], sizeof(next.position));
  next.yaw = yaws[group];
  next.camera_yaw = camera_yaws[group];
  next.expression_a = 9;
  next.expression_b = group == 3 ? 1 : 3;
  memcpy(next.actions, actions[group][variant], sizeof(next.actions));
  for (unsigned i = 0; i < 5; i++)
    for (unsigned j = 0; j < 4; j++)
      next.camera_table[i][j] = UINT32_C(0x40000000);
  *out = next;
  return 1;
}
const char *bk_ending_normal_node_name(unsigned index) {
  return index < BK_ENDING_NORMAL_NODES ? names[index] : NULL;
}
