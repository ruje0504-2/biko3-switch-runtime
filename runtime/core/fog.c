#include "core/fog.h"
#include <math.h>
#include <stdio.h>
int bk_fog_validate(const BkFog *f, char error[256]) {
  if (!f || !isfinite(f->start) || !isfinite(f->end) || !isfinite(f->density) ||
      f->enabled > 1 ||
      (f->enabled &&
       (f->mode > 3 || f->table > 1 || f->range_based > 1 || f->density < 0 ||
        f->density > 1 || (f->mode == 3 && f->end <= f->start)))) {
    snprintf(error, 256, "invalid fog mode/range/density");
    return 0;
  }
  return 1;
}
static int boolean(int v) { return v == 0 || v == 1; }
int bk_fog_enable(BkFogState *s, int enabled, int range, int table_supported,
                  int range_supported) {
  if (!s || !boolean(enabled) || !boolean(range) || !boolean(table_supported) ||
      !boolean(range_supported))
    return 0;
  s->enabled = (uint32_t)enabled;
  if (table_supported)
    s->table_mode = 3;
  else
    s->vertex_mode = 3;
  if (range_supported && range && enabled)
    s->range_based = 1;
  return 1;
}
int bk_fog_load(BkFogState *s, const BkFog *f, int table_supported,
                int range_supported, char error[256]) {
  if (!s || !boolean(table_supported) || !boolean(range_supported) ||
      !bk_fog_validate(f, error))
    return 0;
  if (!f->enabled)
    return 1;
  BkFogState next = *s;
  bk_fog_enable(&next, 1, (int)f->range_based, table_supported,
                range_supported);
  if (f->table && table_supported)
    next.table_mode = f->mode;
  else
    next.vertex_mode = f->mode;
  next.color = f->color;
  if (f->mode == 3) {
    next.start = f->start;
    next.end = f->end;
  } else
    next.density = f->density;
  *s = next;
  return 1;
}
int bk_fog_resolve(const BkFogState *s, BkFog *out, char error[256]) {
  if (!s || !out)
    return 0;
  BkFog fog = {s->enabled,
               s->table_mode ? s->table_mode : s->vertex_mode,
               s->table_mode != 0,
               s->table_mode ? 0 : s->range_based,
               s->color,
               s->start,
               s->end,
               s->density};
  if (!bk_fog_validate(&fog, error))
    return 0;
  *out = fog;
  return 1;
}
