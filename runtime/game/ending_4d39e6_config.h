#ifndef BK_GAME_ENDING_4D39E6_CONFIG_H
#define BK_GAME_ENDING_4D39E6_CONFIG_H

#include <stdint.h>

/* 4D39E6's independent bk3_12 actor configuration.  This module owns only
 * original data tables and target arithmetic; scene/resource ownership stays
 * in scene/ending_auxiliary_assets. */
typedef struct {
  char primary[32], face[32], visible_nodes[3][32];
  float position[3];
  int32_t yaw, camera_yaw, expression_a, expression_b;
  uint32_t camera_table[5][4];
} BkEnding4d39Config;

int bk_ending_4d39e6_config(BkEnding4d39Config *, unsigned group,
                            unsigned variant);

/*4D4167..4D4203 copies the OLD published nodes5/13/0 in that order.
 *A_okosi719B40 is a separate optional interaction anchor, not a camera target.*/
int bk_ending_4d39e6_targets(float output[3][3], const float node5[3],
                             const float node13[3], const float node0[3],
                             char error[256]);

#endif
