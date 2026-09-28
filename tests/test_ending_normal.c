#include "game/ending_normal.h"
#include "model/model.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void) {
  char e[256];
  BkEndingNormalConfig c, before;
  for (unsigned g = 0; g < 5; g++)
    for (unsigned v = 0; v < 2; v++) {
      assert(bk_ending_normal_config(&c, g, v));
      assert(c.expression_a == 9 && c.expression_b == (g == 3 ? 1 : 3));
      for (unsigned i = 0; i < 5; i++)
        for (unsigned j = 0; j < 4; j++)
          assert(c.camera_table[i][j] == UINT32_C(0x40000000));
    }
  before = c;
  assert(!bk_ending_normal_config(&c, 5, 0));
  assert(!memcmp(&c, &before, sizeof(c)));
  assert(!bk_ending_normal_config(&c, 0, 2));
  assert(!memcmp(&c, &before, sizeof(c)));
  assert(!bk_ending_normal_node_name(39));
  assert(!strcmp(bk_ending_normal_node_name(0), "A_kao"));
  assert(!bk_ending_normal_background(5, 0));
  assert(!bk_ending_normal_background(0, 2));
  BkModelFrame frames[5] = {0};
  strcpy(frames[0].name, "root");
  frames[0].parent_index = BK_MODEL_NONE;
  strcpy(frames[1].name, "export target");
  frames[1].parent_index = 4;
  strcpy(frames[2].name, "prefix target");
  frames[2].parent_index = 0;
  frames[3].parent_index = 2;
  strcpy(frames[4].name, "branch");
  frames[4].parent_index = 0;
  BkModel m = {.frames = frames, .frame_count = 5};
  uint32_t node = 33;
  assert(bk_model_find_frame_first(&m, 0, "target", &node, e) && node == 2);
  assert(bk_model_find_frame_first(&m, 4, "target", &node, e) && node == 1);
  assert(bk_model_find_frame_first(&m, 0, "", &node, e) && node == 3);
  assert(bk_model_find_frame_first(&m, 0, "missing", &node, e) &&
         node == BK_MODEL_NONE);
  node = 33;
  assert(!bk_model_find_frame(&m, "target", &node, e) && node == 33);
  assert(!bk_model_find_frame_first(&m, 5, "target", &node, e) && node == 33);
  puts("PASS normal-ending configuration and native subtree binding");
  return 0;
}
