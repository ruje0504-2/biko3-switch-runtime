#include "world/actor_forest.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static const float I[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
static void procedural(BkModel *model, BkClipSet *clips) {
  char e[256];
  BkActorPose *poses[2];
  for (unsigned i = 0; i < 2; i++) {
    poses[i] = bk_actor_pose_create_loaded(model, clips, 0,
                                           (float[]){3 + 2 * i, 0, 0}, 0, e);
    assert(poses[i]);
  }
  BkActorForest *f = bk_actor_forest_create(poses, 2, e);
  assert(f && bk_actor_forest_attach(f, 0, 2, e) &&
         bk_actor_forest_attach(f, 0, 4, e));
  assert(bk_actor_forest_attach(f, 4, 3, e));
  uint32_t node = 99, hidden;
  assert(bk_actor_forest_find(f, 2, "target", &node, e) &&
         node == BK_FRAME_NONE);
  assert(bk_actor_forest_find(f, 4, "target", &node, e) && node == 5);
  assert(bk_actor_forest_find(f, 3, "target", &node, e) && node == 3);
  assert(bk_actor_forest_find(f, 2, "", &node, e) && node == 2);
  assert(bk_actor_forest_visibility(f, 2, 7, e));
  assert(bk_actor_pose_hidden(poses[0], 1, &hidden) && hidden == 0);
  assert(bk_actor_forest_visibility(f, 4, 255, e));
  assert(bk_actor_pose_hidden(poses[0], 1, &hidden) && hidden == 255);
  assert(bk_actor_pose_hidden(poses[1], 1, &hidden) && hidden == 255);
  assert(bk_actor_forest_attach(f, 3, 1, e));
  float moved[16], caches[6][16], view[16];
  memcpy(moved, I, 64);
  moved[12] = 20;
  assert(bk_actor_pose_root_local(poses[1], moved, e));
  for (unsigned i = 0; i < 6; i++)
    memcpy(caches[i], bk_actor_forest_world(f, i), 64);
  BkNodeReference before, after;
  assert(bk_actor_forest_anchor_reference(f, 1, &before, e));
  assert(bk_actor_forest_camera_publish(f, e));
  assert(bk_actor_forest_view(f)[12] == -20 &&
         bk_actor_forest_view(f)[13] == -10);
  for (unsigned i = 0; i < 6; i++)
    assert(!memcmp(caches[i], bk_actor_forest_world(f, i), 64));
  assert(bk_actor_forest_anchor_reference(f, 1, &after, e) &&
         !memcmp(&before, &after, sizeof(before)));
  after.parent_world[0] = 2;
  assert(!bk_actor_forest_commit_anchor_reference(f, 1, &after, e));
  after = before;
  after.world[12] = 123;
  after.local[12] = 321;
  assert(bk_actor_forest_commit_anchor_reference(f, 1, &after, e));
  assert(bk_actor_forest_world(f, 1)[12] == 123);
  assert(bk_actor_forest_anchor_reference(f, 1, &before, e) &&
         !memcmp(&before, &after, sizeof(before)));
  memcpy(view, bk_actor_forest_view(f), 64);
  assert(bk_actor_forest_detach(f, 3, 1, e) &&
         bk_actor_forest_camera_publish(f, e));
  assert(!memcmp(view, bk_actor_forest_view(f), 64));
  after.local[0] = NAN;
  assert(!bk_actor_forest_commit_anchor_reference(f, 1, &after, e));
  node = 99;
  assert(!bk_actor_forest_find(f, 6, "", &node, e) && node == 99);
  assert(!bk_actor_forest_visibility(f, 6, 0, e));
  bk_actor_forest_destroy(f);
  for (unsigned i = 0; i < 2; i++)
    bk_actor_pose_destroy(poses[i]);
}
int main(void) {
  char error[256];
  uint8_t xan[0x5190] = {0};
  strcpy((char *)xan, "fixture.x");
  strcpy((char *)xan + 256, "fixture.x");
  BkClipSet *clips = bk_clip_set_decode(xan, sizeof(xan), error);
  assert(clips);
  BkModelFrame frames[2] = {0};
  frames[0].parent_index = BK_MODEL_NONE;
  frames[1].parent_index = 0;
  frames[0].id = 1;
  frames[1].id = 2;
  strcpy(frames[1].name, "export target");
  memcpy(frames[0].local, I, 64);
  memcpy(frames[1].local, I, 64);
  frames[1].local[13] = 10;
  BkModel model = {.frames = frames, .frame_count = 2};
  procedural(&model, clips);
  BkActorPose *poses[2];
  for (unsigned i = 0; i < 2; ++i) {
    poses[i] = bk_actor_pose_create_loaded(
        &model, clips, 0, (float[]){i ? 5 : 3, 0, 0}, 0, error);
    assert(poses[i]);
  }
  BkActorForest *f = bk_actor_forest_create(poses, 2, error);
  assert(f);
  assert(
      !bk_actor_forest_create((BkActorPose *[]){poses[0], poses[0]}, 2, error));
  uint32_t a = bk_actor_forest_node(f, 0, 0), b = bk_actor_forest_node(f, 1, 0),
           n, actor, frame;
  const BkFrameVisit *visits;
  assert(a == 2 && b == 4 && bk_actor_forest_node(f, 2, 0) == BK_FRAME_NONE);
  assert(bk_actor_forest_binding(f, b + 1, &actor, &frame) && actor == 1 &&
         frame == 1);
  assert(!bk_actor_forest_binding(f, 1, &actor, &frame));
  assert(bk_actor_forest_draw(f, 0, &visits, &n, error) && n == 1 &&
         visits[0].node == 1);
  assert(bk_actor_forest_attach(f, 0, a, error));
  assert(bk_actor_forest_attach(f, a + 1, b, error));
  assert(bk_actor_forest_world(f, b)[12] == 8 &&
         bk_actor_forest_world(f, b)[13] == 10);
  assert(!bk_actor_forest_attach(f, b, a, error));
  BkActorVisibilityEdit hide = {0, 1};
  assert(bk_actor_pose_visibility(poses[0], &hide, 1, error));
  float matrix[16];
  memcpy(matrix, I, 64);
  matrix[12] = 20;
  assert(bk_actor_forest_anchor(f, 0, matrix, 0, error));
  assert(bk_actor_forest_draw(f, a, &visits, &n, error) && n == 0);
  assert(bk_actor_forest_world(f, b)[12] == 8 &&
         bk_actor_forest_view(f)[12] == -20);
  assert(bk_actor_forest_refresh(f, error));
  assert(bk_actor_forest_world(f, b)[12] == 28);
  assert(bk_actor_forest_attach(f, 0, b, error));
  assert(bk_actor_forest_world(f, b)[12] == 25 &&
         bk_actor_forest_world(f, b)[13] == 0);
  matrix[12] = 30;
  assert(bk_actor_forest_anchor(f, 0, matrix, 0, error));
  assert(bk_actor_forest_attach(f, 0, b, error));
  assert(bk_actor_forest_world(f, b)[12] == 25);
  assert(bk_actor_forest_draw(f, b, &visits, &n, error));
  assert(bk_actor_forest_world(f, b)[12] == 35);
  size_t world_count = 0;
  const float *published = bk_actor_pose_world(poses[1], &world_count);
  assert(world_count == 32 && published == bk_actor_pose_frame(poses[1], 0));
  assert(!memcmp(published + 16, bk_actor_pose_frame(poses[1], 1), 64));
  assert(!bk_actor_pose_world(NULL, &world_count));
  assert(!bk_actor_pose_world(poses[1], NULL));
  unsigned submitted = 0;
  for (uint32_t i = 0; i < n; ++i)
    if (visits[i].submit)
      ++submitted;
  assert(submitted == 2);
  float saved[16], view[16];
  memcpy(saved, bk_actor_forest_world(f, b), 64);
  memcpy(view, bk_actor_forest_view(f), 64);
  memcpy(matrix, I, 64);
  matrix[0] = 0;
  assert(bk_actor_forest_anchor(f, 1, matrix, 0, error));
  assert(!bk_actor_forest_draw(f, 0, &visits, &n, error));
  assert(!memcmp(saved, bk_actor_forest_world(f, b), 64) &&
         !memcmp(view, bk_actor_forest_view(f), 64));
  assert(bk_actor_forest_anchor(f, 1, I, 0, error));
  assert(bk_actor_forest_detach(f, 0, 1, error));
  assert(bk_actor_forest_draw(f, b, &visits, &n, error));
  assert(!memcmp(view, bk_actor_forest_view(f), 64));
  /* Late registry growth must preserve cross-model parents, detached camera,
   * hidden flags and held world/parent caches without publishing locals. */
  assert(bk_actor_forest_attach(f, a + 1, b, error));
  BkActorPose *late = bk_actor_pose_create_loaded(&model, clips, 0,
                                                  (float[]){7, 0, 0}, 0, error);
  assert(late);
  float before[3][2][3][16];
  BkActorPose *all[] = {poses[0], poses[1], late};
  memcpy(matrix, I, 64);
  matrix[12] = 100;
  assert(bk_actor_pose_root_local(poses[0], matrix, error));
  for (unsigned p = 0; p < 3; p++)
    for (unsigned j = 0; j < 2; j++) {
      memcpy(before[p][j][0], bk_actor_pose_local(all[p], j), 64);
      memcpy(before[p][j][1], bk_actor_pose_frame(all[p], j), 64);
      memcpy(before[p][j][2], bk_actor_pose_parent_world(all[p], j), 64);
    }
  uint32_t index = 99;
  const BkFrameTree *old_tree = bk_actor_forest_tree(f);
  assert(!bk_actor_forest_append(f, poses[0], &index, error));
  assert(index == 99 && old_tree == bk_actor_forest_tree(f));
  assert(!bk_actor_forest_append(f, late, NULL, error));
  assert(bk_actor_forest_append(f, late, &index, error) && index == 2);
  uint32_t c = bk_actor_forest_node(f, 2, 0);
  assert(c == 6 && bk_frame_tree_count(bk_actor_forest_tree(f)) == 8);
  assert(bk_frame_tree_parent(bk_actor_forest_tree(f), b) == a + 1 &&
         bk_frame_tree_parent(bk_actor_forest_tree(f), 1) == BK_FRAME_NONE &&
         bk_frame_tree_parent(bk_actor_forest_tree(f), c) == BK_FRAME_NONE &&
         bk_frame_tree_parent(bk_actor_forest_tree(f), c + 1) == c);
  assert(!memcmp(view, bk_actor_forest_view(f), 64));
  for (unsigned p = 0; p < 3; p++)
    for (unsigned j = 0; j < 2; j++) {
      assert(!memcmp(before[p][j][0], bk_actor_pose_local(all[p], j), 64));
      assert(!memcmp(before[p][j][1], bk_actor_pose_frame(all[p], j), 64));
      assert(
          !memcmp(before[p][j][2], bk_actor_pose_parent_world(all[p], j), 64));
    }
  assert(bk_actor_forest_attach(f, 0, c, error));
  assert(bk_actor_forest_world(f, c)[12] == 37 &&
         bk_actor_forest_world(f, b)[12] == 135);
  assert(bk_actor_forest_draw(f, a, &visits, &n, error) && n == 0);
  assert(!memcmp(view, bk_actor_forest_view(f), 64));
  assert(bk_actor_forest_attach(f, 0, b, error));
  memcpy(matrix, I, 64);
  matrix[0] = 1e30f;
  assert(bk_actor_pose_root_local(poses[0], matrix, error));
  assert(bk_actor_pose_root_local(poses[1], matrix, error));
  memcpy(saved, bk_actor_forest_world(f, b), 64);
  assert(!bk_actor_forest_attach(f, a, b, error));
  assert(bk_frame_tree_parent(bk_actor_forest_tree(f), b) == 0 &&
         !memcmp(saved, bk_actor_forest_world(f, b), 64));
  bk_actor_forest_destroy(f);
  bk_actor_pose_destroy(late);
  for (unsigned i = 0; i < 2; ++i)
    bk_actor_pose_destroy(poses[i]);
  bk_clip_set_destroy(clips);
  puts("PASS bound actor forest, detached roots, cross-model refresh, hidden "
       "draws, held/missing camera, invalid composition atomicity");
}
