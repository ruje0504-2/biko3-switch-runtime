#ifndef BK_GAME_PLAYER_SCENE_H
#define BK_GAME_PLAYER_SCENE_H
#include "world/collision.h"
#include "world/player_wall.h"
typedef struct {
  BkPlayerWall wall;
  float wall_heading;
  char wall_name[260], surface_name[260];
  int32_t npc_in_view, any_wall;
} BkPlayerScene;
typedef struct {
  BkPlayerWallInput wall;
  /* Results of projecting the cached NPC head by 42d56c, not a fresh pose.
   * Original bounds are inclusive 1024*scaleX, 768*scaleY, depth<1. */
  float head_distance, projected_depth, screen_scale[2];
  int32_t screen_position[2], npc_hidden;
  const char *excluded_surface;
} BkPlayerSceneInput;
int bk_player_scene_in_view(int *visible, const BkPlayerSceneInput *input);
/* 4b39d9 with 4b3250/4b3620/4b2f8e: ALL first wall passes precede ALL
 * restore passes, then ground selection with intermediate Y mutations.
 * Clear near_wall/names/heading; retain camera/ray accumulators and normal.
 * Ground smooths by constant .4, independent of frame seconds; native's
 * -9999 no-ground sentinel is preserved. Atomic state on failure.
 * The mesh helper's unused visibility return is omitted: 4b39d9 discards it
 * and its floor branch can read uninitialized triangle locals. It has no
 * effect on npc_in_view, position or any other original observable state. */
int bk_player_scene_step(BkPlayerScene *state, const BkCollision *collision,
                         const BkPlayerSceneInput *input, char error[256]);
#endif
