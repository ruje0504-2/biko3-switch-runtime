#ifndef BK_GAME_NPC_ROUTE_SOUND_H
#define BK_GAME_NPC_ROUTE_SOUND_H
#include "world/spatial_audio.h"
typedef struct {
  const char *file;
  BkSpatialAudio gain;
} BkNpcRouteSound;
/* Complete4f6226 selection/gain. An unmatched profile succeeds with fileNULL
 * and zero gain, without reading positions. Matched cues replace NPC+568.
 * Source must be the route event's pre-ground snapshot. Output atomic. */
int bk_npc_route_sound(BkNpcRouteSound *, int32_t group, int32_t area,
                       uint32_t cursor, const float source[3],
                       const float listener[3], float listener_yaw,
                       int32_t effect_volume);
#endif
