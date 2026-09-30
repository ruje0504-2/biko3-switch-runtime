#ifndef BK_SCENE_SPECIAL_AUDIO_H
#define BK_SCENE_SPECIAL_AUDIO_H
#include "game/special_event.h"
#include "scene/voice_audio.h"
typedef struct BkSpecialAudio BkSpecialAudio;
/* Four effect voices first..first+3 plus music first+4. Borrow store/mixer
 * and original four retained loop bytes (72257d+i*120). Initial music is
 * bg036/041/037/040/038 at-6000, looping. Effect0 is se120/122/none/143/121;
 * group1 starts stopped, other existing effects loop. Unloaded loop bytes
 * are retained. Caller binds the SAME loops[0] into SpecialEventBindings.
 * Previous mixer owner must be stopped first. Decode before starting any
 * playback. Late resource failure does not start partial audio. */
BkSpecialAudio *bk_special_audio_create(BkResourceStore *, BkAudio *,
    unsigned first, unsigned group, int32_t effect_volume,
    uint8_t loops[4], char error[256]);
/* Logical stop clears effects then music once. Destruction only drops owned
 * clip references: late retirement cannot stop a replacement scene's audio. */
int bk_special_audio_stop(BkSpecialAudio *, char error[256]);
void bk_special_audio_destroy(BkSpecialAudio *);
int bk_special_audio_present(const BkSpecialAudio *, unsigned effect, int *);
/*50db23/50d858/46435e/direct Play/50d2a0 adapters. Master values are the live
 * original settings at this service call. Spatial attenuation is call.amount;
 * category0/1/2 selects music/voice/effect master. Direct Play preserves source
 * position;46435e restarts. Replaced clips retire after queued PCM consumption.
 * Normalized logical bk3_02 assets support both native packed/name forms. */
int bk_special_audio_call(BkSpecialAudio *, const BkSpecialEventAudioCall *,
    int32_t music_master, int32_t voice_master, int32_t effect_master,
    char error[256]);
int bk_special_audio_level(BkSpecialAudio *, unsigned effect,
    BkVoiceEnvelope *shared, float seconds, float *, char error[256]);
const char *bk_special_audio_music(unsigned group);
const char *bk_special_audio_initial_effect(unsigned group);
int bk_special_audio_music_volume(const BkSpecialAudio *, int32_t *);
#endif
