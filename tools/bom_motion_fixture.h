#ifndef BK_BOM_MOTION_FIXTURE_H
#define BK_BOM_MOTION_FIXTURE_H
/* Explicit controller inputs for numeric tests, never a gameplay dispatcher. */
#include "scene/bom_assets.h"
#include <string.h>
typedef struct {
  BkBomManual manual[2];
  BkBomReturn returns[2];
} BkBomMotionFixture;
static void motion_fixture_init(BkBomMotionFixture *s) {
  memset(s, 0, sizeof(*s));
  for (unsigned i = 0; i < 2; i++)
    bk_bom_return_init(s->returns + i);
}
static int motion_fixture_step(BkBomMotionFixture *s, BkBomAssets *a,
                               unsigned step, char e[256]) {
  static const float I[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
  unsigned count = bk_bom_assets_count(a) > 1 ? 2 : 1, phase = step % 30;
  int done;
  if (phase < 10) {
    for (unsigned i = 0; i < count; i++)
      if (!bk_bom_assets_manual(a, s->manual + i, (BkBomManualKind)i, i,
                                (int)(step * 13 + i * 7) % 17 - 8,
                                (int)(step * 11 + i * 3) % 19 - 9, .09f, 7.5f,
                                step % 2, e))
        return 0;
  } else if (phase < 20) {
    if (!bk_bom_assets_return_multiple(
            a, s->returns + 1, count, I, .09f, step % 2,
            (uint32_t[]){0, 16, 33, 60, 333}[step % 5], &done, e))
      return 0;
  } else if (!bk_bom_assets_return_single(
                 a, s->returns, 0, I, .09f, step % 2,
                 (uint32_t[]){0, 16, 33, 60, 333}[step % 5], phase == 29, &done,
                 e))
    return 0;
  return bk_bom_assets_follow_references(a, e);
}
#endif
