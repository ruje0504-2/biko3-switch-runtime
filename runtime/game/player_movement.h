#ifndef BK_GAME_PLAYER_MOVEMENT_H
#define BK_GAME_PLAYER_MOVEMENT_H
#include <stdint.h>
enum {
  BK_PLAYER_FORWARD = 1u << 0,
  BK_PLAYER_BACKWARD = 1u << 1,
  BK_PLAYER_LEFT = 1u << 2,
  BK_PLAYER_RIGHT = 1u << 3,
  BK_PLAYER_SLOW = 1u << 4,
  BK_PLAYER_LATCH_TRIGGER = 1u << 5
};
typedef struct {
  float position[3], previous[3], velocity[3];
  float yaw, pitch, turn[2], acceleration;
  int32_t action, move_latch;
  int8_t interaction_mode;
} BkPlayerMovement;
typedef struct {
  float seconds, look[2]; /* Original4b757e values, multiplied by seconds. */
  uint32_t buttons;
  int controls_allowed; /* Result of4c20ee; its action mutations already
                           applied. */
  int32_t active_clip;
  int32_t actions[21]; /* Live actor+14..64, preserve aliases. */
} BkPlayerMovementInput;
/* 4bf290..4bf3c0: restore nineteen authored player action bindings. Slots7
 * and15 are not written by original initialization and remain caller-owned. */
int bk_player_actions_initialize(int32_t actions[21]);
/* Movement part of4c0126 through4c0ba4, before root matrix/collision.
 * Save old position; resolve keys; move using OLD yaw; select actions; then
 * turn, one-step yaw wrap and pitch clamp. Input query booleans already merge
 * original keyboard/mouse/pad aliases. LEFT wins RIGHT, forward wins backward.
 * LATCH_TRIGGER represents the original(0,1,2) query without guessing its UI
 * meaning; prior latch affects speed before the trigger can change it.
 * Does not implement menus, stance/interaction selection, camera, collision,
 * publication or device key mapping. Invalid input/output math is atomic. */
int bk_player_movement(BkPlayerMovement *, const BkPlayerMovementInput *,
                       char error[256]);
#endif
