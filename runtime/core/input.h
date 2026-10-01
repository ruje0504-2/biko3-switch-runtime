#ifndef BK_INPUT_H
#define BK_INPUT_H
#include "core/camera.h"
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
  BK_BUTTON_SLOW = 1u << 14,
  BK_BUTTON_CAMERA_ORBIT = 1u << 15,
  BK_BUTTON_CAMERA_ADJUST = 1u << 16,
  BK_BUTTON_FPS = 1u << 17
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
/* Viewport-local software pointer. Absolute input is in target pixels;
 * relative motion is in pixels. Left stick and d-pad move the cursor, with
 * positive stick Y pointing up; right stick remains available to the camera.
 * First touch positions without a synthetic drag. Position and applied motion
 * are clamped to the same content viewport used for drawing/hit testing.
 * Invalid input leaves the whole state unchanged. No platform/GPU calls. */
typedef struct {
  float position[2], motion[2];
  uint8_t absolute_active;
} BkVirtualPointer;
int bk_virtual_pointer_step(BkVirtualPointer *, const BkViewport *,
                             const BkInput *, double seconds, char error[256]);
#endif
