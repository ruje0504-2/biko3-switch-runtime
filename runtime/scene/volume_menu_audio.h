#ifndef BK_SCENE_VOLUME_MENU_AUDIO_H
#define BK_SCENE_VOLUME_MENU_AUDIO_H
#include "scene/volume_menu.h"
#include "media/audio.h"
typedef struct BkVolumeMenuAudio BkVolumeMenuAudio;
/* Owns the six original bk3_02 buffers in six consecutive free mixer voices.
 * Caller reserves that range until destruction. Original NULL slot7 is silent.
 * Stops retain buffers/positions; a new test click restarts from source0. */
BkVolumeMenuAudio *bk_volume_menu_audio_create(BkResourceStore *, BkAudio *,
                                               unsigned first_voice, char[256]);
void bk_volume_menu_audio_destroy(BkVolumeMenuAudio *);
int bk_volume_menu_audio_play(BkVolumeMenuAudio *, unsigned slot, int32_t volume, char[256]);
int bk_volume_menu_audio_stop(BkVolumeMenuAudio *, unsigned slot, char[256]);
int bk_volume_menu_audio_gain(BkVolumeMenuAudio *, unsigned slot, int32_t volume, char[256]);
int bk_volume_menu_audio_playing(BkVolumeMenuAudio *, unsigned slot, int *, char[256]);
#endif
