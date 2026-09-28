#include "world/face_controller.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
int main(void) {
  char error[256];
  BkFaceState face;
  assert(bk_face_init(&face, 8, 8, error));
  assert(face.eye_max == 9 && face.mouth_max == 9 &&
         face.transition_rate == .0005f && face.dirty == 1);
  assert(bk_face_request(&face, 2, 0xfffffff0, error));
  BkFaceCommands commands;
  assert(bk_face_mouth(&face, 4, 0, &commands, error));
  assert(commands.count == 16 && commands.commands[0].blend == 1);
  assert(commands.commands[0].group == 0 && commands.commands[7].index == 7);
  assert(commands.commands[8].group == 1 && commands.commands[15].index == 7);
  assert(commands.commands[8].to == 24 && commands.commands[8].from == 0);
  assert(commands.commands[0].weight == (float)(16.0 * .0005f));
  uint32_t random = 1;
  face.eye_mode = 2;
  /* Mouth submits the final blend; blink can skip it before sampling when
   * the raw factor is already >1. The two paths intentionally differ. */
  assert(bk_face_mouth(&face, 4, 3000, &commands, error));
  assert(commands.count == 16 && commands.commands[0].weight == 1);
  face.transition = 1;
  assert(bk_face_blink(&face, 3000, 3000, &random, &commands, error));
  assert(commands.count == 0 && face.transition == 0);
  face.eye_mode = 0;
  face.blink_phase = 1;
  face.blink_duration_ms = 0;
  BkFaceState held = face;
  BkFaceCommands saved = commands;
  assert(!bk_face_blink(&face, 4000, 4000, &random, &commands, error));
  assert(!memcmp(&face, &held, sizeof(face)) && random == 1);
  assert(!memcmp(&commands, &saved, sizeof(saved)));
  assert(!bk_face_mouth(&face, NAN, 0, &commands, error));
  assert(!bk_face_init(&face, 9, 0, error));
  assert(!bk_face_eye_range(&face, 0, NAN, &random, error));
  assert(!bk_face_mouth_range(&face, INFINITY, 9, error));
  assert(!bk_face_transition_seconds(&face, NAN, error));
  assert(!memcmp(&face, &held, sizeof(face)) && random == 1);
  assert(bk_face_init(&face, 0, 0, error));
  face.eye = 3;
  assert(bk_face_request(&face, 1, 100, error) && face.eye == 3);
  assert(bk_face_transition_seconds(&face, .009f, error));
  assert(face.transition == 0 && face.transition_rate == 0);
  assert(bk_face_eye_range(&face, 0, 2, &random, error));
  assert(face.blink_phase == 1 && random != 1 && face.eye == 3);
  face.mouth = 8;
  assert(bk_face_mouth_range(&face, 0, 2, error) && face.mouth == 2);
  assert(bk_face_blink(&face, 1000, 1000, &random, &commands, error));
  assert(commands.count == 0);
  puts("PASS: face command bounds/order, clock wrap, blend completion, range setters and atomic rejection");
  return 0;
}
