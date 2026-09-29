#ifndef BK_GAME_ENDING_TERTIARY_H
#define BK_GAME_ENDING_TERTIARY_H
#include <stdint.h>
/*4d2320 packaged resource profile. Camera/event variant5 is independent
 * of the action/background variant0/1 selected by the outer loader. */
typedef struct {
  char primary[32], face[32], auxiliaries[2][32], visible_nodes[3][32];
  float position[3];
  int32_t yaw, camera_yaw, expression_a, expression_b, expression_mode;
  int32_t actions[80];
  uint32_t camera_table[5][4];
} BkEndingTertiaryConfig;
int bk_ending_tertiary_config(BkEndingTertiaryConfig *, unsigned group,
                              unsigned action_variant);
typedef enum {
  BK_ENDING_TERTIARY_MATERIAL_PAIR, /*54ae00: two260-byte names/group*/
  BK_ENDING_TERTIARY_MATERIAL_SIX,  /*548f88: six260-byte names/group*/
  BK_ENDING_TERTIARY_MATERIAL_FOUR  /*54b828: four260-byte names/group*/
} BkEndingTertiaryMaterialTable;
/* Empty strings are original absent entries; NULL denotes invalid indices. */
const char *bk_ending_tertiary_material_name(BkEndingTertiaryMaterialTable,
                                            unsigned group, unsigned slot);
const int32_t *bk_ending_tertiary_initial_targets(void); /*54ccb4, five words*/
#endif
