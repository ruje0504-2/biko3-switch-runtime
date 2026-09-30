#ifndef BK_GAME_ENDING_CAMERA_CONFIG_H
#define BK_GAME_ENDING_CAMERA_CONFIG_H
#include "world/ending_camera.h"
/*4be4f8(flow0x10) for five profiles and ten authored event variants. Copies
 * the three presets to both retained and active banks, preserving raw floats.
 * Does not change the live camera or derive a variant from gameplay state. */
int bk_ending_camera_config(BkEndingCameraPresets *, unsigned group,
                            unsigned variant);
/*4be4f8(flow48), independent of ending variants.*/
int bk_special_event_camera_config(BkEndingCameraPresets *, unsigned group);
#endif
