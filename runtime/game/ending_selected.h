#ifndef BK_GAME_ENDING_SELECTED_H
#define BK_GAME_ENDING_SELECTED_H
#include <stdint.h>
/*4D1025, independent phase5/6 loader selected by argument0/1/2. Event
 *2/3/4 or7/8/9 selects the process model_paths entry and flow16 cameras.
 *This is not the normal/secondary/third actor topology or controller. */
typedef struct {
  char pack[8], primary[32], face[32], visible_nodes[3][32];
  float position[3];
  int32_t yaw, camera_yaw, expression_a, expression_b, expression_mode;
  uint32_t event, phase, camera_table[5][4];
} BkEndingSelectedConfig;
/*The caller supplies the outer normalized action variant0/1. Invalid input
 *leaves output unchanged. primary is the process-initial path basename;
 *the resource loader still receives the actual retained model_paths[event]. */
int bk_ending_selected_config(BkEndingSelectedConfig *, unsigned group,
                               unsigned variant, unsigned selection);
/*4D1C64..4D1CB9: only group1/phase5 replaces the background, and only
 *outside selected2/3/4. Returns -1 for an invalid group/action variant. */
int bk_ending_selected_replaces_background(unsigned group, unsigned variant,
                                           int32_t selected);
#endif
