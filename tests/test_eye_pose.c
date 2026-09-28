#include "core/matrix.h"
#include "world/actor_pose.h"
#include "world/eye_pose.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static void word(uint8_t *p, uint32_t n) {
  for (unsigned i = 0; i < 4; i++)
    p[i] = (uint8_t)(n >> (i * 8));
}
static void number(uint8_t *p, float x) {
  uint32_t n;
  memcpy(&n, &x, 4);
  word(p, n);
}
static void identity(float m[16]) {
  memset(m, 0, 64);
  m[0] = m[5] = m[10] = m[15] = 1;
}
int main(void) {
  char error[256];
  BkEyePoseFrame e[2];
  for (unsigned i = 0; i < 2; i++) {
    identity(e[i].local);
    identity(e[i].world);
    identity(e[i].parent_world);
  }
  float target[16];
  identity(target);
  target[12] = 40;
  target[13] = 20;
  target[14] = 100;
  assert(bk_eye_pose_aim(e, target, 0, 0, .2f, .3f, error));
  assert(e[0].local[0] < 1 && e[0].local[0] > .9f);
  BkEyePoseFrame saved[2];
  memcpy(saved, e, sizeof(e));
  const float bad[] = {NAN, INFINITY, -1};
  for (unsigned i = 0; i < 3; i++) {
    assert(!bk_eye_pose_aim(e, target, 0, 0, bad[i], .3f, error));
    assert(!memcmp(saved, e, sizeof(e)));
  }
  e[1].parent_world[0] = 0;
  memcpy(saved, e, sizeof(e));
  assert(!bk_eye_pose_aim(e, target, 0, 0, .2f, .3f, error));
  assert(!memcmp(saved, e, sizeof(e)));
  /* Non-affine inverse supports input/output aliasing and rejects atomically.
   */
  float matrix[16], inverse[16], product[16];
  identity(matrix);
  matrix[3] = .2f;
  matrix[12] = 2;
  matrix[15] = .8f;
  memcpy(inverse, matrix, 64);
  assert(bk_matrix_inverse(inverse, inverse));
  bk_matrix_multiply(product, matrix, inverse);
  for (unsigned i = 0; i < 16; i++)
    assert(fabsf(product[i] - (i % 5 == 0)) < 1e-6f);
  float snapshot[16];
  memcpy(snapshot, inverse, 64);
  memset(matrix, 0, 64);
  assert(!bk_matrix_inverse(inverse, matrix));
  assert(!memcmp(inverse, snapshot, 64));
  uint8_t anim[72 + 24 + 440] = {0}, xan[0x5190] = {0};
  BkModelFrame frames[5] = {0};
  for (unsigned i = 0; i < 5; i++) {
    frames[i].id = 100 + i;
    frames[i].parent_index = i == 0 ? BK_MODEL_NONE : i < 2 ? 0 : i < 4 ? 1 : 2;
    identity(frames[i].local);
  }
  strcpy(frames[1].name, "head");
  frames[1].local[13] = 10;
  frames[1].local[14] = 20;
  frames[2].local[12] = -.5f;
  frames[3].local[12] = .5f;
  frames[4].local[14] = 1;
  BkModelChunk chunk = {"ANIM", 0, sizeof(anim)};
  BkModel model = {.source = anim,
                   .source_size = sizeof(anim),
                   .chunks = &chunk,
                   .chunk_count = 1,
                   .frames = frames,
                   .frame_count = 5};
  word(anim + 68, 1);
  word(anim + 72, 101);
  word(anim + 92, 2);
  for (unsigned i = 0; i < 2; i++) {
    uint8_t *k = anim + 96 + i * 220;
    number(k, 20.0f * i);
    word(k + 4, 1);
    word(k + 20, 1);
    word(k + 36, 1);
    number(k + 8, 20.0f * i);
    number(k + 12, 10);
    number(k + 16, 20);
    number(k + 40, 1);
    number(k + 44, 1);
    number(k + 48, 1);
    number(k + 128, 1);
  }
  memcpy(xan, "test.x", 7);
  memcpy(xan + 256, "test.x", 7);
  word(xan + 512 + 0x190 + 0x50, 20);
  number(xan + 512 + 0x190 + 0x54, 1);
  number(xan + 512 + 0x190 + 0x58, 20);
  BkClipSet *clips = bk_clip_set_decode(xan, sizeof(xan), error);
  assert(clips);
  float position[3] = {30, 0, 0};
  BkActorPose *a =
      bk_actor_pose_create(&model, clips, 0, "head", position, 0, 0, 1, error);
  assert(a);
  uint32_t indices[2] = {2, 3};
  float head[16], child[16], parent[16];
  memcpy(head, bk_actor_pose_frame(a, 1), 64);
  memcpy(child, bk_actor_pose_frame(a, 4), 64);
  memcpy(parent, bk_actor_pose_parent_world(a, 2), 64);
  assert(parent[12] == 0 && bk_actor_pose_frame(a, 0)[12] == 30);
  assert(bk_actor_pose_eyes(a, indices, target, 0, 0, .2f, .3f, error));
  assert(!memcmp(head, bk_actor_pose_frame(a, 1), 64));
  assert(!memcmp(child, bk_actor_pose_frame(a, 4), 64));
  assert(!memcmp(parent, bk_actor_pose_parent_world(a, 2), 64));
  float local[2][16];
  for (unsigned i = 0; i < 2; i++)
    memcpy(local[i], bk_actor_pose_local(a, indices[i]), 64);
  bk_actor_pose_publish(a);
  assert(bk_actor_pose_parent_world(a, 2)[12] == 30);
  assert(memcmp(child, bk_actor_pose_frame(a, 4), 64));
  assert(bk_actor_pose_step(a, position, 0, -1, .1f, error));
  assert(bk_actor_pose_step(a, position, 0, -1, .1f, error));
  for (unsigned i = 0; i < 2; i++)
    assert(!memcmp(local[i], bk_actor_pose_local(a, indices[i]), 64));
  position[0] = 80;
  assert(bk_actor_pose_place(a, position, 0, error));
  for (unsigned i = 0; i < 2; i++)
    assert(!memcmp(local[i], bk_actor_pose_local(a, indices[i]), 64));
  memcpy(snapshot, bk_actor_pose_frame(a, 2), 64);
  target[15] = NAN;
  assert(!bk_actor_pose_eyes(a, indices, target, 0, 0, .2f, .3f, error));
  assert(!memcmp(snapshot, bk_actor_pose_frame(a, 2), 64));
  for (unsigned i = 0; i < 2; i++)
    assert(!memcmp(local[i], bk_actor_pose_local(a, indices[i]), 64));
  indices[0] = BK_MODEL_NONE;
  assert(bk_actor_pose_eyes(a, indices, NULL, 0, 99, NAN, NAN, error));
  BkActorVisibilityEdit hide = {0, 1};
  assert(bk_actor_pose_visibility(a, &hide, 1, error));
  memcpy(parent, bk_actor_pose_parent_world(a, 2), 64);
  bk_actor_pose_publish(a);
  assert(!memcmp(parent, bk_actor_pose_parent_world(a, 2), 64));
  bk_actor_pose_destroy(a);
  bk_clip_set_destroy(clips);
  puts("PASS gaze atomic errors, original cache phases, persistent untracked "
       "locals and 4x4 inverse");
  return 0;
}
