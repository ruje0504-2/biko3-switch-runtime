#include "game/prop_config.h"
#include "game/prop_config_tables.inc"
#include <stddef.h>
int bk_prop_config(const BkPropConfig **out, uint32_t *count, uint32_t group,
                   uint32_t area) {
  if (!out || !count || group >= 5 || area >= 9)
    return 0;
  *out = configs + profiles[group][area].offset;
  *count = profiles[group][area].count;
  return 1;
}
