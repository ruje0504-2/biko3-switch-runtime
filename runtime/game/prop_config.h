#ifndef BK_GAME_PROP_CONFIG_H
#define BK_GAME_PROP_CONFIG_H
#include "world/route.h"
typedef struct {
  int32_t kind, cursor;
  const char *clip, *route_file, *anchor;
  uint32_t point_count;
  BkRoutePoint points[4];
} BkPropConfig;
/* 511940 tables58ba3c/58c57c and512fca. 45 valid entry profiles; max16
 * props. File routes read their first1280 bytes, zero-extended. Inline
 * zero routes at(0,5) are real background-anchor instances, not missing data.
 * Immutable config; caller owns route/model/timeline/instance state. */
int bk_prop_config(const BkPropConfig **out, uint32_t *count, uint32_t group,
                   uint32_t area);
#endif
