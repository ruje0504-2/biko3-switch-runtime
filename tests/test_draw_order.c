#include "model/draw_order.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static uint32_t rng = 0x42b1bd;
static uint32_t next(void) {
  rng = rng * 1664525u + 1013904223u;
  return rng;
}
static void check_cache(void) {
  BkDrawOrderCache *cache = bk_draw_order_cache_create(128);
  assert(cache && !bk_draw_order_cache_create(0));
  BkDrawKey keys[129];
  uint32_t plain[129], cached[129];
  float scratch[129];
  for (unsigned frame = 0; frame < 6000; frame++) {
    unsigned count = frame % 129;
    for (unsigned i = 0; i < count; i++)
      keys[i] = (BkDrawKey){.sorted = next() % 3 == 0,
                            .texture_key = next(),
                            .priority = next() % 5,
                            .distance = (float)(next() % 100)};
    for (unsigned repeat = 0; repeat < 4; repeat++) {
      /* Keep ordinary key sequence stable, change transparent distance/priority
       * and their positions, then invalidate by count and texture content. */
      for (unsigned i = 0; i < count; i++)
        if (keys[i].sorted) {
          keys[i].distance = (float)(next() % 100);
          keys[i].priority = next() % 5;
        }
      if (repeat == 2 && count > 1) {
        BkDrawKey tmp = keys[0];
        memmove(keys, keys + 1, (count - 1) * sizeof(*keys));
        keys[count - 1] = tmp;
      }
      assert(bk_draw_order(keys, count, plain, scratch));
      assert(bk_draw_order_cached(cache, keys, count, cached, scratch));
      assert(!memcmp(plain, cached, count * sizeof(*plain)));
    }
  }
  keys[0] = (BkDrawKey){.sorted = 1, .distance = NAN};
  cached[0] = 99;
  assert(!bk_draw_order_cached(cache, keys, 1, cached, scratch) &&
         cached[0] == 99);
  assert(!bk_draw_order_cached(cache, keys, 129, cached, scratch));
  assert(bk_draw_order_cached(cache, NULL, 0, NULL, NULL));
  bk_draw_order_cache_destroy(cache);
}
static void stable_layers(void) {
  /* An unrelated moving transparent object must not swap coincident light
   * and shadow layers. Priority dominates distance, ties retain insertion. */
  BkDrawOrderCache *cache = bk_draw_order_cache_create(8);
  assert(cache);
  for (unsigned f = 0; f < 1000; ++f) {
    BkDrawKey keys[] = {{0, 1, 0, 0},   {1, 0, 0, 100},
                        {1, 0, 0, 100}, {1, 0, 0, (float)(f % 200)},
                        {1, 0, 2, 10},  {1, 0, 2, 20}};
    uint32_t order[6], plain[6];
    float scratch[6];
    assert(bk_draw_order_stable(cache, keys, 6, order, scratch));
    assert(bk_draw_order_stable(NULL, keys, 6, plain, scratch));
    assert(!memcmp(order, plain, sizeof(order)));
    assert(order[0] == 0 && order[1] == 5 && order[2] == 4);
    unsigned light = 6, shadow = 6;
    for (unsigned i = 0; i < 6; ++i) {
      if (order[i] == 1)
        light = i;
      if (order[i] == 2)
        shadow = i;
    }
    assert(light < shadow);
  }
  BkDrawKey bad = {.sorted = 1, .distance = NAN};
  uint32_t order = 42;
  float scratch;
  assert(!bk_draw_order_stable(cache, &bad, 1, &order, &scratch) &&
         order == 42);
  assert(bk_draw_order_stable(cache, NULL, 0, NULL, NULL));
  bk_draw_order_cache_destroy(cache);
}
int main(void) {
  stable_layers();
  check_cache();
  BkDrawKey keys[] = {{0, 1, 0, 0},  {0, 3, 0, 0},  {0, 2, 0, 0},
                      {1, 0, 0, 10}, {1, 0, 0, 20}, {1, 0, 1, 1}};
  uint32_t order[6];
  float scratch[6];
  assert(bk_draw_order(keys, 6, order, scratch));
  /* Frozen ordinary keys produce [2,0,1], NOT descending texture order. */
  const uint32_t wanted[] = {2, 0, 1, 5, 3, 4};
  assert(!memcmp(order, wanted, sizeof(order)));
  assert(bk_draw_order(keys + 3, 3, order, scratch));
  assert(order[0] == 0 && order[1] == 1 && order[2] == 2);
  memset(order, 0x5a, sizeof(order));
  uint32_t saved[6];
  memcpy(saved, order, sizeof(order));
  keys[5].distance = NAN;
  assert(!bk_draw_order(keys, 6, order, scratch));
  assert(!memcmp(order, saved, sizeof(order)));
  assert(bk_draw_order(NULL, 0, NULL, NULL));
  assert(!bk_draw_order(keys, BK_DRAW_QUEUE_LIMIT + 1, order, scratch));
  keys[5].distance = 0;
  keys[5].sorted = 2;
  assert(!bk_draw_order(keys, 6, order, scratch));
  float w[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 3, 4, 0, 2};
  float v[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, -1.5f, -2, 0, 1};
  float distance = 99;
  assert(bk_draw_distance(&distance, w, v, 7) && distance == 7);
  w[15] = 1;
  assert(bk_draw_distance(&distance, w, v, 7) && distance == 9.5f);
  w[0] = NAN;
  assert(!bk_draw_distance(&distance, w, v, 7) && distance == 9.5f);
  puts("PASS native queue partition, frozen ordinary keys, transparent bypass, "
       "priority, W and invalid inputs");
}
