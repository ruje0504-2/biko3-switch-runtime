#ifndef BK_GAME_BACKGROUND_H
#define BK_GAME_BACKGROUND_H
#include "core/timer.h"
#include "world/spatial_audio.h"
typedef struct {
  BkTimer ambient_timer;
  int32_t music_volume;
  int8_t music_mode, ambient_latch[8];
} BkBackgroundState;
typedef struct {
  int present, playing, loop;
  float position[3], trigger;
} BkAmbientInput;
typedef struct {
  uint32_t now;
  float seconds;
  int32_t group, area, background_clip, door_clip;
  int32_t music_master, effect_master;
  float player[3], player_yaw, npc[3];
  int background_present, weather_enabled, weather_present;
  int8_t ambient_gate;
  BkAmbientInput ambient[8];
} BkBackgroundInput;
typedef struct {
  /*0 no transport command,1 restart/play,2 stop. Gain follows transport. */
  int action, loop, spatial;
  BkSpatialAudio gain;
} BkAmbientCommand;
typedef struct {
  int music_update;
  int32_t music_volume, random_choice;
  BkAmbientCommand ambient[8];
  /* Animation order: door request/advance, background advance, weather
   * root=(0,0,10), request0/advance. Children are not published here. */
  int32_t door_request;
  int door_advance, background_advance, weather_advance;
  float seconds, weather_seconds;
} BkBackgroundCommands;
/*4f737b,4fa480,4fa753,4fab7b and50db23. Shared RNG only advances at the
 * original5000ms timer boundary. Reads old background/door clip before
 * animation commands; retains per-ambient edge latches across frames.
 * No resource/device/animation side effects; rejection preserves outputs. */
int bk_background_step(BkBackgroundState *, uint32_t *shared_random,
                       const BkBackgroundInput *, BkBackgroundCommands *);
/*4f720c used by pause51a77c: music fade and trigger0 ambient gain only.
 * Holds timer/latches, consumes no RNG and schedules no audio transport or
 * animation. Rejected input preserves state/output. */
int bk_background_pause_step(BkBackgroundState *, const BkBackgroundInput *,
                             BkBackgroundCommands *);
#endif
