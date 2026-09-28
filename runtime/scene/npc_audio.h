#ifndef BK_SCENE_NPC_AUDIO_H
#define BK_SCENE_NPC_AUDIO_H
#include "media/audio.h"
#include "scene/entry_assets.h"
#include "world/voice_envelope.h"
typedef struct BkNpcAudio BkNpcAudio;
/* Resources/mixer borrowed; caller reserves two distinct voice slots. Preloads
 * seven bk3_02 footstep clips. Mixer independently retains commanded clips.
 * Call stop before destroying a live scene; destroy only frees the cache. */
BkNpcAudio *bk_npc_audio_create(BkResourceStore *, BkAudio *,
                                unsigned footsteps, unsigned speech,
                                char error[256]);
void bk_npc_audio_destroy(BkNpcAudio *);
int bk_npc_audio_stop(BkNpcAudio *, char error[256]);
int bk_npc_audio_footsteps(BkNpcAudio *, const BkEntryNpcFootsteps *,
                           char error[256]);
/* Selection belongs to game dispatch. Decode replacement before committing;
 * restart at0 on the next submission. No dialogue filename is guessed. */
int bk_npc_audio_speak(BkNpcAudio *, const char *pack, const char *name,
                       int loop, int32_t volume, char error[256]);
/*4ff69a loads/stops without playing;51b244 starts it on first camera arrival.
 * Resource failures preserve the previous owned clip; mixer errors are fatal.
 */
int bk_npc_audio_prepare_speech(BkNpcAudio *, const char *pack,
                                const char *name, int32_t volume,
                                char error[256]);
int bk_npc_audio_release_speech(BkNpcAudio *, char error[256]);
int bk_npc_audio_restart_speech(BkNpcAudio *, int loop, char error[256]);
/* 220 pre-gain interleaved PCM16 samples at consumed SOURCE frame. Native lock
 * wraps at buffer end even for non-loop voices. Inactive/underrun returns0
 * preserving shared envelope. Device failure is a hard error. */
int bk_npc_audio_voice(BkNpcAudio *, BkVoiceEnvelope *shared, float seconds,
                       float *level, char error[256]);
typedef struct {
  BkNpcFootstepActions actions;
  int32_t effect_volume;
  BkEntryNpcPresentation
      presentation; /* voice_level replaced by cursor input */
} BkNpcFrameTailInput;
/* After spatial: footsteps -> shadow placement -> voice/presentation. Poll
 * audio first, fill afterwards; publish caches only on success. Explicit live
 * bindings/state; not a game loop. Failure aborts frame, no global rollback. */
int bk_npc_frame_tail(BkEntryAssets *, BkNpcAudio *, BkNpcSpatialState *,
                      BkFaceState *, uint32_t *random, BkVoiceEnvelope *,
                      uint8_t *latches, size_t latch_count,
                      const BkNpcFrameTailInput *, char error[256]);
#endif
