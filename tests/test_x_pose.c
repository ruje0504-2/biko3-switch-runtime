#include "model/animation.h"
#include "model/x_pose.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static const char valid[] =
    "xof 0302txt 0032\nHeader {1;0;1;} Material mat {0;0;0;0;;} "
    "Frame parent {FrameTransformMatrix {1,0,0,0,0,1,0,0,0,0,-1,0,2,3,4,1;;} "
    "Frame cam { Mesh dummy { {mat} } }} "
    "AnimationSet {Animation {{cam} "
    "AnimationKey {2;2;0;3;0,0,0;;,10;3;10,20,30;;;} "
    "AnimationKey {1;2;5;3;2,2,2;;,15;3;3,3,3;;;} }}";
static void reject(const char *body) {
  char input[2048], error[256];
  snprintf(input, sizeof(input), "xof 0302txt 0032\n%s", body);
  BkModel *m = (BkModel *)(uintptr_t)1;
  assert(bk_model_x_pose_decode(input, strlen(input), &m, error) ==
         BK_MODEL_INVALID);
  assert(!m && *error);
}
int main(void) {
  char error[256];
  BkModel *m = NULL;
  assert(bk_model_x_pose_decode(valid, strlen(valid), &m, error) ==
         BK_MODEL_OK);
  assert(m->frame_count == 3 && m->mesh_count == 0 && m->submesh_count == 0);
  assert(!memcmp(m->source, valid, strlen(valid)));
  assert(m->frames[2].parent_index == 1);
  BkModelAnimation *a = bk_model_animation_create(m, error);
  assert(a && bk_model_animation_duration(a) == 15);
  float pose[48];
  assert(bk_model_animation_sample(a, 10, 0, pose, 48, error));
  assert(pose[44] == 12 && pose[45] == 23 && pose[46] == -26);
  bk_model_animation_destroy(a);
  bk_model_destroy(m);
  reject("Frame a { Frame a {} }");
  reject("Frame a {Unknown {}} ");
  reject("Frame a {FrameTransformMatrix {1,2,3;}} ");
  reject("Frame a {Mesh m {\"unterminated}} ");
  reject("Frame a {Mesh m {/*unterminated}} ");
  reject("Frame a {} AnimationSet {Animation {{missing} AnimationKey "
         "{2;2;0;3;0,0,0;1;3;1,1,1;}}}");
  reject("Frame a {} AnimationSet {Animation {{a} AnimationKey {4;1;0;16;}}}");
  reject("Frame a {} AnimationSet {Animation {{a} AnimationKey "
         "{2;2;1;3;0,0,0;1;3;1,1,1;}}}");
  reject("Frame a {} AnimationSet {Animation {{a} AnimationKey {2;65537;}}}");
  reject("Frame a {} AnimationSet {} AnimationSet {}");
  reject("Frame a {FrameTransformMatrix {1e10,0,0;}} ");
  reject("Frame a {} AnimationSet {Animation {{a} AnimationKey "
         "{2;1;0;3;1,1,1;}}}");
  /* Deterministic mutation/truncation checks exercise cleanup on partial
   * hierarchy, track and key allocations under sanitizers. */
  unsigned accepted = 0, rejected = 0;
  uint32_t rng = 0x447667;
  for (unsigned i = 0; i < 4096; ++i) {
    char text[sizeof(valid)];
    memcpy(text, valid, sizeof(valid));
    size_t length = strlen(valid);
    rng = rng * 1664525u + 1013904223u;
    if (i % 2)
      length = rng % length;
    else
      text[rng % length] = (char)((rng >> 16) & 127);
    m = NULL;
    if (bk_model_x_pose_decode(text, length, &m, error) == BK_MODEL_OK) {
      assert(m && m->frame_count > 1);
      float *world = malloc((size_t)m->frame_count * 64);
      assert(world && bk_model_world_matrices(
                          m, world, (size_t)m->frame_count * 16, error));
      free(world);
      ++accepted;
    } else {
      assert(!m);
      ++rejected;
    }
    bk_model_destroy(m);
  }
  assert(accepted && rejected > 3000);
  printf("text X pose tests passed: %u accepted, %u rejected mutations\n",
         accepted, rejected);
  return 0;
}
