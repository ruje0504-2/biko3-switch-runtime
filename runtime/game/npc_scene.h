#ifndef BK_GAME_NPC_SCENE_H
#define BK_GAME_NPC_SCENE_H
#include "world/collision.h"
#include "world/visibility.h"
typedef struct {
  float position[3];
  int32_t behavior;
  uint8_t hidden, visible;
  char surface_name[260];
} BkNpcSceneState;
typedef struct {
  BkSightInput cone;
  BkSightSegment sight;
  int32_t suppressed_actions[6];
  /* Original global725930, usually empty; preserved explicitly. */
  const char *excluded_surface;
} BkNpcSceneInput;
/* 0x4b3e6f: cone -> per-mesh occlusion/ground -> detection -> smoothed Y.
 * Reads caller-supplied cached head endpoints/facing. Mesh iteration order
 * and intermediate Y changes are significant. Failure is atomic. This is
 * the NPC query; it does not implement player wall sliding or prop updates. */
int bk_npc_scene_step(BkNpcSceneState *state, const BkCollision *collision,
                      const BkNpcSceneInput *input, float seconds,
                      char error[256]);
#endif
