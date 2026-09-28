#ifndef BK_WORLD_SPATIAL_AUDIO_H
#define BK_WORLD_SPATIAL_AUDIO_H
#include <stdint.h>
typedef struct {
  int32_t volume, pan;
} BkSpatialAudio;
/* 50d2a0 numerical policy, before DirectSound setters. Native volume/pan
 * units retained; output volume floors at -6000, pan amplitude is1500.
 * Source yaw is irrelevant. Category/master volume chosen by caller.
 * Does not decode, load, play or claim success of an audio device. */
int bk_spatial_audio(BkSpatialAudio *out, const float source[3],
                     const float listener[3], float listener_yaw,
                     int32_t master_volume, float attenuation);
#endif
