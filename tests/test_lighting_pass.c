#include "game/lighting_pass.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
int main(void) {
  BkLightingPassInput in = {
      .light_count = BK_PASS_LIGHTS, .scene_root = 999, .shadow_mode = 2};
  for (unsigned i = 0; i < 16; ++i)
    in.lights[i] = (BkPassLight){{.2f, .3f, .4f}, 1 + (int)(i % 2), 0};
  for (unsigned i = 0; i < 52; ++i)
    in.objects[i] = i + 1;
  BkLightingPass pass;
  const int modes[] = {0, 1, 2, 10, -1, 3, INT32_MAX};
  for (unsigned i = 0; i < sizeof(modes) / sizeof(*modes); ++i) {
    in.mode = modes[i];
    assert(bk_lighting_pass(&in, &pass));
    assert(pass.count <= BK_PASS_COMMANDS);
    for (unsigned j = 0; j < pass.count; ++j)
      if (pass.commands[j].kind == BK_PASS_LIGHT_ENABLE)
        assert(pass.commands[j].target < BK_PASS_LIGHTS &&
               pass.commands[j].value <= 1);
  }
  BkLightingPass saved = pass;
  in.light_count = 17;
  assert(!bk_lighting_pass(&in, &pass));
  assert(!memcmp(&saved, &pass, sizeof(pass)));
  in.light_count = 16;
  in.lights[15].diffuse[0] = NAN;
  assert(!bk_lighting_pass(&in, &pass));
  assert(!memcmp(&saved, &pass, sizeof(pass)));
  BkPassLight prior[16];
  memcpy(prior, in.lights, sizeof(prior));
  uint8_t names[16] = {0};
  uint32_t color = 123;
  assert(!bk_light_ambient_initialize(in.lights, names, 16, &color));
  assert(color == 123 && !memcmp(prior, in.lights, sizeof(prior)));
  char long_name[65];
  memset(long_name, 'B', sizeof(long_name));
  int32_t group = 71;
  assert(!bk_light_group(long_name, "BK3_L", &group) && group == 71);
  assert(bk_light_group("LightGroup_BK3_L", "bk3_l", &group) && group == 2);
  assert(bk_light_group("LightGroup_BK3_L", "BK3_L", &group) && group == 1);
  assert(bk_light_group("LightGroup_BK3_L", NULL, &group) && group == 2);
  assert(bk_light_group("", NULL, &group) && group == 2);
  assert(bk_light_group("LightGroup_bk3_l", "BK3_L", &group) && group == 2);
  puts("PASS lighting pass: maximum registry/root arrays, ordered bounds, "
       "atomic invalid plans, bounded name classification");
}
