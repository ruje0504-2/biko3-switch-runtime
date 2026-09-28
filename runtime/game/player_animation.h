#ifndef BK_GAME_PLAYER_ANIMATION_H
#define BK_GAME_PLAYER_ANIMATION_H
#include "world/actor_pose.h"
typedef struct {
  int32_t idle, walk, run;
} BkPlayerAnimationActions;
/* Original4bfea9 alpha replacement; hard visibility is independent. */
float bk_player_presentation_alpha(int8_t camera_mode, int8_t interface_mode,
                                   int8_t interaction_mode);
/* Original 0x4bf290..0x4bf2cd default movement slots. */
int bk_player_animation_actions(BkPlayerAnimationActions *out);
/* Original 0x4bffd0..0x4c0078 primary-player animation submission, AFTER
 * action/physics/audio decisions. Idle advances at 0.2 seconds, others 0.5.
 * Idle/walk/run use 10-tick requests; others preserve configured transition.
 * Does not update a secondary accessory model or publish cached children. */
int bk_player_animation_step(BkActorPose *actor, const float position[3],
                             float yaw_degrees,
                             const BkPlayerAnimationActions *actions,
                             int32_t action, float seconds, char error[256]);
#endif
