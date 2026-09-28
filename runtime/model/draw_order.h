#ifndef BK_MODEL_DRAW_ORDER_H
#define BK_MODEL_DRAW_ORDER_H
#include <stdint.h>
#define BK_DRAW_QUEUE_LIMIT 16384u
typedef struct {
  uint32_t sorted, texture_key, priority;
  float distance;
} BkDrawKey;
/* Original422e2f/40de00/42c3fc: world*view translation includes world W,
 * squared length rounds once to float, sqrt+bias rounds at final storage. */
int bk_draw_distance(float *out, const float world[16], const float view[16],
                     uint32_t bias);
/* Full42aa47 order for the two mesh queues, flush(enabled=1). The ordinary
 * queue's texture keys stay in their original slots while payloads exchange
 * (42b1bd). With no ordinary entries, original42aa47 skips transparent sort.
 * Else transparent distance then priority use native exchange passes.
 * order and scratch each have count elements and do not alias keys or each
 * other. Validation precedes writes. texture_key is caller-supplied identity:
 * an original process pointer for replay, a stable resource key in the port. */
int bk_draw_order(const BkDrawKey *keys, uint32_t count, uint32_t *order,
                  float *scratch);
typedef struct BkDrawOrderCache BkDrawOrderCache;
/* Cache only the ordinary queue's permutation, keyed by its complete ordered
 * float texture-key sequence. Transparency is still sorted every call.
 * This preserves the native frozen-key swap behavior, not a conventional
 * texture sort. No per-frame allocations; capacity is fixed at construction. */
BkDrawOrderCache *bk_draw_order_cache_create(uint32_t capacity);
void bk_draw_order_cache_destroy(BkDrawOrderCache *);
int bk_draw_order_cached(BkDrawOrderCache *, const BkDrawKey *, uint32_t count,
                         uint32_t *order, float *scratch);
#endif
