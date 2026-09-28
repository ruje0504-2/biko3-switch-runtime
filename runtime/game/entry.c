#include "game/entry.h"
#include <math.h>
#include <stddef.h>
int bk_game_entry_resolve(BkEntryRequest *out, uint32_t *route_start,
                          const BkEntryProgress *progress, uint32_t group,
                          uint32_t area, uint8_t previous_flow) {
  if (!out || !route_start || !progress || group >= 5 || area >= 9)
    return 0;
  static const uint32_t returns[5] = {4, 5, 5, 6, 5};
  static const uint32_t return_cursors[5] = {66, 82, 194, 46, 230};
  if (previous_flow == 0x48)
    area = returns[group];
  uint32_t cursor = progress->cursor[group][area];
  uint32_t start = progress->start[group][area];
  /* Flow48 overrides cursor later, but start still addresses its own grid. */
  if ((previous_flow != 0x48 && cursor >= 1024) || start >= 1024)
    return 0;
  *out = (BkEntryRequest){
      group, area, previous_flow == 0x48 ? return_cursors[group] : cursor,
      previous_flow};
  *route_start = start;
  return 1;
}
/* Selection metadata from the pinned Chinese EXE. Independently checked
 * against native dispatch/initializer in original_entry_oracle.py. */
static const char *const routes[5][9] = {
    {"l01_00.ckp", "l01_01.ckp", "l01_03.ckp", "l01_02.ckp", "l01_10.ckp",
     "l01_11.ckp", "l01_12.ckp", "l01_13.ckp", "l01_14.ckp"},
    {"l02_02.ckp", "l02_00.ckp", "l02_01.ckp", "l02_03.ckp", "l02_10.ckp",
     "l02_11.ckp", "l02_12.ckp", "l02_13.ckp", "l02_14.ckp"},
    {"l03_01.ckp", "l03_00.ckp", "l03_02.ckp", "l03_03.ckp", "l03_10.ckp",
     "l03_11.ckp", "l03_12.ckp", "l03_13.ckp", "l03_14.ckp"},
    {"l04_03.ckp", "l04_01.ckp", "l04_00.ckp", "l04_02.ckp", "l04_10.ckp",
     "l04_11.ckp", "l04_12.ckp", "l04_13.ckp", "l04_14.ckp"},
    {"l05_00.ckp", "l05_02.ckp", "l05_03.ckp", "l05_01.ckp", "l05_10.ckp",
     "l05_11.ckp", "l05_12.ckp", "l05_13.ckp", "l05_14.ckp"},
};
static const float player_spawns[5][9][4] = {
    {
        {0x1.8300000000000p+8f, 0x0.0p+0f, -0x1.e000000000000p+5f,
         0x1.0e00000000000p+8f},
        {0x1.9c00000000000p+6f, 0x0.0p+0f, 0x1.6800000000000p+5f, 0x0.0p+0f},
        {-0x1.5800000000000p+5f, 0x0.0p+0f, 0x1.d800000000000p+6f,
         0x1.0e00000000000p+8f},
        {-0x1.8a00000000000p+7f, 0x0.0p+0f, -0x1.b000000000000p+5f,
         0x1.6800000000000p+7f},
        {-0x1.a200000000000p+8f, 0x0.0p+0f, -0x1.2000000000000p+4f,
         0x1.6800000000000p+6f},
        {0x1.4000000000000p+3f, 0x0.0p+0f, 0x0.0p+0f, 0x1.6800000000000p+6f},
        {0x1.3800000000000p+6f, 0x1.e000000000000p+4f, -0x1.de00000000000p+7f,
         0x0.0p+0f},
        {-0x1.e800000000000p+6f, -0x1.5000000000000p+4f, -0x1.8000000000000p+6f,
         0x0.0p+0f},
        {-0x1.c000000000000p+6f, 0x0.0p+0f, 0x0.0p+0f, 0x1.6800000000000p+6f},
    },
    {
        {-0x1.3800000000000p+6f, 0x0.0p+0f, -0x1.4d00000000000p+8f,
         0x1.0e00000000000p+8f},
        {0x1.0000000000000p+5f, 0x0.0p+0f, -0x1.ae00000000000p+7f,
         0x1.6800000000000p+6f},
        {0x1.9c00000000000p+6f, 0x0.0p+0f, 0x1.6800000000000p+5f, 0x0.0p+0f},
        {-0x1.5800000000000p+5f, 0x0.0p+0f, 0x1.3100000000000p+8f,
         0x1.0e00000000000p+8f},
        {-0x1.6f00000000000p+9f, 0x0.0p+0f, -0x1.e000000000000p+5f,
         0x1.6800000000000p+6f},
        {0x1.d800000000000p+5f, 0x0.0p+0f, -0x1.cf00000000000p+8f,
         0x1.6800000000000p+6f},
        {0x1.ae00000000000p+8f, 0x0.0p+0f, 0x1.8000000000000p+5f, 0x0.0p+0f},
        {-0x1.c800000000000p+6f, 0x0.0p+0f, 0x1.2000000000000p+7f,
         0x1.6800000000000p+6f},
        {0x1.0000000000000p+5f, 0x0.0p+0f, 0x1.6000000000000p+5f,
         0x1.6800000000000p+7f},
    },
    {
        {0x1.1600000000000p+8f, 0x0.0p+0f, 0x1.8400000000000p+8f,
         0x1.6800000000000p+7f},
        {0x1.9800000000000p+6f, 0x0.0p+0f, 0x1.8000000000000p+3f,
         0x1.6800000000000p+7f},
        {-0x1.f800000000000p+5f, 0x0.0p+0f, -0x1.b400000000000p+7f,
         0x1.0e00000000000p+8f},
        {-0x1.6800000000000p+6f, 0x0.0p+0f, 0x1.0000000000000p+3f, 0x0.0p+0f},
        {-0x1.2000000000000p+7f, 0x0.0p+0f, -0x1.3400000000000p+7f,
         0x1.6800000000000p+6f},
        {-0x1.9600000000000p+7f, 0x0.0p+0f, -0x1.b600000000000p+7f,
         0x1.6800000000000p+5f},
        {-0x1.7e00000000000p+7f, 0x0.0p+0f, -0x1.1200000000000p+7f,
         0x1.6800000000000p+5f},
        {-0x1.b000000000000p+6f, 0x0.0p+0f, 0x0.0p+0f, 0x1.6800000000000p+6f},
        {-0x1.9000000000000p+5f, 0x0.0p+0f, 0x0.0p+0f, 0x1.6800000000000p+6f},
    },
    {
        {-0x1.9400000000000p+7f, 0x0.0p+0f, 0x1.6500000000000p+8f,
         0x1.6800000000000p+7f},
        {0x0.0p+0f, 0x0.0p+0f, 0x1.2f00000000000p+8f, 0x1.6800000000000p+6f},
        {0x1.9800000000000p+6f, 0x0.0p+0f, 0x1.8000000000000p+3f,
         0x1.6800000000000p+7f},
        {-0x1.1800000000000p+5f, 0x0.0p+0f, -0x1.b400000000000p+7f,
         0x1.0e00000000000p+8f},
        {-0x1.3e00000000000p+7f, 0x0.0p+0f, -0x1.8000000000000p+1f,
         0x1.6800000000000p+6f},
        {-0x1.7c00000000000p+7f, 0x0.0p+0f, -0x1.0000000000000p+0f,
         0x1.6800000000000p+6f},
        {0x1.d800000000000p+6f, 0x0.0p+0f, -0x1.0f00000000000p+8f,
         0x1.0e00000000000p+8f},
        {-0x1.3400000000000p+7f, 0x0.0p+0f, -0x1.7800000000000p+5f,
         0x1.6800000000000p+6f},
        {-0x1.1a00000000000p+7f, 0x0.0p+0f, -0x1.b000000000000p+6f,
         0x1.6800000000000p+6f},
    },
    {
        {0x1.ec00000000000p+7f, 0x0.0p+0f, -0x1.3900000000000p+8f,
         0x1.3b00000000000p+8f},
        {-0x1.3000000000000p+5f, 0x0.0p+0f, -0x1.b200000000000p+7f,
         0x1.0e00000000000p+8f},
        {-0x1.9000000000000p+7f, 0x0.0p+0f, 0x1.9000000000000p+4f, 0x0.0p+0f},
        {0x1.4000000000000p+2f, 0x0.0p+0f, 0x1.2f00000000000p+8f,
         0x1.6800000000000p+6f},
        {0x1.8000000000000p+2f, 0x0.0p+0f, 0x1.cccccc0000000p-1f,
         0x1.6800000000000p+6f},
        {-0x1.4000000000000p+5f, 0x0.0p+0f, 0x1.5000000000000p+4f,
         0x1.6800000000000p+6f},
        {-0x1.b200000000000p+7f, 0x0.0p+0f, 0x1.8000000000000p+1f,
         0x1.6800000000000p+6f},
        {-0x1.1c00000000000p+6f, 0x0.0p+0f, -0x1.0000000000000p+1f,
         0x1.4000000000000p+6f},
        {-0x1.3400000000000p+8f, 0x0.0p+0f, -0x1.0000000000000p+1f,
         0x1.6800000000000p+6f},
    },
};
int bk_game_entry_select(BkEntrySelection *out, const BkEntryRequest *r) {
  if (!out || !r || r->group >= 5 || r->area >= 9 || r->route_cursor >= 1024)
    return 0;
  static const char *const actors[] = {"h01_80.xan", "h02_80.xan", "h03_80.xan",
                                       "h04_80.xan", "h05_80.xan"};
  static const char *const heads[] = {"atama", "kubiX", "kubiX", "atama",
                                      "atama"};
  static const uint32_t alternate[] = {66, 82, 194, 46, 230};
  const float *spawn = player_spawns[r->group][r->area];
  BkEntrySelection result = {0};
  result.route_file = routes[r->group][r->area];
  result.actor_clip = actors[r->group];
  result.head_node = heads[r->group];
  result.camera_clip = r->group == 4 && (r->area == 4 || r->area == 5)
                           ? "cam00_01.xan"
                           : "cam00_02.xan";
  for (unsigned i = 0; i < 3; i++)
    result.player_position[i] = spawn[i];
  result.player_yaw = spawn[3];
  result.route_cursor = r->route_cursor;
  if (r->previous_flow == 8 || r->previous_flow == 0x38) {
    result.phase = 0;
    result.dialogue = 1;
  } else if (r->previous_flow == 0x48) {
    result.phase = 3;
    result.route_cursor = alternate[r->group];
    result.actor_fade_out = r->group == 1;
  } else {
    result.phase = r->area >= 8 ? 3 : 2;
  }
  *out = result;
  return 1;
}
int bk_game_entry_camera_pose(BkCameraFollowPose *out,
                              const float actor_origin[3]) {
  if (!out || !actor_origin)
    return 0;
  BkCameraFollowPose pose = {0};
  pose.world[0] = pose.world[5] = pose.world[10] = pose.world[15] = 1;
  pose.world[13] = 20;
  for (unsigned i = 0; i < 3; i++) {
    if (!isfinite(actor_origin[i]))
      return 0;
    pose.position[i] = actor_origin[i];
  }
  *out = pose;
  return 1;
}
