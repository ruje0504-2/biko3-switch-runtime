#ifndef BK_INPUT_H
#define BK_INPUT_H
#include <stdint.h>
enum {
  BK_BUTTON_CONFIRM = 1u << 0,
  BK_BUTTON_BACK = 1u << 1,
  BK_BUTTON_PAUSE = 1u << 2,
  BK_BUTTON_UP = 1u << 3,
  BK_BUTTON_DOWN = 1u << 4,
  BK_BUTTON_LEFT = 1u << 5,
  BK_BUTTON_RIGHT = 1u << 6,
  BK_BUTTON_CAMERA_TRACK = 1u << 7,
  BK_BUTTON_ACTOR_PREVIEW = 1u << 8,
  BK_BUTTON_AUDIO_PREVIEW = 1u << 9,
  BK_BUTTON_GAME_CAMERA = 1u << 10,
  BK_BUTTON_PHOTO = 1u << 11,
  BK_BUTTON_INTERACT = 1u << 12,
  BK_BUTTON_STANCE = 1u << 13,
  BK_BUTTON_SLOW = 1u << 14
};
typedef struct {
  uint32_t held, pressed, released;
  float move_x, move_y, look_x, look_y;
  float pointer_x, pointer_y, pointer_motion_x, pointer_motion_y;
  uint8_t pointer_active;
} BkInput;
/* Accumulate edges until a simulation tick consumes them. */
void bk_input_latch(BkInput *pending, const BkInput *sample);
BkInput bk_input_consume(BkInput *pending);
#endif
