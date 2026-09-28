#include "world/actor_pose.h"
#include "world/bom_motion.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static const float I[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
static BkNodeReference node(void) {
  BkNodeReference n;
  memcpy(n.local, I, 64);
  memcpy(n.world, I, 64);
  memcpy(n.parent_world, I, 64);
  return n;
}
int main(void) {
  char e[256];
  BkNodeReference n = node();
  BkBomManual m = {0};
  assert(bk_bom_manual_step(&m, BK_BOM_MANUAL_FIRST, &n, I, 1, 0, .09f, 7.5f, 1,
                            e));
  /* Original normalizes even the smallest nonzero ordinary input to radius. */
  assert(m.offset[0] == .09f && m.offset[1] == 0 && n.world[12] == .09f);
  m = (BkBomManual){0};
  n = node();
  assert(bk_bom_manual_step(&m, BK_BOM_MANUAL_FIRST, &n, I, 1, 0, .000001f,
                            7.5f, 1, e));
  assert(m.offset[0] == 0 && m.offset[1] == 0);
  BkBomManual held_m = m;
  BkNodeReference held_n = n;
  n.parent_world[0] = 0;
  held_n = n;
  assert(!bk_bom_manual_step(&m, BK_BOM_MANUAL_FIRST, &n, I, 3, 7, .09f, 7.5f,
                             1, e));
  assert(!memcmp(&m, &held_m, sizeof(m)) && !memcmp(&n, &held_n, sizeof(n)));
  assert(bk_bom_manual_step(&m, BK_BOM_MANUAL_FIRST, NULL, I, 3, 7, NAN, NAN, 0,
                            e));
  BkBomOscillator osc = {.fresh = 1};
  float offset = 17;
  int done = -1;
  assert(bk_bom_oscillator_step(&osc, 0, .001f, 0, &offset, &done, e));
  assert(done == 1 && offset == 17 && osc.fresh == 1 && osc.bound == 0);
  BkBomReturn s;
  bk_bom_return_init(&s);
  s.axes[0] = (BkBomOscillator){.fresh = 0,
                                .direction = 1,
                                .amplitude = .5f,
                                .travel = .2f,
                                .bound = .9f,
                                .start = .5f};
  s.offset[0][0] = .3f;
  s.complete[0][0] = s.complete[0][1] = 1;
  BkBomReturn held_s = s;
  n = node();
  n.local[12] = n.world[12] = 3;
  assert(bk_bom_return_single(&s, &n, I, I, .09f, 1, 16, 1, &done, e));
  assert(done == 1 && n.world[12] == 0 && !s.complete[0][0] &&
         !s.complete[0][1]);
  assert(!memcmp(s.axes, held_s.axes, sizeof(s.axes)) &&
         !memcmp(s.offset, held_s.offset, sizeof(s.offset)));
  /* Missing second node preserves first node/oscillator side effects. */
  n = node();
  n.world[12] = n.local[12] = .2f;
  held_n = n;
  bk_bom_return_init(&s);
  held_s = s;
  BkNodeReference *nodes[] = {&n, NULL};
  const float *refs[] = {I, I};
  assert(bk_bom_return_multiple(&s, nodes, refs, 2, I, .09f, 1, 16, &done, e));
  assert(!done && memcmp(&n, &held_n, sizeof(n)) &&
         memcmp(&s, &held_s, sizeof(s)));
  assert(!memcmp(s.axes + 2, held_s.axes + 2, 2 * sizeof(*s.axes)));
  held_n = n;
  held_s = s;
  assert(!bk_bom_return_multiple(&s, nodes, refs, 3, I, .09f, 1, 16, &done, e));
  assert(!memcmp(&s, &held_s, sizeof(s)) && !memcmp(&n, &held_n, sizeof(n)));
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
  BkActorPose *p =
      bk_actor_pose_create_loaded(&model, clips, 0, (float[3]){10, 0, 0}, 0, e);
  assert(p);
  bk_actor_pose_publish(p);
  BkClipState before, after;
  assert(bk_actor_pose_state(p, &before));
  float child[16], parent[16];
  memcpy(child, bk_actor_pose_frame(p, 2), 64);
  memcpy(parent, bk_actor_pose_parent_world(p, 1), 64);
  assert(bk_actor_pose_node_reference(p, 1, &n, e));
  m = (BkBomManual){0};
  assert(bk_bom_manual_step(&m, BK_BOM_MANUAL_FIRST, &n, I, 2, 1, .09f, 7.5f, 1,
                            e));
  assert(bk_actor_pose_commit_reference(p, 1, &n, e));
  assert(bk_actor_pose_state(p, &after));
  assert(!memcmp(&before, &after, sizeof(before)) &&
         !memcmp(child, bk_actor_pose_frame(p, 2), 64) &&
         !memcmp(parent, bk_actor_pose_parent_world(p, 1), 64));
  assert(!memcmp(n.local, bk_actor_pose_local(p, 1), 64) &&
         !memcmp(n.world, bk_actor_pose_frame(p, 1), 64));
  n.parent_world[12] += 1;
  assert(!bk_actor_pose_commit_reference(p, 1, &n, e));
  n.parent_world[12] -= 1;
  n.world[0] = NAN;
  assert(!bk_actor_pose_commit_reference(p, 1, &n, e));
  assert(!bk_actor_pose_node_reference(p, 0, &n, e));
  assert(!bk_actor_pose_node_reference(p, 3, &n, e));
  bk_actor_pose_publish(p);
  assert(memcmp(child, bk_actor_pose_frame(p, 2), 64));
  bk_actor_pose_destroy(p);
  bk_clip_set_destroy(clips);
  puts("PASS BOM motion: native normalization, atomic failure, retained reset, "
       "missing-node prefix, node/cache/scheduler isolation");
}
