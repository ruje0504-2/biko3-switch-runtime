#include "scene/item_assets.h"
#include "scene/item_notice_state.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
typedef struct {
  unsigned calls;
  int fail;
} Service;
static int sound(void *ctx, char error[256]) {
  (void)error;
  Service *s = ctx;
  s->calls++;
  return !s->fail;
}
static int notice(void *ctx, uint32_t id, char error[256]) {
  assert(id == 20000);
  return sound(ctx, error);
}
static void notice_state(void) {
  BkItemNoticeState state = {.deadline = 73};
  BkItemPickupState pickup = {.notice_visible = 1, .selected_item = 2};
  BkTextFlow flow = {7, 2, 8, 0, 4};
  BkItemNoticeFrame frame;
  assert(bk_item_notice_initialize(&state, 0));
  assert(state.deadline == 73 && state.duration == 5000);
  assert(state.panel.alpha == 1 && state.panel.stage == 0);
  state.icons[2] = (BkFadeSprite){.25f, 2, 1};
  assert(bk_item_notice_initialize(&state, 1));
  assert(state.icons[2].alpha == .25f && state.icons[2].stage == 1);
  assert(bk_item_notice_step(&state, &pickup, &flow, 0, 100, &frame));
  assert(state.deadline == 5100 && pickup.notice_timer_armed == 1);
  assert(frame.bind_message && frame.draw_text && state.panel.stage == 2);
  assert(flow.enabled == 1 && flow.started == 0 && flow.scroll == 2);
  /* Expiration on stage2 still enters stage3 and draws font once. */
  assert(bk_item_notice_step(&state, &pickup, &flow, .25f, 5100, &frame));
  assert(!pickup.notice_visible && !pickup.notice_timer_armed);
  assert(frame.bind_message && frame.draw_text && state.panel.stage == 3);
  assert(bk_item_notice_step(&state, &pickup, &flow, .25f, 5101, &frame));
  assert(!frame.bind_message && !frame.draw_text && state.panel.stage == 5);
  /* Pickup while fading resets timer without reversing the fade. */
  pickup.notice_visible = 1;
  pickup.notice_timer_armed = 0;
  assert(bk_item_notice_step(&state, &pickup, &flow, 1, 6000, &frame));
  assert(state.deadline == 11000 && state.panel.stage == 0);
  assert(bk_item_notice_step(&state, &pickup, &flow, .25f, 6250, &frame));
  assert(state.panel.stage == 1 && state.panel.alpha == .25f);
  /* A late invalid sprite must not commit preceding timer/panel changes. */
  state.icons[4].speed = NAN;
  BkItemNoticeState saved = state;
  BkItemPickupState saved_pickup = pickup;
  BkTextFlow saved_flow = flow;
  BkItemNoticeFrame saved_frame = frame;
  assert(!bk_item_notice_step(&state, &pickup, &flow, .25f, 11000, &frame));
  assert(memcmp(&saved, &state, sizeof(state)) == 0);
  assert(memcmp(&saved_pickup, &pickup, sizeof(pickup)) == 0);
  assert(memcmp(&saved_flow, &flow, sizeof(flow)) == 0);
  assert(memcmp(&saved_frame, &frame, sizeof(frame)) == 0);
  BkItemNoticeSprite layout[6] = {0}, saved_layout[6] = {0};
  assert(!bk_item_notice_layout(layout, 5, 640));
  assert(memcmp(layout, saved_layout, sizeof(layout)) == 0);
}
int main(void) {
  notice_state();
  BkItemState states[BK_ITEM_LIMIT] = {0}, before[BK_ITEM_LIMIT];
  uint8_t inventory[5] = {0, 2, 1, 128, 255};
  states[0].hidden = 93;
  states[0].yaw = -77;
  states[1].hidden = 47;
  states[1].yaw = 90;
  states[15].yaw = NAN;
  uint32_t count = 456;
  assert(bk_item_initialize(states, &count, 4, 4, inventory));
  assert(count == 2 && states[0].id == 1 && states[1].id == 4);
  assert(states[0].position[1] == 48 && states[1].position[1] == 17);
  assert(states[0].hidden == 93 && states[1].hidden == 47);
  assert(states[0].yaw == -77 && states[1].yaw == 90 && isnan(states[15].yaw));
  inventory[1] = 1;
  inventory[4] = 0;
  assert(bk_item_initialize(states, &count, 4, 4, inventory));
  assert(states[0].hidden == 1 && states[1].hidden == 0);
  memcpy(before, states, sizeof(before));
  assert(!bk_item_initialize(states, &count, 5, 0, inventory));
  assert(memcmp(before, states, sizeof(before)) == 0 && count == 2);
  states[1].yaw = INFINITY;
  memcpy(before, states, sizeof(before));
  assert(!bk_item_initialize(states, &count, 4, 4, inventory));
  assert(memcmp(before, states, sizeof(before)) == 0 && count == 2);
  /* No active slot means no active yaw to validate. */
  assert(bk_item_initialize(states, &count, 0, 0, inventory) && count == 0);
  char error[256];
  BkResourceStore *store = bk_resources_create(error);
  assert(store);
  BkItemAssets *empty =
      bk_item_assets_create(store, 0, 0, inventory, states, error);
  assert(empty && bk_item_assets_count(empty) == 0);
  assert(bk_item_assets_step(empty, .1f, error));
  assert(!bk_item_assets_step(empty, NAN, error));
  assert(!bk_item_assets_step(empty, -1, error));
  assert(!bk_item_assets_step(empty, 1e30f, error));
  assert(!bk_item_assets_pose(empty, 0) && !bk_item_assets_bind_pose(empty, 0));
  assert(!bk_item_assets_hidden(empty, 0, 1));
  assert(bk_item_assets_snapshot(empty, before));
  assert(memcmp(before, states, sizeof(before)) == 0);
  states[1].yaw = 0;
  assert(!bk_item_assets_create(store, 4, 4, inventory, states, error));
  bk_item_assets_destroy(empty);
  bk_resources_destroy(store);
  memset(states, 0, sizeof(states));
  states[0].position[1] = 1000; /* Vertical separation is not a pickup gate. */
  BkItemPickupState pickup = {.selected_item = 3, .message_cursor = 42};
  BkItemPickups out = {.count = 55};
  Service service = {.fail = 1};
  BkItemPickupOps ops = {&service, sound, notice};
  const float current[3] = {-10, 0, 0}, previous[3] = {10, 0, 0};
  assert(!bk_item_pickup_run(states, 1, &pickup, 2, current, previous, NULL,
                             &out, error));
  assert(states[0].hidden == 0 && pickup.collected[0] == 0 && out.count == 55);
  assert(!bk_item_pickup_run(states, 1, &pickup, 2, current, previous, &ops,
                             &out, error));
  assert(states[0].hidden == 1 && pickup.collected[0] == 0 &&
         service.calls == 1);
  assert(pickup.selected_item == 3 && pickup.message_cursor == 42 &&
         out.count == 55);
  states[0].hidden = 0;
  service.fail = 0;
  assert(bk_item_pickup_run(states, 1, &pickup, 2, current, previous, &ops,
                            &out, error));
  assert(states[0].hidden == 1 && pickup.collected[0] == 1 &&
         service.calls == 3);
  assert(pickup.notice_visible == 1 && pickup.notice_timer_armed == 0);
  assert(pickup.selected_item == 0 && pickup.message_cursor == 0 &&
         out.count == 1);
  puts("PASS item initialization atomicity, retained/inactive state, missing "
       "resources and empty owner");
}
