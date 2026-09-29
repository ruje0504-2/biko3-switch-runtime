#include "scene/ending_target.h"
#undef NDEBUG
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
int main(void) {
  static const float identity[16] = {1, 0, 0, 0, 0, 1, 0, 0,
                                      0, 0, 1, 0, 0, 0, 0, 1};
  char error[256] = {0};
  BkEndingFrameState frame = {.phase = 1};
  BkEndingControlState control = {0};
  BkEndingAuxiliaryState auxiliary = {0};
  int32_t clip = 1, actions[80], targets[39][2], kind = 9, column = 10;
  float world[39][16], camera[16], position[3] = {0, 0, 100};
  uint8_t present[39] = {0};
  BkEndingUiSprite ring = {0};
  ring.rect[2] = 100;
  for (unsigned i = 0; i < 80; ++i)
    actions[i] = -1;
  for (unsigned i = 0; i < 39; ++i) {
    memcpy(world[i], identity, sizeof(identity));
    targets[i][0] = 123;
    targets[i][1] = -234;
  }
  memcpy(camera, identity, sizeof(identity));
  BkEndingUiPickBindings geometry = {world, present, 39, position, camera,
                                     camera, camera, 0};
  BkEndingTargetBindings bindings = {&frame, &control, &auxiliary, &clip,
      actions, targets, &kind, &column, &ring, camera, &geometry};
  const float pointer[2] = {0, 0};
  int result = -1;
  /* Absent optional nodes keep previously published coordinates; their
   * unused matrices may be invalid without inventing a target or failing. */
  world[20][12] = NAN;
  assert(bk_ending_target_step(&bindings, pointer, &result, error));
  assert(!result && frame.camera_cached == -1 && kind == -1 && column == -1);
  for (unsigned i = 0; i < 39; ++i)
    assert(targets[i][0] == 123 && targets[i][1] == -234);
  actions[0] = 1;
  actions[5] = 11;
  present[1] = present[11] = 1;
  assert(bk_ending_target_step(&bindings, pointer, &result, error));
  assert(result && frame.camera_cached == 11 && kind == 0 && column == 1);
  assert(ring.transform.scale[0] == 1.f && ring.transform.pivot[0] == .5f);
  world[11][12] = 2;
  assert(bk_ending_target_step(&bindings, pointer, &result, error));
  assert(result && frame.camera_cached == 1 && column == 0);
  /* Reject an incomplete binding table before changing existing state. */
  geometry.count = 38;
  assert(!bk_ending_target_step(&bindings, pointer, &result, error));
  assert(frame.camera_cached == 1 && column == 0);
  geometry.count = 39;
  /* Undefined original variant flag is distinguished from a valid miss. */
  control.variant = 1;
  assert(!bk_ending_target_step(&bindings, pointer, &result, error));
  assert(frame.camera_cached == 1 && kind == -1 && column == -1);
  control.variant = 0;
  /* A zero-distance camera is the original infinite circle/scale, rather
   * than dividing by an arbitrary epsilon or returning a spurious miss. */
  present[11] = 0;
  position[2] = 0;
  assert(bk_ending_target_step(&bindings, pointer, &result, error));
  assert(result && frame.camera_cached == 1 && isinf(ring.transform.scale[0]));
  assert(ring.transform.scale[0] > 0);
  assert(bk_ending_target_eligible(0, 0, 0, .39f, 6));
  assert(!bk_ending_target_eligible(0, 0, 0, nextafterf(.39f, 0), 6));
  assert(!bk_ending_target_eligible(1, 0, 0, 1, 26));
  assert(bk_ending_target_eligible(1, 1, 5, 0, 32));
  puts("PASS ending target: missing nodes, ties, nearer target, binding "
       "rejection, undefined variant, coincident camera, eligibility boundary");
  return 0;
}
