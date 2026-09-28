#include "scene/dialogue_actor_assets.h"
#include "world/menu_camera.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#define CHECK(v)                                                               \
  do {                                                                         \
    if (!(v))                                                                  \
      goto done;                                                               \
  } while (0)
int main(int argc, char **argv) {
  if (argc != 2)
    return 2;
  char error[256] = {0}, path[1024];
  BkResourceStore *store = bk_resources_create(error);
  BkDialogueActorAssets *a = NULL;
  unsigned frames = 0, hidden = 0, resets = 0;
  unsigned long long matrices = 0;
  int rc = 1;
  CHECK(store);
  snprintf(path, sizeof(path), "%s/bk3_01.pp", argv[1]);
  CHECK(bk_resources_mount(store, "bk3_01", path, error));
  CHECK(bk_resources_mount_directory(store, "faces", argv[1], 20480, error));
  for (unsigned group = 0; group < 5; group++) {
    uint32_t rng = 98765, clocks[4] = {1000, 1001, 1002, 1003};
    a = bk_dialogue_actor_assets_create(store, group, clocks, &rng, error);
    CHECK(a);
    BkActorPose *pose = bk_dialogue_actor_assets_pose(a);
    BkDialogueActorState s = {0};
    assert(bk_dialogue_actor_initialize(&s, group));
    BkDialogue d = {.code_c = (int)group + 1, .code_e = 9};
    BkTimer timer = {11, 0, 1};
    uint8_t phase = 0;
    BkMenuCamera camera = {0};
    assert(bk_menu_camera_dialogue(&camera));
    for (unsigned frame = 0; frame < 360; frame++) {
      BkDialogueActorInput in = {.game_seconds = frame % 7 ? .25f : 0,
                                 .mouth_level = frame % 10,
                                 .timestamp_ms = 1100 + frame * 137,
                                 .timer_clock_ms = 1101 + frame * 137,
                                 .face_clocks = {1102 + frame * 137,
                                                 1103 + frame * 137,
                                                 1104 + frame * 137}};
      memcpy(in.camera_world, camera.pose.world, 64);
      if (frame % 30 == 0) {
        d.code_f = (int)(frame / 30 % 6) * 10 + frame / 30 % 7;
        d.code_m = 9 + frame / 30 % 7;
        phase = 0;
      }
      if (frame % 30 == 20) {
        s.rules.expression = -1;
        phase = 1;
      }
      if (frame % 30 == 25) {
        s.rules.expression = 2;
        phase = 5;
      }
      d.code_c = frame % 3 ? (int)group + 1 : 0;
      int old_m = d.code_m;
      BkDialogueActorState saved = s;
      BkClipState before, after;
      assert(bk_actor_pose_state(pose, &before));
      uint32_t old_rng = rng;
      in.game_seconds = NAN;
      assert(!bk_dialogue_actor_assets_step(a, &s, &d, &phase, &timer, &in,
                                            &rng, error));
      assert(!memcmp(&saved, &s, sizeof(s)) && old_rng == rng);
      assert(bk_actor_pose_state(pose, &after) &&
             !memcmp(&before, &after, sizeof(before)));
      in.game_seconds = frame % 7 ? .25f : 0;
      CHECK(bk_dialogue_actor_assets_step(a, &s, &d, &phase, &timer, &in, &rng,
                                          error));
      resets += old_m != 0 && d.code_m == 0;
      uint32_t is_hidden;
      assert(bk_actor_pose_hidden(pose, bk_dialogue_actor_assets_root(a),
                                  &is_hidden));
      hidden += is_hidden != 0;
      if (is_hidden) {
        assert(bk_actor_pose_state(pose, &after));
        assert(!memcmp(&before, &after, sizeof(before)));
      }
      if (frame % 4 != 2)
        bk_actor_pose_publish(pose);
      const BkModel *m = bk_actor_pose_model(pose);
      for (uint32_t f = 0; f < m->frame_count; f++) {
        const float *w = bk_actor_pose_frame(pose, f),
                    *l = bk_actor_pose_local(pose, f);
        for (unsigned j = 0; j < 16; j++)
          assert(isfinite(w[j]) && isfinite(l[j]));
        matrices += 2;
      }
      frames++;
    }
    bk_dialogue_actor_assets_destroy(a);
    a = NULL;
  }
  assert(hidden && resets);
  printf("PASS dialogue actors:5 bodies; %u frames, %u hidden, %u one-shot "
         "resets, %llu matrices\n",
         frames, hidden, resets, matrices);
  rc = 0;
done:
  if (rc)
    fprintf(stderr, "%s\n", error);
  bk_dialogue_actor_assets_destroy(a);
  bk_resources_destroy(store);
  return rc;
}
