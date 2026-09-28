#ifndef BK_SCENE_BACKGROUND_AUDIO_H
#define BK_SCENE_BACKGROUND_AUDIO_H
#include "media/audio.h"
#include "scene/background_assets.h"
typedef struct BkBackgroundAudio BkBackgroundAudio;
/* Owns9 clips, borrows mixer; reserves9 consecutive voices (music then
 * ambient0..7). Starts music at-6000 and authored continuous ambience.
 * Sets state's music_volume only; retained mode/timer/latches stay intact. */
BkBackgroundAudio *
bk_background_audio_create(BkResourceStore *, BkAudio *, unsigned first_voice,
                           const BkBackgroundConfig *, BkBackgroundState *,
                           int32_t effect_volume, char error[256]);
void bk_background_audio_destroy(BkBackgroundAudio *);
int bk_background_audio_stop(BkBackgroundAudio *, char error[256]);
/* Fills actual ambient presence/playing/source, consumes frame audio before
 * animation. No implicit device poll/fill or hierarchy publication. */
int bk_background_audio_step(BkBackgroundAudio *, BkBackgroundAssets *,
                             BkBackgroundState *, uint32_t *shared_random,
                             const BkBackgroundInput *, BkBackgroundCommands *,
                             char error[256]);
/*51a77c/4f720c: adjusts gain without playback status, transport, random
 * timer, or model animation. Retains all playback cursors. */
int bk_background_audio_pause_step(BkBackgroundAudio *, BkBackgroundState *,
                                   const BkBackgroundInput *, char error[256]);
#endif
