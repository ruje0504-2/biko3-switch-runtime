#include "game/ending_4d39e6_config.h"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static void check_config(unsigned group, unsigned variant,
                         const char *const visible[3]) {
  BkEnding4d39Config c;
  assert(bk_ending_4d39e6_config(&c, group, variant));
  char expected[32];
  snprintf(expected, sizeof(expected), "h%02u_12.xan", group + 1);
  assert(!strcmp(c.primary, expected));
  snprintf(expected, sizeof(expected), "h%02u_12.fam", group + 1);
  assert(!strcmp(c.face, expected));
  assert(c.yaw == (group == 0 ? 90 : 0));
  assert(c.camera_yaw == c.yaw);
  assert(c.expression_a == 6);
  assert(c.expression_b == (group < 2 ? 3 : 1));
  for (unsigned i = 0; i < 3; ++i)
    assert(!strcmp(c.visible_nodes[i], visible[i]));
  for (unsigned i = 0; i < 3; ++i)
    assert(c.position[i] == 0.0f);
  for (unsigned i = 0; i < 5; ++i)
    for (unsigned j = 0; j < 4; ++j)
      assert(c.camera_table[i][j] == UINT32_C(0x40000000));
}

int main(void) {
  static const char *const empty[3] = {"", "", ""};
  static const char *const names[5][3] = {
      {"O_body", "otoko_j1", "O_bo"},
      {"otoko", "", ""},
      {"otoko", "", ""},
      {"O_body", "otoko_j1", "O_bo"},
      {"_O_body", "otoko_j1", "_O_bo"}};
  for (unsigned group = 0; group < 5; ++group) {
    check_config(group, 0, empty);
    check_config(group, 1, names[group]);
  }
  BkEnding4d39Config keep = {0};
  assert(!bk_ending_4d39e6_config(&keep, 5, 0));
  assert(!bk_ending_4d39e6_config(&keep, 0, 2));
  assert(!bk_ending_4d39e6_config(NULL, 0, 0));

  float out[3][3] = {{-1, -1, -1}, {-1, -1, -1}, {-1, -1, -1}};
  const float node[3] = {1.0f, 10.5f, -3.0f};
  const float second[3] = {4.0f, -1.5f, 8.0f};
  const float third[3] = {17, 23, 29};
  char error[256];
  assert(bk_ending_4d39e6_targets(out, node, second, third, error));
  assert(!memcmp(out[0], node, sizeof(node)));
  assert(!memcmp(out[1], second, sizeof(second)));
  assert(!memcmp(out[2], third, sizeof(third)));
  float before[3][3];
  memcpy(before, out, sizeof(before));
  const float bad[3] = {NAN, 0.0f, 0.0f};
  assert(!bk_ending_4d39e6_targets(out, bad, second, third, error));
  assert(!memcmp(out, before, sizeof(out)));
  assert(!bk_ending_4d39e6_targets(NULL, node, second, third, error));
  puts("PASS ending 4D39E6 config: 10 authored variants, cached node order, invalid-input preservation");
  return 0;
}
