#ifndef BK_GAME_PROP_SOUND_H
#define BK_GAME_PROP_SOUND_H
#include "game/prop_motion.h"
#include "world/spatial_audio.h"
typedef struct {
  int8_t stage;
} BkPropSoundState;
typedef struct {
  float listener[3], listener_yaw;
  int32_t effect_volume;
  int voice_present, voice_playing;
} BkPropSoundInput;
typedef struct {
  /* Resource names are in bk3_02. release occurs before load. loop loads
   * auto-start; play explicitly starts a nonloop load. NULL means no load. */
  const char *file;
  int32_t initial_volume;
  int release, loop, play, spatial, frequency;
  BkSpatialAudio gain;
  float source[3];
} BkPropSoundCommand;
/*512853 does NOT reset retained sound stage. Loaded one-shots remain stopped.
 */
int bk_prop_sound_initial(int32_t kind, int32_t effect_volume,
                          BkPropSoundCommand *);
/*512c0e/514b9a/514f0e; policy only. At spatial==1 apply gain; if frequency==1
 * then SetFrequency(original clip Hz +2*gain.volume). Caller queries the
 * latest playback status, and commits commands synchronously before display. */
int bk_prop_sound_step(BkPropSoundState *, const BkPropState *,
                       const BkPropSoundInput *, BkPropSoundCommand *);
#endif
