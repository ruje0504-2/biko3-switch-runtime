#ifndef BK_SCENE_DIALOGUE_AUDIO_H
#define BK_SCENE_DIALOGUE_AUDIO_H
#include "resource/dialogue.h"
#include "scene/dialogue_media.h"
#include "scene/voice_audio.h"
typedef struct BkDialogueAudio BkDialogueAudio;
/* Owns decoded clips; borrows store, mixer and two distinct reserved slots.
 * Creation clears those slots, but does not load or start music. Stop while
 * mixer is alive before destroy. Queued audible epochs retain their clips. */
BkDialogueAudio *bk_dialogue_audio_create(BkResourceStore *, BkAudio *,
                                          unsigned music, unsigned speech,
                                          char error[256]);
int bk_dialogue_audio_stop(BkDialogueAudio *, char error[256]);
void bk_dialogue_audio_destroy(BkDialogueAudio *);
int bk_dialogue_audio_music_open(BkDialogueAudio *, BkDialogueMusic *,
                                 BkDialogue *, char error[256]);
int bk_dialogue_audio_music_step(BkDialogueAudio *, BkDialogueMusic *,
                                 BkDialogue *, float game_seconds,
                                 int32_t music_master, char error[256]);
int bk_dialogue_audio_speech_step(BkDialogueAudio *, BkDialogue *,
                                  int32_t voice_master, char error[256]);
int bk_dialogue_audio_speech_present(const BkDialogueAudio *);
/*4f0e44 click-stop preserves source position. No implicit replay. */
int bk_dialogue_audio_speech_pause(BkDialogueAudio *, char error[256]);
int bk_dialogue_audio_voice_level(BkDialogueAudio *, BkVoiceEnvelope *,
                                  float game_seconds, float *level,
                                  char error[256]);
#endif
