#include "model/playback.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void word(uint8_t *p, uint32_t x) {
  for (unsigned i = 0; i < 4; i++)
    p[i] = (uint8_t)(x >> (i * 8));
}
static void number(uint8_t *p, float x) {
  uint32_t n;
  memcpy(&n, &x, 4);
  word(p, n);
}
static void identity(float *m) {
  memset(m, 0, 64);
  m[0] = m[5] = m[10] = m[15] = 1;
}
int main(void) {
  uint8_t bytes[72 + 24 + 3 * 220] = {0}, original[sizeof(bytes)];
  BkModelChunk chunk = {"ANIM", 0, sizeof(bytes)};
  BkModelFrame frames[2] = {0};
  frames[0].id = 101;
  frames[0].parent_index = 1;
  identity(frames[0].local);
  frames[0].local[12] = 3;
  frames[1].id = 100;
  frames[1].parent_index = BK_MODEL_NONE;
  identity(frames[1].local);
  frames[1].local[12] = 2;
  BkModel model = {.source = bytes,
                   .source_size = sizeof(bytes),
                   .chunks = &chunk,
                   .chunk_count = 1,
                   .frames = frames,
                   .frame_count = 2};
  word(bytes + 68, 1);
  word(bytes + 72, 100);
  word(bytes + 92, 3);
  for (unsigned i = 0; i < 3; i++) {
    uint8_t *k = bytes + 96 + i * 220;
    number(k, (float)i * 10);
    word(k + 4, 1);
    word(k + 20, 1);
    word(k + 36, 1);
    number(k + 8, (float)i * 10);
    number(k + 40, 1);
    number(k + 44, 1);
    number(k + 48, 1);
    number(k + 128, 1);
  }
  memcpy(original, bytes, sizeof(bytes));
  char error[256];
  float pose[32], saved[32];
  BkModelAnimation *a = bk_model_animation_create(&model, error);
  assert(a);
  assert(bk_model_animation_track_count(a) == 1 &&
         bk_model_animation_duration(a) == 20);
  const float times[] = {0, 5, 20, 20.5f, 21.5f, 40, 41.75f};
  const float expected[] = {0, 5, 20, 0, 1, 0, 1};
  for (unsigned i = 0; i < sizeof(times) / sizeof(*times); i++) {
    assert(bk_model_animation_sample(a, times[i], 1, pose, 32, error));
    assert(pose[28] == expected[i] && pose[12] == expected[i] + 3);
  }
  assert(bk_model_animation_sample(a, 25, 0, pose, 32, error));
  assert(pose[28] == 20);
  assert(bk_model_animation_blend(a, 0, 20, .25f, 0, pose, 32, error));
  assert(pose[28] == 3.125f && pose[12] == 6.125f);
  assert(bk_model_animation_blend(a, 20, 0, .25f, 0, pose, 32, error));
  assert(pose[28] == 16.875f);
  assert(bk_model_animation_blend(a, 0, 20, 2, 0, pose, 32, error));
  assert(pose[28] == 20);
  memcpy(saved, pose, sizeof(pose));
  for (unsigned i = 0; i < 4; i++) {
    const float bad[] = {-1, NAN, INFINITY, 2147483648.0f};
    assert(!bk_model_animation_sample(a, bad[i], 1, pose, 32, error));
    assert(!memcmp(saved, pose, sizeof(pose)));
  }
  assert(!bk_model_animation_sample(a, 1, 2, pose, 32, error));
  assert(!bk_model_animation_sample(a, 1, 1, pose, 31, error));
  assert(!memcmp(saved, pose, sizeof(pose)));
  assert(!bk_model_animation_blend(a, 0, 20, NAN, 0, pose, 32, error));
  assert(!bk_model_animation_blend(a, 0, -1, .5f, 0, pose, 32, error));
  assert(!memcmp(saved, pose, sizeof(pose)));
  assert(!memcmp(bytes, original, sizeof(bytes)));
  assert(frames[1].local[12] == 2);
  bk_model_animation_destroy(a);
  /* An external root replaces its base matrix before animated descendants
   * are combined. File order is deliberately child-before-parent. */
  word(bytes + 72, 101);
  a = bk_model_animation_create(&model, error);
  assert(a);
  BkModelRootTransform root = {.frame = 1};
  identity(root.world);
  root.world[12] = 100;
  BkModelPoseSample request = {5, 20, .25f, 0, 0};
  assert(bk_model_animation_pose(a, &request, &root, pose, 32, error));
  assert(pose[12] == 105 && pose[28] == 100);
  request.blend = 1;
  assert(bk_model_animation_pose(a, &request, &root, pose, 32, error));
  assert(pose[12] == 107.34375f);
  memcpy(saved, pose, sizeof(pose));
  root.frame = 0;
  assert(!bk_model_animation_pose(a, &request, &root, pose, 32, error));
  root.frame = 1;
  root.world[12] = NAN;
  assert(!bk_model_animation_pose(a, &request, &root, pose, 32, error));
  root.world[12] = 100;
  root.world[15] = 0;
  assert(!bk_model_animation_pose(a, &request, &root, pose, 32, error));
  assert(!memcmp(saved, pose, sizeof(pose)));
  bk_model_animation_destroy(a);
  /* Missing channel values are filled from the authored neighbors, with a
   * held tail; raw unflagged values must not leak into the pose. */
  memcpy(bytes, original, sizeof(bytes));
  word(bytes + 96 + 220 + 4, 0);
  number(bytes + 96 + 220 + 8, 900);
  word(bytes + 96 + 440 + 36, 0);
  number(bytes + 96 + 440 + 40, 800);
  a = bk_model_animation_create(&model, error);
  assert(a && bk_model_animation_sample(a, 10, 0, pose, 32, error));
  assert(pose[28] == 10);
  assert(bk_model_animation_sample(a, 20, 0, pose, 32, error));
  assert(pose[16] == 1);
  bk_model_animation_destroy(a);
  strcpy(frames[0].name, "export head");
  strcpy(frames[1].name, "root");
  uint32_t node = 99;
  assert(bk_model_find_frame(&model, "head", &node, error) && node == 0);
  assert(!bk_model_find_frame(&model, "Head", &node, error) && node == 0);
  strcpy(frames[1].name, "head");
  assert(!bk_model_find_frame(&model, "head", &node, error) && node == 0);
  memcpy(bytes, original, sizeof(bytes));
  /* Combined playback commits timeline and pose together. Instances sharing
   * immutable assets retain independent roots, time and published matrices. */
  word(bytes + 72, 101);
  uint8_t xan[0x5190] = {0};
  memcpy(xan, "test.x", 7);
  memcpy(xan + 256, "test.x", 7);
  word(xan + 512 + 0x190 + 0x50, 20);
  number(xan + 512 + 0x190 + 0x54, 1);
  number(xan + 512 + 0x190 + 0x58, 20);
  BkClipSet *clips = bk_clip_set_decode(xan, sizeof(xan), error);
  assert(clips);
  BkModelPlayback *first = bk_model_playback_create(&model, clips, 0, error);
  BkModelPlayback *second = bk_model_playback_create(&model, clips, 0, error);
  assert(first && second);
  assert(!bk_model_playback_advance(first, 0, NULL, error));
  assert(bk_model_playback_select(first, 0, 1, error));
  identity(root.world);
  root.world[12] = 100;
  root.frame = 1;
  assert(bk_model_playback_advance(first, 0, &root, error));
  const float *world = bk_model_playback_frame(first, 0);
  assert(world && world[12] > 100 && world[28] == 100);
  memcpy(saved, world, sizeof(saved));
  BkClipState before, after;
  assert(bk_model_playback_state(first, &before));
  root.frame = 0;
  assert(!bk_model_playback_advance(first, .5f, &root, error));
  assert(bk_model_playback_state(first, &after));
  assert(!memcmp(&before, &after, sizeof(before)));
  assert(!memcmp(saved, bk_model_playback_frame(first, 0), sizeof(saved)));
  assert(bk_model_playback_frame(second, 0)[12] == 5);
  assert(!bk_model_playback_frame(first, 2));
  root.frame = 1;
  assert(!bk_model_playback_advance(first, NAN, &root, error));
  assert(bk_model_playback_advance(first, .5f, &root, error));
  assert(bk_model_playback_state(first, &before));
  memcpy(saved, bk_model_playback_frame(first, 0), sizeof(saved));
  root.world[12] = 200;
  assert(bk_model_playback_place(first, &root, error));
  assert(fabsf(bk_model_playback_frame(first, 0)[12] - saved[12] - 100) <
         1e-5f);
  assert(bk_model_playback_state(first, &after) &&
         !memcmp(&before, &after, sizeof(before)));
  root.frame = 0;
  memcpy(saved, bk_model_playback_frame(first, 0), sizeof(saved));
  assert(!bk_model_playback_place(first, &root, error));
  assert(!memcmp(saved, bk_model_playback_frame(first, 0), sizeof(saved)));
  root.frame = 1;
  assert(bk_model_playback_request(second, 0, error));
  assert(bk_model_playback_state(second, &before));
  assert(bk_model_playback_place(second, &root, error));
  assert(bk_model_playback_frame(second, 0)[12] == 203);
  assert(bk_model_playback_place(second, NULL, error));
  assert(bk_model_playback_frame(second, 0)[12] == 5);
  assert(bk_model_playback_state(second, &after) &&
         !memcmp(&before, &after, sizeof(before)));
  assert(bk_model_playback_request(second, 0, error));
  assert(bk_model_playback_advance(second, 0, NULL, error));
  assert(bk_model_playback_frame(second, 1)[12] == 2);
  bk_model_playback_destroy(first);
  bk_model_playback_destroy(second);
  /* Animation-group plain-time cache survives blend submissions. A fresh
   * group's cached zero also leaves authored base locals intact. */
  bk_clip_set_destroy(clips);
  memset(xan + 512, 0, sizeof(xan) - 512);
  for (unsigned i = 0; i < 3; i++) {
    uint8_t *def = xan + 512 + 0x190 + i * 156;
    word(def + 0x50, 20);
    number(def + 0x54, 10.0f * i);
    number(def + 0x58, 20.0f + (i == 2 ? 10 : 0));
    number(def + 0x7c, i == 2 ? 5 : 0);
  }
  clips = bk_clip_set_decode(xan, sizeof(xan), error);
  assert(clips);
  first = bk_model_playback_create(&model, clips, 0, error);
  assert(first);
  memcpy(saved, bk_model_playback_frame(first, 0), sizeof(saved));
  assert(bk_model_playback_select(first, 0, 0, error));
  assert(bk_model_playback_advance(first, 0, NULL, error));
  assert(!memcmp(saved, bk_model_playback_frame(first, 0), sizeof(saved)));
  assert(bk_model_playback_select(first, 1, 0, error));
  assert(bk_model_playback_advance(first, 0, NULL, error));
  assert(memcmp(saved, bk_model_playback_frame(first, 0), sizeof(saved)));
  assert(bk_model_playback_select(first, 2, 0, error));
  BkPlaybackEffects effects;
  assert(bk_model_playback_step_effects(first, -1, BK_CLIP_REQUEST_TEN_TICKS, 0,
                                        NULL, &effects, error));
  assert(effects.pose.blend && effects.pose.from == 10 &&
         effects.pose.to == 25 && effects.source == 20);
  BkPlaybackEffects held_effects = effects;
  assert(bk_model_playback_state(first, &before));
  assert(!bk_model_playback_step_effects(first, -1, BK_CLIP_REQUEST_TEN_TICKS,
                                         NAN, NULL, &effects, error));
  assert(!memcmp(&effects, &held_effects, sizeof(effects)));
  assert(bk_model_playback_state(first, &after) &&
         !memcmp(&before, &after, sizeof(before)));
  assert(bk_model_playback_advance(first, 1.0f / 6, NULL, error));
  memcpy(saved, bk_model_playback_frame(first, 0), sizeof(saved));
  assert(bk_model_playback_select(first, 1, 0, error));
  assert(bk_model_playback_advance(first, 0, NULL, error));
  assert(bk_model_playback_state(first, &before) && before.source == 10);
  assert(!memcmp(saved, bk_model_playback_frame(first, 0), sizeof(saved)));
  assert(!bk_model_playback_step_mode(first, 0, (BkClipRequestMode)2, 0, NULL,
                                      error));
  assert(bk_model_playback_state(first, &after) &&
         !memcmp(&before, &after, sizeof(before)));
  assert(!memcmp(saved, bk_model_playback_frame(first, 0), sizeof(saved)));
  /* A procedural edit on an animated node survives an unchanged plain
   * sample, placement and hidden-style hold. A new sample overwrites it. */
  BkModelLocalEdit edit = {.frame = 0};
  memcpy(edit.local, bk_model_playback_local(first, 0), 64);
  edit.local[12] = 789;
  assert(bk_model_playback_edit_locals(first, &edit, 1, error));
  assert(bk_model_playback_advance(first, 0, NULL, error));
  assert(bk_model_playback_local(first, 0)[12] == 789);
  assert(bk_model_playback_place(first, NULL, error));
  assert(bk_model_playback_local(first, 0)[12] == 789);
  assert(bk_model_playback_hold_mode(first, -1, BK_CLIP_REQUEST_TEN_TICKS, NULL,
                                     error));
  assert(bk_model_playback_local(first, 0)[12] == 789);
  BkModelLocalEdit bad[2] = {edit, edit};
  bad[1].local[15] = NAN;
  memcpy(saved, bk_model_playback_frame(first, 0), sizeof(saved));
  assert(!bk_model_playback_edit_locals(first, bad, 2, error));
  assert(!memcmp(saved, bk_model_playback_frame(first, 0), sizeof(saved)));
  bad[1] = edit;
  bad[1].frame = 1;
  assert(!bk_model_playback_edit_locals(first, bad, 2, error));
  assert(!memcmp(saved, bk_model_playback_frame(first, 0), sizeof(saved)));
  assert(bk_model_playback_advance(first, .01f, NULL, error));
  assert(bk_model_playback_local(first, 0)[12] != 789);
  bk_model_playback_destroy(first);
  bk_clip_set_destroy(clips);
  /* No ANIM chunk is a real static model; a present malformed chunk is not.
   * Root placement still composes the authored child hierarchy. */
  model.chunk_count = 0;
  a = bk_model_animation_create(&model, error);
  assert(a && !bk_model_animation_track_count(a) &&
         bk_model_animation_duration(a) == 0);
  assert(bk_model_animation_sample(a, 123, 1, pose, 32, error));
  assert(pose[12] == 5 && pose[28] == 2);
  root.frame = 1;
  identity(root.world);
  root.world[12] = 100;
  assert(bk_model_animation_pose(a, &request, &root, pose, 32, error));
  assert(pose[12] == 103 && pose[28] == 100);
  bk_model_animation_destroy(a);
  model.chunk_count = 1;
  memcpy(bytes, original, sizeof(bytes));
  /* Rejected byte layouts never become an apparently valid empty animation. */
  for (uint32_t n = 0; n < sizeof(bytes); n++) {
    chunk.size = n;
    a = bk_model_animation_create(&model, error);
    assert(!a);
  }
  chunk.size = sizeof(bytes);
  const unsigned bad_offsets[] = {68, 72, 76, 92, 100, 116, 132, 256};
  for (unsigned i = 0; i < sizeof(bad_offsets) / sizeof(*bad_offsets); i++) {
    memcpy(bytes, original, sizeof(bytes));
    word(bytes + bad_offsets[i], 0xffffffff);
    a = bk_model_animation_create(&model, error);
    assert(!a);
  }
  /* Sanitizer target reaches parser and sample with valid and malformed keys.
   */
  uint32_t state = 0x262709;
  for (unsigned n = 0; n < 6000; n++) {
    memcpy(bytes, original, sizeof(bytes));
    for (unsigned j = 0; j < 1 + n % 9; j++) {
      state ^= state << 13;
      state ^= state >> 17;
      state ^= state << 5;
      bytes[state % sizeof(bytes)] ^= (uint8_t)(state >> 24);
    }
    a = bk_model_animation_create(&model, error);
    if (a) {
      bk_model_animation_sample(a, 5.25f, 1, pose, 32, error);
      bk_model_animation_destroy(a);
    }
  }
  puts("PASS: animation hierarchy, loop boundary, clamp, immutable source, "
       "invalid time/layout, 6000 mutations");
  return 0;
}
