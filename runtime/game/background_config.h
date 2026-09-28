#ifndef BK_GAME_BACKGROUND_CONFIG_H
#define BK_GAME_BACKGROUND_CONFIG_H
#include <stddef.h>
#include <stdint.h>
typedef struct {
  const char *file;
  float position[3], trigger;
  /*50d858:0 stopped one-shot,1 starts loop,2 loaded loop not yet started. */
  int32_t loop_mode;
  int uses_effect_volume;
} BkAmbientConfig;
typedef struct {
  const char *clip, *atr, *music, *names[3];
  int32_t bounds[4];
  BkAmbientConfig ambient[8];
} BkBackgroundConfig;
/*57ab18/57d818/580518 tables and4f75f3/4f834a/4f8b61 dispatch.
 * Basenames resolve in caller-selected bk3_03 or low-detail bk3_17, ATR loose
 * mount and bk3_02 audio. Profiles include94 authored ambient slots. */
const BkBackgroundConfig *bk_background_config(uint32_t group, uint32_t area);
#endif
