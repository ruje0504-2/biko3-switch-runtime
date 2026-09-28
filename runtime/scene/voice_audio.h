#ifndef BK_SCENE_VOICE_AUDIO_H
#define BK_SCENE_VOICE_AUDIO_H
#include "media/audio.h"
#include "world/voice_envelope.h"
/* Shared4af18f adapter for gameplay and menu speech: reads220 interleaved
 * PCM16 samples at the audible source cursor, before gain, wrapping at end.
 * Caller owns this voice's cursor queries and the global envelope lifetime.
 * Inactive/underrun returns0 without clearing that retained envelope. */
int bk_scene_voice_envelope(BkAudio *, unsigned voice, BkVoiceEnvelope *,
                            float game_seconds, float *level, char error[256]);
#endif
