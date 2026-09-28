#ifndef BK_GAME_NPC_CONTACT_H
#define BK_GAME_NPC_CONTACT_H
#include <stdint.h>
typedef struct {
  int32_t group, area;
  uint32_t cursor;
  float actor_position[3], alpha, player_position[3], player_direction[3];
  int8_t interaction_df, interaction_e0;
} BkNpcContactInput;
typedef struct {
  int32_t behavior;
  /* Original global bytes 71ba8c, 71bcda and 71bcd8 respectively. Values are
   * preserved state-machine codes, not a declaration of task success. */
  uint8_t prompt, response, outcome;
} BkNpcContactState;
typedef enum {
  BK_NPC_CONTACT_SCRIPTED, /* 0x500b50 */
  BK_NPC_CONTACT_WAITING,  /* 0x500e74 */
  BK_NPC_CONTACT_AREA      /* 0x500ff5 */
} BkNpcContactQuery;
/* Original query result is written separately from API validity. AREA can
 * return 1 with no contact: the AI caller must still stop this update branch.
 * Segment endpoints are player_position +/- 4*player_direction, radius15.
 * SCRIPTED has no vertical gate; other queries use player height +/-20.
 * Invalid/nonfinite/overflow input changes neither state nor result. */
int bk_npc_contact_query(BkNpcContactState *state,
                         const BkNpcContactInput *input,
                         BkNpcContactQuery query, uint8_t *result);
#endif
