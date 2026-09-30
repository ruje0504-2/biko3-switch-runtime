#include "model/draw_order.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
struct BkDrawOrderCache {
  uint32_t capacity, count;
  float *keys;
  uint32_t *permutation, *indices;
  int valid;
};
void bk_draw_order_cache_destroy(BkDrawOrderCache *c) {
  if (!c)
    return;
  free(c->keys);
  free(c->permutation);
  free(c->indices);
  free(c);
}
BkDrawOrderCache *bk_draw_order_cache_create(uint32_t capacity) {
  if (!capacity || capacity > BK_DRAW_QUEUE_LIMIT)
    return NULL;
  BkDrawOrderCache *c = calloc(1, sizeof(*c));
  if (!c)
    return NULL;
  c->capacity = capacity;
  c->keys = malloc((size_t)capacity * sizeof(*c->keys));
  c->permutation = malloc((size_t)capacity * sizeof(*c->permutation));
  c->indices = malloc((size_t)capacity * sizeof(*c->indices));
  if (!c->keys || !c->permutation || !c->indices) {
    bk_draw_order_cache_destroy(c);
    return NULL;
  }
  return c;
}
int bk_draw_distance(float *out, const float world[16], const float view[16],
                     uint32_t bias) {
  if (!out || !world || !view)
    return 0;
  for (unsigned i = 0; i < 16; ++i)
    if (!isfinite(world[i]) || !isfinite(view[i]))
      return 0;
  float p[3];
  for (unsigned c = 0; c < 3; ++c) {
    double sum = 0;
    for (unsigned k = 0; k < 4; ++k)
      sum += (double)world[12 + k] * view[k * 4 + c];
    p[c] = (float)sum;
  }
  float square =
      (float)((double)p[0] * p[0] + (double)p[1] * p[1] + (double)p[2] * p[2]);
  float distance = (float)(sqrt((double)square) + bias);
  if (!isfinite(distance))
    return 0;
  *out = distance;
  return 1;
}
static int draw_order(BkDrawOrderCache *cache, const BkDrawKey *keys,
                      uint32_t count, uint32_t *order, float *scratch, int stable) {
  if (count > BK_DRAW_QUEUE_LIMIT || (count && (!keys || !order || !scratch)))
    return 0;
  uint32_t ordinary = 0;
  for (uint32_t i = 0; i < count; ++i) {
    if (keys[i].sorted > 1 || (keys[i].sorted && !isfinite(keys[i].distance)))
      return 0;
    ordinary += !keys[i].sorted;
  }
  uint32_t first = 0, second = ordinary;
  for (uint32_t i = 0; i < count; ++i) {
    if (keys[i].sorted)
      order[second++] = i;
    else {
      scratch[first] = (float)keys[i].texture_key;
      order[first++] = i;
    }
  }
  if (cache && ordinary) {
    if (!cache->valid || cache->count != ordinary ||
        memcmp(cache->keys, scratch, ordinary * sizeof(float))) {
      memcpy(cache->keys, scratch, ordinary * sizeof(float));
      for (uint32_t i = 0; i < ordinary; i++)
        cache->permutation[i] = i;
      for (uint32_t i = 0; i < ordinary; i++)
        for (uint32_t j = i + 1; j < ordinary; j++)
          if (scratch[i] < scratch[j]) {
            uint32_t t = cache->permutation[i];
            cache->permutation[i] = cache->permutation[j];
            cache->permutation[j] = t;
          }
      cache->valid = 1;
      cache->count = ordinary;
    }
    memcpy(cache->indices, order, ordinary * sizeof(uint32_t));
    for (uint32_t i = 0; i < ordinary; i++)
      order[i] = cache->indices[cache->permutation[i]];
  } else
    for (uint32_t i = 0; i < ordinary; ++i)
      for (uint32_t j = i + 1; j < ordinary; ++j)
        if (scratch[i] < scratch[j]) {
          uint32_t tmp = order[i];
          order[i] = order[j];
          order[j] = tmp;
        }
  if (ordinary && stable) {
    /* Stable priority/distance order keeps coincident surface layers in file
     * order. Native exchange passes can reorder ties when an unrelated mesh
     * moves, changing additive-light/inverse-shadow composition. */
    for (uint32_t i = ordinary + 1; i < count; ++i) {
      uint32_t item = order[i], j = i;
      const BkDrawKey *a = &keys[item];
      while (j > ordinary) {
        const BkDrawKey *b = &keys[order[j - 1]];
        if (b->priority > a->priority ||
            (b->priority == a->priority && b->distance >= a->distance))
          break;
        order[j] = order[j - 1];
        --j;
      }
      order[j] = item;
    }
  } else if (ordinary)
    for (unsigned pass = 0; pass < 2; ++pass)
      for (uint32_t i = ordinary; i < count; ++i)
        for (uint32_t j = i + 1; j < count; ++j) {
          const BkDrawKey *a = &keys[order[i]], *b = &keys[order[j]];
          if (pass ? a->priority < b->priority : a->distance < b->distance) {
            uint32_t tmp = order[i];
            order[i] = order[j];
            order[j] = tmp;
          }
        }
  return 1;
}
int bk_draw_order(const BkDrawKey *keys, uint32_t count, uint32_t *order,
                  float *scratch) {
  return draw_order(NULL, keys, count, order, scratch, 0);
}
int bk_draw_order_cached(BkDrawOrderCache *cache, const BkDrawKey *keys,
                         uint32_t count, uint32_t *order, float *scratch) {
  if (!cache || count > cache->capacity)
    return 0;
  return draw_order(cache, keys, count, order, scratch, 0);
}

int bk_draw_order_stable(BkDrawOrderCache *cache, const BkDrawKey *keys,
                          uint32_t count, uint32_t *order, float *scratch) {
  if (cache && count > cache->capacity)
    return 0;
  return draw_order(cache, keys, count, order, scratch, 1);
}
