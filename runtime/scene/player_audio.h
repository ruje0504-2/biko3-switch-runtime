#ifndef BK_SCENE_PLAYER_AUDIO_H
#define BK_SCENE_PLAYER_AUDIO_H
#include "media/audio.h"
#include "scene/entry_assets.h"
typedef struct BkPlayerAudio BkPlayerAudio;
/* Owns nine effect clips, borrows mixer. This instance exclusively controls
 * its explicit voice slot. Caller releases voice before destroying mixer. */
BkPlayerAudio *bk_player_audio_create(BkResourceStore *, BkAudio *,
                                      unsigned voice, char error[256]);
void bk_player_audio_destroy(BkPlayerAudio *);
int bk_player_audio_stop(BkPlayerAudio *, char error[256]);
/*4bfdd6 unloads the effect buffer; unlike Stop, clears buffer presence. */
int bk_player_audio_release_buffer(BkPlayerAudio *, char error[256]);
/* External gameplay cue replaces this same player effect buffer (+458),
 * preserving the presentation service's voice-present bookkeeping. Mixer
 * retains the borrowed clip; command is one-shot at0. */
int bk_player_audio_effect(BkPlayerAudio *, BkAudioClip *, int32_t volume,
                           int32_t pan, char error[256]);
/* Fill input voice status from newest audio command, then execute the whole
 * presentation with synchronous release/load/play or stop commands before
 * animation. Does not poll/fill device or publish children. Shared bank is
 * also passed to NPC footsteps; keep player/NPC stage order in the driver. */
int bk_player_audio_presentation(BkPlayerAudio *, BkEntryAssets *,
                                 BkPlayerEventState *, uint8_t *steps,
                                 size_t step_count, uint8_t *shared,
                                 size_t shared_count,
                                 const BkEntryPlayerPresentation *,
                                 int32_t effect_volume, char error[256]);
#endif
