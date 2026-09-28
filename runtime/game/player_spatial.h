#ifndef BK_GAME_PLAYER_SPATIAL_H
#define BK_GAME_PLAYER_SPATIAL_H
#include "game/player_movement.h"
#include "game/player_scene.h"
#include "world/placement.h"
typedef struct {
  BkPlayerMovement movement;
  BkPlayerScene scene;
  /* Native+2a8 is chosen BEFORE ground correction, deliberately not refreshed
   * after it. This is consumed by the subsequent player camera. */
  float vertical_position;
} BkPlayerSpatial;
typedef struct {
  BkPlayerMovementInput movement;
  /* previous/motion/height below are replaced by the movement result.
   * Camera rays and cached NPC screen projection must be supplied by caller. */
  BkPlayerSceneInput scene;
  float base_head_height, cached_head_height;
} BkPlayerSpatialInput;
/* Compose movement through4c0ce3 (primary root application boundary).
 * Caller applies4c20ee interaction/action mutations before this stage.
 * Slots8/9 use cached player head Y; slots11/12/13/14/16/17 skip physics,
 * updating only screen visibility and clearing near_wall. Output placement
 * is applied without advancing/publishing animation. Atomic CPU outputs.
 * UI/interaction triggers after the root and scripted4c0d9e are separate. */
int bk_player_spatial_step(BkPlayerSpatial *state, const BkCollision *collision,
                           const BkPlayerSpatialInput *input,
                           BkActorPlacement *placement, char error[256]);
/*4c1313 noninteractive phase0/2 player: action=idle, save previous position,
 * collision/ground using the RETAINED vertical/velocity/turn inputs, place,
 * interaction_mode=0. No movement integration or shadow placement. Caller
 * requests idle before invoking this, without advancing the animation. */
int bk_player_idle_step(BkPlayerSpatial *, int32_t idle_action,
                        const BkCollision *, const BkPlayerSceneInput *,
                        BkActorPlacement *, char error[256]);
#endif
