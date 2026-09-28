#include "game/background_config.h"
#include "game/background_config_tables.inc"
#include <stddef.h>
const BkBackgroundConfig *bk_background_config(uint32_t group, uint32_t area) {
  return group < 5 && area < 9 ? &profiles[group][area] : NULL;
}
