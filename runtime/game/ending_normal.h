#ifndef BK_GAME_ENDING_NORMAL_H
#define BK_GAME_ENDING_NORMAL_H
#include <stdint.h>
#define BK_ENDING_NORMAL_NODES 39
typedef struct {
  char primary[32], auxiliary[32], face[32], bom[32];
  float position[3];
  int32_t yaw, camera_yaw, expression_a, expression_b;
  /*4d4823 copies all80 words; consumers interpret their original offsets. */
  int32_t actions[80];
  uint32_t camera_table[5][4];
} BkEndingNormalConfig;
/* Production4cf318 packaged branch, after4cc582 normalizes variant to0/1.
 * Resource identity comes from the native dispatch, never BOM metadata.
 * No live process state, retained flags or clocks are reset here. */
int bk_ending_normal_config(BkEndingNormalConfig *, unsigned group,
                            unsigned variant);
const char *bk_ending_normal_node_name(unsigned index);
/*4cc582 common background table55f4bc, after normal variant normalization.
 * The group1 special-background flag bypasses this table at the caller. */
const char *bk_ending_normal_background(unsigned group, unsigned variant);
/*57551c: original five groups of fourteen preferences, shared by the
 * action controller and normal UI feedback. -1 means an invalid index. */
int bk_ending_normal_preference(unsigned group, unsigned index);
#endif
