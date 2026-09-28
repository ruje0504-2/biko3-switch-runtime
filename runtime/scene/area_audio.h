#ifndef BK_SCENE_AREA_AUDIO_H
#define BK_SCENE_AREA_AUDIO_H
#include "game/area_boundary.h"
#include "game/npc_route_sound.h"
#include "scene/player_audio.h"
typedef struct BkAreaAudio BkAreaAudio;
/* Owns5 boundary/route cue clips, borrows mixer/player audio. npc_voice
 * represents NPC+568, DISTINCT from footstep+448 and speech+984; caller
 * reserves it. Player cues use the existing player's effect buffer, retaining
 * its presentation voice-presence state. No implicit poll/fill. */
BkAreaAudio *bk_area_audio_create(BkResourceStore *, BkAudio *,
                                  unsigned npc_voice, BkPlayerAudio *,
                                  char error[256]);
void bk_area_audio_destroy(BkAreaAudio *);
/* Clears only the separately owned NPC cue. Player lifetime belongs to its
 * own service; the enclosing scene stops that voice separately. */
int bk_area_audio_stop(BkAreaAudio *, char error[256]);
/* Preflights all commands; applies original NPC-before-player order. Audio
 * backend failure after an earlier command is fatal, not rolled back. */
int bk_area_audio_apply(BkAreaAudio *, const BkAreaBoundaryCommands *,
                        char error[256]);
/*4f6226 shares this same NPC+568 voice; later boundary playback replaces it.
 * NULLfile is a no-op. */
int bk_area_audio_route(BkAreaAudio *, const BkNpcRouteSound *,
                        char error[256]);
#endif
