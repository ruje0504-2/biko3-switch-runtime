#ifndef BK_GAME_PLAYER_TRIGGER_H
#define BK_GAME_PLAYER_TRIGGER_H
#include <stdint.h>
typedef struct {
  float origin[4], target[4]; /* XYZ and yaw: native+7d8/+7e8. */
  int32_t prop_kind;          /* +800, not the selected array index. */
} BkPlayerTrigger;
typedef struct {
  int32_t active, kind;
  float position[3];
} BkPlayerTriggerProp;
typedef struct {
  float position[3], wall_heading;
  int32_t action, actions[21], group, area;
  const char *wall_name;
  BkPlayerTriggerProp props[16];
} BkPlayerTriggerInput;
/* 4c2678/4c2849: variant0 types10/19/18, variant1 type11. First active
 * object in original order within inclusive XZ20; no vertical gate.
 * Already in slot11/13 respectively returns hit without refreshing target. */
int bk_player_trigger_prop(BkPlayerTrigger *state, int *hit,
                           const BkPlayerTriggerInput *input, unsigned variant,
                           char error[256]);
/* 4c29f7: group/area maps eight literal names, including literal "NULL".
 * Already slot16 returns hit even if current name misses; slot17 cannot
 * restart. Matching wall target is 15 units along wall_heading+180.
 * Failure preserves state/hit. No prop instantiation or animation occurs. */
int bk_player_trigger_wall(BkPlayerTrigger *state, int *hit,
                           const BkPlayerTriggerInput *input, char error[256]);
/* 4c32cc: exact case-sensitive wall name at group0..4/area0..8. */
int bk_player_trigger_cover(int *hit, const BkPlayerTriggerInput *input);
/* 4c5078: membership only, without slot16/17 overrides or target writes. */
int bk_player_trigger_wall_available(int *hit, const BkPlayerTriggerInput *input);
#endif
