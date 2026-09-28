#ifndef BK_SCENE_NPC_EVENT_AUDIO_H
#define BK_SCENE_NPC_EVENT_AUDIO_H
#include "scene/area_audio.h"
#include "scene/system_audio.h"
typedef struct BkNpcEventAudio BkNpcEventAudio;
/* Two retained AI buffers7299c0/se114,729ae0/se115. Borrow sharedNPC+568
 * area service and globalBEF050 service, never allocate duplicate voices.
 * AI volume is captured by4fad20 initialization, route gain uses live master.
 * Stop before destroy; destruction frees resources without stopping playback.
 */
BkNpcEventAudio *bk_npc_event_audio_create(BkResourceStore *, BkAudio *,
                                           unsigned ai1, unsigned ai2,
                                           int32_t initial_effect_volume,
                                           BkAreaAudio *, BkSystemAudio *,
                                           char error[256]);
void bk_npc_event_audio_destroy(BkNpcEventAudio *);
/* Stops only the two owned AI voices. Shared services have separate lifetime.
 */
int bk_npc_event_audio_stop(BkNpcEventAudio *, char error[256]);
/* All CPU commands validated first, then AI -> wait -> route, before NPC
 * footstep/presentation stages. No poll/fill. Backend failure aborts frame;
 * earlier submitted commands are not rolled back. */
int bk_npc_event_audio_apply(BkNpcEventAudio *, const BkNpcSpatialEffects *,
                             int32_t group, int32_t area,
                             const float player_position[3], float player_yaw,
                             int32_t live_effect_volume, char error[256]);
#endif
