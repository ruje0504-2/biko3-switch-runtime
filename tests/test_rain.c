#include "game/rain.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
int main(void) {
  BkRainState s;
  for (unsigned g = 0; g < 5; ++g)
    for (unsigned a = 0; a < 9; ++a) {
      assert(bk_rain_initialize(&s, g, a, 1));
      assert(s.present == (g == 4 && a < 7 ? 0xffffu : 0));
    }
  assert(bk_rain_initialize(&s, 4, 0, 1));
  uint32_t random = 17;
  BkRainDraw draw = {0};
  assert(bk_rain_draw(&s, &random, 1, .5f, &draw));
  assert(draw.count == 16 && s.phase[0] == 2);
  assert(bk_rain_draw(&s, &random, 1, .5f, &draw));
  assert(draw.count == 16 && s.phase[15] == 3);
  BkRainState held = s;
  uint32_t held_random = random;
  assert(bk_rain_draw(&s, &random, 0, NAN, &draw));
  assert(draw.count == 0 && random == held_random &&
         !memcmp(&s, &held, sizeof(s)));
  BkRainDraw held_draw = draw;
  const float invalid[] = {0, -1, NAN, INFINITY, 1e20f, .0001f};
  for (unsigned i = 0; i < sizeof(invalid) / sizeof(*invalid); ++i) {
    assert(!bk_rain_draw(&s, &random, 1, invalid[i], &draw));
    assert(!memcmp(&draw, &held_draw, sizeof(draw)) &&
           !memcmp(&s, &held, sizeof(s)) && random == held_random);
  }
  s.phase[15] = 4;
  held = s;
  assert(!bk_rain_draw(&s, &random, 1, 1, &draw));
  assert(!memcmp(&s, &held, sizeof(s)) && random == held_random &&
         !memcmp(&draw, &held_draw, sizeof(draw)));
  assert(bk_rain_initialize(&s, 4, 0, 1));
  s.present = 0x8001;
  assert(bk_rain_draw(&s, &random, 1, .5f, &draw));
  assert(draw.count == 2 && draw.sprites[0].slot == 0 &&
         draw.sprites[1].slot == 15 && s.phase[7] == 1);
  puts("PASS rain:45 profiles, sparse slots, phase lifecycle and transactional "
       "rejection");
}
