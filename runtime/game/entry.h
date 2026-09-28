#ifndef BK_GAME_ENTRY_H
#define BK_GAME_ENTRY_H
#include "core/camera.h"
#include <stdint.h>
/* M1 camera dependency: verified entry selection only. This is not a running
 * mission, AI, dialogue implementation or save decoder. The caller supplies
 * the current route cursor from its explicit new-game/save state. */
typedef struct {
  uint32_t group, area, route_cursor;
  uint8_t previous_flow;
} BkEntryRequest;
/* Represented BF3C8C/BF3EA8 grids; process startup is zero. Later save/progress
 * restoration is explicit, never reconstructed from a currently loaded CKP. */
typedef struct {
  uint32_t cursor[5][9], start[5][9];
} BkEntryProgress;
/*4e8e3b resolves flow48's destination area BEFORE reading profile progress or
 * selecting any actor/background/prop/item/camera resource. Atomic outputs. */
int bk_game_entry_resolve(BkEntryRequest *out, uint32_t *route_start,
                          const BkEntryProgress *, uint32_t group,
                          uint32_t area, uint8_t previous_flow);
typedef struct {
  const char *route_file, *actor_clip, *head_node, *camera_clip;
  float player_position[3], player_yaw;
  uint32_t route_cursor;
  /* +0x331 is fade direction, distinct from hard hidden flag +0x328. */
  uint8_t phase, dialogue, actor_fade_out;
} BkEntrySelection;
/* Original 0x4ebfd0 flow precedence, 0x4ff78e route dispatch, 0x4fb73e
 * cursor override and 0x583218 spawn table. Failure leaves result unchanged.
 * Cursor range must be checked after loading the selected CKP. */
int bk_game_entry_select(BkEntrySelection *out, const BkEntryRequest *request);
/* Mode-2 camera reset sets identity orientation at (0,20,0); 0x4ebfd0
 * subsequently seeds smoothing from NPC origin, not from the camera frame.
 * Other loader modes must use their own verified reset path. */
int bk_game_entry_camera_pose(BkCameraFollowPose *out,
                              const float actor_origin[3]);
#endif
