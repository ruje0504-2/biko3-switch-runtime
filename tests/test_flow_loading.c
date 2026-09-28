#include "scene/flow_loading.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
typedef struct {
  unsigned loads, clicks;
  int fail;
  BkCommonHudState *common;
} Context;
static int load(void *p, uint8_t target, char e[256]) {
  (void)e;
  Context *c = p;
  assert(target == 2);
  ++c->loads;
  assert(c->common->blocked == 1);
  return !c->fail;
}
static int click(void *p, char e[256]) {
  (void)e;
  Context *c = p;
  ++c->clicks;
  return !c->fail;
}
int main(void) {
  char e[256];
  BkFlowLoadingState s = {
      .awaiting = 9,
      .special_background = {.alpha = .5f, .speed = 3, .stage = 3},
      .prompt = {.direction = 1}};
  bk_flow_loading_initialize(&s, 0);
  assert(s.awaiting == 9 && s.prompt.direction == 1 &&
         s.special_background.speed == 3);
  BkCommonHudState common = {.curtain = {.alpha = 0, .speed = 2, .stage = 0}};
  BkFlowTransition flow = {0x50, 2, 2, 1};
  BkFlowLoadingFrame f;
  Context ctx = {0, 0, 1, &common};
  BkFlowLoadingOps ops = {&ctx, load, click};
  assert(!bk_flow_loading_step(&s, &common, &flow, 0, 0, .1f, &ops, &f, e));
  assert(ctx.loads == 1 && common.blocked == 1 && flow.current == 0x50);
  assert(f.count == 2 && f.draws[1].asset == BK_LOADING_CURTAIN &&
         f.draws[1].alpha == 0);
  /* The failed load cannot accidentally advance the flow bytes. */
  bk_flow_loading_initialize(&s, 1);
  s.awaiting = 1;
  common.blocked = 0;
  flow.previous = 0x38;
  assert(!bk_flow_loading_step(&s, &common, &flow, 1, 1, .1f, &ops, &f, e));
  assert(ctx.clicks == 1 && s.awaiting == 1 && common.blocked == 0);
  ctx.fail = 0;
  assert(bk_flow_loading_step(&s, &common, &flow, 1, 1, .1f, &ops, &f, e));
  assert(!s.awaiting && common.blocked == 1 && f.count == 4);
  assert(f.draws[2].asset == BK_LOADING_PROMPT &&
         f.draws[3].asset == BK_LOADING_PULSE);
  flow.mode = 3;
  common.blocked = 255;
  assert(bk_flow_loading_step(&s, &common, &flow, 0, 0, .1f, &ops, &f, e));
  assert(flow.current == 2 && !common.blocked && ctx.loads == 1);
  assert(!bk_flow_loading_step(&s, &common, &flow, 0, 0, .1f, &ops, &f, e));
  puts("PASS flow loading partial failure, snapshots and retained "
       "initialization");
}
