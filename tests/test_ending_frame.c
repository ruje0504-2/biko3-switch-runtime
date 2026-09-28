#include "game/ending_frame.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef struct {
  BkEndingFrameState *state;
  uint8_t (*working)[8];
  BkEndingCall calls[16];
  unsigned count, clocks, keys, last_key;
  uint32_t time[2], result;
  int key, fail_at, mutate_common, mutate_first, fail_clock;
} Context;
static int invoke(void *p, const BkEndingCall *c, uint32_t *result,
                  char e[256]) {
  Context *t = p;
  assert(t->count < 16);
  t->calls[t->count++] = *c;
  if (t->mutate_common && c->operation == BK_ENDING_COMMON_4D7AC4) {
    t->state->phase = 6;
    t->state->group = 4;
  }
  if (t->mutate_first && c->operation == BK_ENDING_STAGE_48D8E9) {
    assert(t->working[4][5] == 1);
    t->state->phase = 9;
  }
  if ((int)t->count == t->fail_at) {
    snprintf(e, 256, "missing recovered child");
    return 0;
  }
  *result = t->result;
  return 1;
}
static int clock_read(void *p, uint32_t *out, char e[256]) {
  Context *t = p;
  if (t->fail_clock) {
    snprintf(e, 256, "clock failure");
    return 0;
  }
  assert(t->clocks < 2);
  *out = t->time[t->clocks++];
  return 1;
}
static int key_read(void *p, unsigned key, int *out, char e[256]) {
  (void)e;
  Context *t = p;
  t->keys++;
  t->last_key = key;
  *out = t->key;
  return 1;
}
int main(void) {
  char e[256];
  uint8_t work[5][8], before[5][8];
  BkEndingFrameInput input = {{11, 22, 33, 44, 55, 66, 77, 88, 99, 110, 121}};
  BkEndingFrameState s;
  Context t;
  BkEndingFrameOps ops = {&t, invoke, clock_read, key_read};
#define RESET()                                                                \
  do {                                                                         \
    memset(&s, 0, sizeof(s));                                                  \
    memset(work, 0x83, sizeof(work));                                          \
    memcpy(before, work, sizeof(work));                                        \
    memset(&t, 0, sizeof(t));                                                  \
    t.state = &s;                                                              \
    t.working = work;                                                          \
    t.fail_at = -1;                                                            \
    ops = (BkEndingFrameOps){&t, invoke, clock_read, key_read};                \
  } while (0)
  RESET();
  s.phase = 1;
  t.mutate_common = t.mutate_first = 1;
  assert(bk_ending_frame_step(&s, work, &input, &ops, e));
  assert(t.count == 3 && s.phase == 9 && s.group == 4);
  assert(t.calls[1].operation == BK_ENDING_STAGE_48D8E9 &&
         t.calls[2].operation == BK_ENDING_STAGE_494015);
  before[4][5] = 1;
  assert(!memcmp(work, before, sizeof(work)));
  for (unsigned i = 0; i < 3; i++)
    assert(t.calls[i].with_input &&
           !memcmp(&input, &t.calls[i].input, sizeof(input)));
  /* Child failure stops the frame after the flag write, never rolls it back
   * and never silently continues into the next controller/camera. */
  RESET();
  s.phase = 5;
  t.fail_at = 2;
  assert(!bk_ending_frame_step(&s, work, &input, &ops, e));
  assert(t.count == 2 && work[0][2] == 1 &&
         !strcmp(e, "missing recovered child"));
  RESET();
  s.phase = 3;
  t.fail_at = 1;
  assert(!bk_ending_frame_step(&s, work, &input, &ops, e));
  assert(t.count == 1 && !memcmp(work, before, sizeof(work)));
  /* Strictly greater than30000; two independent timestamp reads. */
  RESET();
  s.phase = 7;
  s.finish_fade_stage = 3;
  s.finish_elapsed = 29999;
  s.previous_clock = 5;
  t.time[0] = 6;
  t.time[1] = 9;
  assert(bk_ending_frame_step(&s, work, &input, &ops, e));
  assert(s.finish_elapsed == 30000 && !s.curtain_wanted &&
         s.previous_clock == 9);
  t.clocks = t.count = t.keys = 0;
  t.time[0] = 10;
  t.time[1] = 12;
  assert(bk_ending_frame_step(&s, work, &input, &ops, e));
  assert(s.transition_action == 7 && s.curtain_wanted == 1 &&
         !s.finish_elapsed && !s.previous_clock && !s.clock_sample &&
         t.clocks == 2 && t.last_key == 0);
  t.count = t.keys = t.clocks = 0;
  ops.clock = NULL;
  assert(bk_ending_frame_step(&s, work, &input, &ops, e));
  assert(!t.clocks && t.keys == 1);
  RESET();
  s.phase = 7;
  s.finish_fade_stage = 3;
  s.previous_clock = 0xfffffffc;
  t.time[0] = 3;
  t.time[1] = 4;
  assert(bk_ending_frame_step(&s, work, &input, &ops, e));
  assert(s.finish_elapsed == 7 && s.previous_clock == 4);
  RESET();
  s.phase = 7;
  s.finish_fade_stage = 3;
  t.time[0] = 0xf0000000;
  t.time[1] = 33;
  assert(bk_ending_frame_step(&s, work, &input, &ops, e));
  assert(!s.finish_elapsed && s.previous_clock == 33);
  RESET();
  s.phase = 7;
  s.finish_fade_stage = 3;
  ops.clock = NULL;
  assert(!bk_ending_frame_step(&s, work, &input, &ops, e) && t.count == 1);
  RESET();
  s.phase = 8;
  s.camera_mode = 1;
  s.camera_request = -7;
  s.camera_cached = 8;
  s.camera_event = 9;
  assert(bk_ending_frame_step(&s, work, &input, &ops, e));
  assert(t.keys == 1 && t.last_key == 0x70 && s.camera_request == -7 &&
         s.camera_cached == 8 && s.camera_event == 9);
  assert(t.calls[3].operation == BK_ENDING_CAMERA_4E1711);
  assert(!memcmp(work, before, sizeof(work)));
  RESET();
  s.phase = 8;
  ops.key = NULL;
  assert(!bk_ending_frame_step(&s, work, &input, &ops, e));
  assert(t.count == 3 && !t.keys && !memcmp(work, before, sizeof(work)));
  RESET();
  s.phase = 2;
  s.camera_mode = 1;
  s.camera_request = -7;
  s.camera_cached = 8;
  s.camera_event = 9;
  assert(bk_ending_frame_step(&s, work, &input, &ops, e));
  assert(s.camera_request == 1 && s.camera_cached == -1 && !s.camera_event);
  for (unsigned done = 0; done < 2; done++) {
    RESET();
    s.phase = 9;
    t.result = done;
    assert(bk_ending_frame_step(&s, work, &input, &ops, e));
    assert(t.count == 1 && t.calls[0].operation == BK_ENDING_STAGE_4E2223);
    RESET();
    s.camera_mode = 2;
    s.camera_clip = 3;
    s.group = 4;
    s.camera_values[0] = 0x7fc01234; /* opaque bits are not evaluated here */
    s.camera_table[4][3] = 0xffffffff;
    t.result = done ? 1 : 256;
    assert(bk_ending_frame_step(&s, work, &input, &ops, e));
    assert(s.camera_mode == (done ? 0 : 2));
    assert(t.calls[1].args[2] == 0x7fc01234 &&
           t.calls[1].args[5] == 0xffffffff);
  }
  RESET();
  s.camera_mode = 2;
  s.camera_clip = 4;
  assert(!bk_ending_frame_step(&s, work, &input, &ops, e) && t.count == 1);
  RESET();
  s.group = 5;
  assert(!bk_ending_frame_step(&s, work, &input, &ops, e) && !t.count);
  RESET();
  ops.invoke = NULL;
  assert(!bk_ending_frame_step(&s, work, &input, &ops, e));
  puts("PASS ending dispatch:live state/latched branch,flag ordering,child "
       "failure,strict timer/wrap,phase8/9 camera differences,raw argument "
       "bits,bounds");
}
