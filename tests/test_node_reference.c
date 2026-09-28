#include "world/actor_pose.h"
#include "world/node_reference.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static const float I[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
int main(void) {
  char e[256];
  float ref[16];
  memcpy(ref, I, 64);
  ref[12] = 13;
  ref[13] = 7;
  BkNodeReference node;
  memcpy(node.local, I, 64);
  memcpy(node.world, I, 64);
  memcpy(node.parent_world, I, 64);
  node.parent_world[12] = 10;
  assert(bk_node_reference_position(&node, ref, (float[]){2, 0, 0}, e));
  assert(node.local[12] == 5 && node.world[12] == 15 && node.world[13] == 7);
  assert(bk_node_reference_orientation(&node, ref, (float[]){0, 0, 1},
                                       (float[]){0, 1, 0}, e));
  assert(node.local[12] == 5);
  BkNodeReference saved = node;
  node.parent_world[0] = 0;
  saved = node;
  assert(!bk_node_reference_orientation(&node, ref, (float[]){0, 0, 1},
                                        (float[]){0, 1, 0}, e));
  assert(!memcmp(&node, &saved, sizeof(node)));
  uint8_t xan[0x5190] = {0};
  strcpy((char *)xan, "fixture.x");
  strcpy((char *)xan + 256, "fixture.x");
  BkClipSet *clips = bk_clip_set_decode(xan, sizeof(xan), e);
  assert(clips);
  BkModelFrame frames[3] = {0};
  for (unsigned i = 0; i < 3; i++) {
    frames[i].id = i + 1;
    frames[i].parent_index = i ? i - 1 : BK_MODEL_NONE;
    memcpy(frames[i].local, I, 64);
    frames[i].local[13] = i;
  }
  BkModel model = {.frames = frames, .frame_count = 3};
  BkActorPose *pose =
      bk_actor_pose_create_loaded(&model, clips, 0, (float[]){10, 0, 0}, 0, e);
  assert(pose);
  assert(bk_actor_pose_parent_world(pose, 1)[12] == 0);
  bk_actor_pose_publish(pose);
  float child[16], parent[16];
  memcpy(child, bk_actor_pose_frame(pose, 2), 64);
  memcpy(parent, bk_actor_pose_parent_world(pose, 1), 64);
  BkClipState before, after;
  assert(bk_actor_pose_state(pose, &before));
  assert(bk_actor_pose_align_reference(pose, 1, ref, e));
  assert(bk_actor_pose_state(pose, &after));
  assert(!memcmp(&before, &after, sizeof(before)));
  assert(bk_actor_pose_local(pose, 1)[12] == 3 &&
         bk_actor_pose_frame(pose, 1)[12] == 13);
  assert(!memcmp(child, bk_actor_pose_frame(pose, 2), 64));
  assert(!memcmp(parent, bk_actor_pose_parent_world(pose, 1), 64));
  bk_actor_pose_publish(pose);
  assert(bk_actor_pose_frame(pose, 2)[12] == 13 &&
         bk_actor_pose_frame(pose, 2)[13] == 9);
  assert(!bk_actor_pose_align_reference(pose, 0, ref, e));
  bk_actor_pose_destroy(pose);
  bk_clip_set_destroy(clips);
  puts("PASS node reference/cache isolation, held descendants, unchanged "
       "scheduler and hierarchy");
}
