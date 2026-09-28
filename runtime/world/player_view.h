#ifndef BK_WORLD_PLAYER_VIEW_H
#define BK_WORLD_PLAYER_VIEW_H
#include "core/camera.h"
#include "world/player_wall.h"
/* Explicit controller selected by game policy, no game/model dependency. */
typedef enum {
  BK_PLAYER_VIEW_HOLD,
  BK_PLAYER_VIEW_ORBIT,
  BK_PLAYER_VIEW_RAY,
  BK_PLAYER_VIEW_PROP_HEAD,
  BK_PLAYER_VIEW_PROP_LOW,
  BK_PLAYER_VIEW_WALL,
  BK_PLAYER_VIEW_TRACK,
  BK_PLAYER_VIEW_COVER,
  BK_PLAYER_VIEW_NPC_FRONT,
  BK_PLAYER_VIEW_HEAD
} BkPlayerViewKind;
typedef struct {
  BkCameraFollowPose pose; /* Actual camera frame and +420 smoothing XYZ. */
  float yaw, pitch, target_distance, distance; /* +42c/+430/+43c/+440. */
  float matrix[16]; /* +4a4; distinct from actual frame in aim-based modes. */
  float probe[3], focus[3], rays[8][3]; /* +524,+530,+53c/+55c/+57c. */
  float npc_distance, npc_heading;
  int32_t blocked[8], selected_ray;
  float lean; /* Shared original559a80; caller retains across controllers. */
} BkPlayerView;
typedef struct {
  float position[3], head[3], vertical, yaw, pitch;
  float npc_position[3], npc_yaw, npc_height, npc_vertical;
  float seconds;
  uint32_t buttons; /* COVER only: bit0 left,bit1 right; left wins. */
  /* TRACK: cached track world after root placement and timeline advance
   * (without publication), plus slot0 source/end AFTER that advance. */
  float track[3], track_source, track_end;
} BkPlayerViewInput;
typedef struct {
  int root_hidden;   /* -1 unchanged, otherwise recursive assignment0/1. */
  int player_hidden; /* -1 unchanged, otherwise logical player+328. */
  int write_return_yaw, reset_mode;
  float return_yaw;
} BkPlayerViewEffects;
/* Wall response reads camera+0x524 (probe), not the rendered camera frame.
 * Fill only camera/rays; caller retains previous/motion/height inputs. */
int bk_player_view_collision_query(const BkPlayerView *, BkPlayerWallInput *);
/* Pure CPU controller math. TRACK's actual XAN advance/root placement is
 * owned by the scene adapter; this interface consumes its cached results.
 * FOV1 is the caller's common policy. Invalid geometry/math is atomic. */
int bk_player_view_step(BkPlayerView *state, BkPlayerViewKind kind,
                        const BkPlayerViewInput *input,
                        BkPlayerViewEffects *effects, char error[256]);
#endif
