#ifndef BK_SCENE_SELECTION_AUDIO_H
#define BK_SCENE_SELECTION_AUDIO_H
#include "scene/voice_audio.h"
typedef struct BkSelectionAudio BkSelectionAudio;
/* Owns bg002; mixer and two distinct reserved voice slots are borrowed.
 * Starts looping music at the original menu master gain. Voice binding is
 * separate because502480 loads music before its camera/stage/body/voice.
 * Clear both slots with stop before destruction while the mixer is alive. */
BkSelectionAudio *bk_selection_audio_create(BkResourceStore *, BkAudio *,
                                            unsigned music, unsigned speech,
                                            int32_t volume, char error[256]);
void bk_selection_audio_destroy(BkSelectionAudio *);
int bk_selection_audio_stop(BkSelectionAudio *, char error[256]);
/* Borrow body-owned speech clip until the next bind/release/destruction.
 * Stores a stopped buffer at position0; gain is validated against the current
 * voice master, and replay supplies its actual gain. No play/pause command
 * pair is issued during binding, so a concurrent pump cannot emit speech.
 * Mixer independently retains old queued epochs until they are consumed. */
int bk_selection_audio_bind_voice(BkSelectionAudio *, BkAudioClip *,
                                  int32_t volume, char error[256]);
int bk_selection_audio_release_voice(BkSelectionAudio *, char error[256]);
int bk_selection_audio_music_gain(BkSelectionAudio *, int32_t volume,
                                  char error[256]);
/* Native46435e restarts at0. UI Stop is position-preserving until replay. */
int bk_selection_audio_voice_play(BkSelectionAudio *, int32_t volume,
                                  char error[256]);
int bk_selection_audio_voice_stop(BkSelectionAudio *, char error[256]);
int bk_selection_audio_voice_status(BkSelectionAudio *, int *playing,
                                    char error[256]);
int bk_selection_audio_voice_level(BkSelectionAudio *, BkVoiceEnvelope *,
                                   float game_seconds, float *level,
                                   char error[256]);
#endif
