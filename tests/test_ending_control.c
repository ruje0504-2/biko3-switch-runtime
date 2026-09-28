#include "game/ending_control.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
typedef struct {
  unsigned calls, fail_at, sounds[3], keys, warps, auxiliary;
  uint32_t key;
  BkEndingFrameState *frame;
} Fixture;
static int tick(Fixture *f) { return ++f->calls != f->fail_at; }
static int key(void *p, unsigned code, unsigned mode, uint32_t *result,
               char e[256]) {
  (void)e;
  Fixture *f = p;
  assert((code == 0 || code == 0x5a || code == 0x33450) &&
         (mode == 1 || mode == 2));
  ++f->keys;
  if (!tick(f))
    return 0;
  *result = f->key;
  return 1;
}
static int sound(void *p, unsigned slot, int32_t volume, char e[256]) {
  (void)e;
  Fixture *f = p;
  assert(volume == -700 && (slot == 0 || slot == 3 || slot == 5));
  if (!tick(f))
    return 0;
  ++f->sounds[slot == 0 ? 0 : slot == 3 ? 1 : 2];
  return 1;
}
static int auxiliary(void *p, int32_t proposed, int32_t *result, char e[256]) {
  (void)e;
  Fixture *f = p;
  assert(proposed == 1);
  if (!tick(f))
    return 0;
  ++f->auxiliary;
  f->frame->auxiliary_mode = 2;
  *result = 256; /* Full EAX, not AL; reread live mode after call. */
  return 1;
}
static int warp(void *p, float x, float y, char e[256]) {
  (void)e;
  Fixture *f = p;
  assert(x == 400 && y == 260);
  if (!tick(f))
    return 0;
  ++f->warps;
  return 1;
}
int main(void) {
  char e[256];
  BkEndingControlState s = {.variant = 1};
  BkEndingFrameState frame = {.phase = 5};
  BkMenuCamera camera = {.yaw = 17, .pitch = 18, .radius = 19, .height = 20};
  camera.matrix[12] = 91;
  BkEndingCameraPresets presets = {
      .active = {{0, 90, 180}, {0, 20, 40}, {5, 10, 15}, {1, 2, 3}}};
  presets.authored[0][0] = -1;
  const float main_node[3] = {1, 2, 3}, second[3] = {3, 4, 5};
  BkEndingControlRect rects[BK_ENDING_CONTROL_RECTS];
  for (unsigned i = 0; i < BK_ENDING_CONTROL_RECTS; ++i)
    rects[i] = (BkEndingControlRect){100, 100, 10, 10};
  rects[0] = (BkEndingControlRect){0, 0, 10, 10};
  float scale = .5f;
  int32_t volume = -700;
  BkEndingControlBindings b = {
      &frame, &camera, &presets, {main_node, second, NULL, NULL},
      rects,  &scale,  &volume};
  BkEndingFrameInput in = {0};
  in.words[9] = in.words[10] = 10; /* Inclusive upper corner. */
  Fixture f = {.key = 1, .frame = &frame};
  BkEndingControlOps ops = {&f, key, sound, auxiliary, warp};
  assert(bk_ending_control_step(&s, &b, &in, &ops, e));
  assert(frame.camera_clip == 1 && frame.camera_mode == 2 && s.hover == 12);
  assert(camera.yaw == 90 && camera.pitch == 20 && presets.active[0][0] == 17);
  assert(s.saved_camera[12] == 91 && s.targets[1][0] == 2);
  assert(f.sounds[0] == 1 && f.sounds[1] == 1 && f.keys == 1);
  f.key = 256;
  f.keys = 0;
  assert(bk_ending_control_step(&s, &b, &in, &ops, e));
  assert(f.keys == 6 && f.sounds[1] == 1 && frame.camera_clip == 1);
  rects[0].x = 100;
  rects[2] = (BkEndingControlRect){0, 0, 10, 10};
  f.key = 1;
  assert(bk_ending_control_step(&s, &b, &in, &ops, e));
  assert(f.auxiliary == 1 && frame.auxiliary_mode == 3);
  rects[2].x = 100;
  rects[11] = rects[12] = (BkEndingControlRect){0, 0, 10, 10};
  assert(bk_ending_control_step(&s, &b, &in, &ops, e));
  assert(f.warps == 2 && frame.phase == 9 && s.previous_phase == 5 &&
         s.pause_selection == 47);
  for (unsigned i = 0; i < 6; ++i)
    assert(s.pause_flags[i] == 1);
  /* Source pointer remains captured through both warps; action11 writes
   * state before sound, action12 writes it after sound. */
  frame.phase = 6;
  f.calls = 0;
  f.fail_at = 2;
  assert(!bk_ending_control_step(&s, &b, &in, &ops, e));
  assert(frame.phase == 9 && s.previous_phase == 6 && s.pause_selection == 45);
  rects[11].x = 100;
  frame.phase = 6;
  f.calls = 0;
  assert(!bk_ending_control_step(&s, &b, &in, &ops, e));
  assert(frame.phase == 6 && s.pause_selection == 45);
  f.fail_at = 0;
  rects[12].x = 100;
  rects[0].x = 0;
  b.nodes[0] = NULL;
  frame.camera_mode = 0;
  assert(!bk_ending_control_step(&s, &b, &in, &ops, e));
  assert(frame.camera_mode == 2); /* Required node failed after real prefix. */
  BkEndingControlState old = s;
  rects[0].x = NAN;
  assert(!bk_ending_control_step(&s, &b, &in, &ops, e));
  assert(!memcmp(&old, &s, sizeof(s)));
  puts("ending control bounds/retention/overlap/auxiliary/failure checks "
       "passed");
  return 0;
}
