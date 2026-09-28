#include "game/item.h"
#include "world/proximity.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
/* Pinned EXE4ec9f0/4ef168; checked against complete native loader. */
static const BkItemConfig profiles[5][9][2] = {
    {
        {{0}},
        {
            {1, "e01_01.xan", {333.0f, 0.0f, 236.0f}},
        },
        {
            {3, "e01_03.xan", {-205.0f, 0.0f, 21.0f}},
        },
        {{0}},
        {
            {4, "e01_04.xan", {-258.0f, -3.0f, -7.0f}},
        },
        {
            {2, "e01_02.xan", {414.0f, 0.0f, 82.0f}},
        },
        {
            {0, "e01_00.xan", {-141.0f, 3.0f, 105.0f}},
        },
        {{0}},
        {{0}},
    },
    {
        {{0}},
        {
            {0, "e02_00.xan", {345.0f, 0.0f, -245.0f}},
        },
        {
            {2, "e02_02.xan", {163.0f, 0.0f, 427.0f}},
        },
        {{0}},
        {{0}},
        {
            {4, "e02_04.xan", {157.0f, 0.0f, -430.0f}},
        },
        {
            {3, "e02_03.xan", {531.0f, 0.0f, 503.0f}},
        },
        {{0}},
        {{0}},
    },
    {
        {
            {4, "e03_04.xan", {23.0f, 0.0f, 117.0f}},
        },
        {{0}},
        {{0}},
        {
            {0, "e03_00.xan", {-254.0f, 0.0f, 157.0f}},
        },
        {
            {1, "e03_01.xan", {-114.0f, 0.0f, -28.0f}},
        },
        {
            {2, "e03_02.xan", {64.0f, 0.0f, -190.0f}},
        },
        {
            {3, "e03_03.xan", {-139.0f, 0.0f, 82.0f}},
        },
        {{0}},
        {{0}},
    },
    {
        {
            {3, "e04_03.xan", {-202.0f, 0.0f, 266.0f}},
        },
        {{0}},
        {
            {1, "e04_01.xan", {388.0f, 0.0f, -63.0f}},
        },
        {{0}},
        {
            {4, "e04_04.xan", {0.0f, 0.0f, -158.0f}},
        },
        {
            {2, "e04_02.xan", {58.0f, 0.0f, 226.0f}},
        },
        {
            {0, "e04_00.xan", {-234.0f, 0.0f, 206.0f}},
        },
        {{0}},
        {{0}},
    },
    {
        {
            {4, "e05_04.xan", {65.0f, 0.0f, -217.0f}},
        },
        {
            {4, "e05_04.xan", {-200.0f, 0.0f, -140.0f}},
        },
        {
            {4, "e05_04.xan", {-109.0f, 0.0f, 164.0f}},
        },
        {
            {4, "e05_04.xan", {171.0f, 0.0f, 299.0f}},
        },
        {
            {1, "e05_01.xan", {50.0f, 48.0f, 120.0f}},
            {4, "e05_04.xan", {82.0f, 17.0f, -91.0f}},
        },
        {
            {2, "e05_02.xan", {0.0f, -8.0f, -113.0f}},
            {4, "e05_04.xan", {212.0f, -10.0f, 8.0f}},
        },
        {
            {0, "e05_00.xan", {189.0f, 0.0f, -140.0f}},
            {4, "e05_04.xan", {-162.0f, 0.0f, 102.0f}},
        },
        {
            {4, "e05_04.xan", {184.0f, 32.0f, -156.0f}},
        },
        {
            {3, "e05_03.xan", {-80.0f, 2.0f, 4.0f}},
        },
    },
};
static const uint32_t counts[5][9] = {
    {0, 1, 1, 0, 1, 1, 1, 0, 0}, {0, 1, 1, 0, 0, 1, 1, 0, 0},
    {1, 0, 0, 1, 1, 1, 1, 0, 0}, {1, 0, 1, 0, 1, 1, 1, 0, 0},
    {1, 1, 1, 1, 2, 2, 2, 1, 1},
};
int bk_item_config(const BkItemConfig **out, uint32_t *count, uint32_t group,
                   uint32_t area) {
  if (!out || !count || group >= 5 || area >= 9)
    return 0;
  *out = profiles[group][area];
  *count = counts[group][area];
  return 1;
}
int bk_item_initialize(BkItemState slots[BK_ITEM_LIMIT], uint32_t *count,
                       uint32_t group, uint32_t area,
                       const uint8_t collected[BK_ITEM_TYPES]) {
  const BkItemConfig *config;
  uint32_t n;
  if (!slots || !count || !collected ||
      !bk_item_config(&config, &n, group, area))
    return 0;
  BkItemState next[BK_ITEM_LIMIT];
  memcpy(next, slots, sizeof(next));
  for (uint32_t i = 0; i < n; ++i) {
    if (!isfinite(next[i].yaw))
      return 0;
    next[i].id = config[i].id;
    memcpy(next[i].position, config[i].position, sizeof(next[i].position));
    uint8_t value = collected[config[i].id];
    if (value == 0 || value == 1)
      next[i].hidden = value;
  }
  memcpy(slots, next, sizeof(next));
  *count = n;
  return 1;
}
int bk_item_pickup_plan(const BkItemState slots[BK_ITEM_LIMIT],
                        uint32_t present, const float current[3],
                        const float previous[3], BkItemPickups *out) {
  if (!slots || !current || !previous || !out || (present >> BK_ITEM_LIMIT))
    return 0;
  for (unsigned i = 0; i < 3; ++i)
    if (!isfinite(current[i]) || !isfinite(previous[i]))
      return 0;
  BkItemPickups result = {0};
  for (uint32_t i = 0; i < BK_ITEM_LIMIT; ++i) {
    if (!(present & (1u << i)) || slots[i].hidden)
      continue;
    int hit;
    if (slots[i].id >= BK_ITEM_TYPES ||
        !bk_proximity_interior_xz(&hit, current, previous, slots[i].position,
                                  10))
      return 0;
    if (hit)
      result.slots[result.count++] = i;
  }
  *out = result;
  return 1;
}
int bk_item_pickup_run(BkItemState slots[BK_ITEM_LIMIT], uint32_t present,
                       BkItemPickupState *state, uint32_t group,
                       const float current[3], const float previous[3],
                       const BkItemPickupOps *ops, BkItemPickups *out,
                       char error[256]) {
  BkItemPickups picks;
  if (!state || !out || group >= 5 ||
      !bk_item_pickup_plan(slots, present, current, previous, &picks) ||
      (picks.count && (!ops || !ops->play_sound || !ops->load_notice))) {
    snprintf(error, 256,
             "item pickup: invalid state/input or missing services");
    return 0;
  }
  for (uint32_t i = 0; i < picks.count; ++i) {
    BkItemState *item = &slots[picks.slots[i]];
    item->hidden = 1;
    if (!ops->play_sound(ops->context, error))
      return 0;
    state->collected[item->id] = 1;
    state->notice_visible = 1;
    state->notice_timer_armed = 0;
    if (!ops->load_notice(ops->context, group * 10000 + item->id, error))
      return 0;
    state->selected_item = (int32_t)item->id;
    state->message_cursor = 0;
  }
  *out = picks;
  return 1;
}
