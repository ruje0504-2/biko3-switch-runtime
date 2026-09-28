#include "game/ending_frame.h"
#include <stdio.h>
#include <string.h>
static int fail(char e[256], const char *why) {
  snprintf(e, 256, "ending frame: %s", why);
  return 0;
}
static int call(const BkEndingFrameOps *ops, BkEndingOperation operation,
                const BkEndingFrameInput *input, unsigned count,
                const uint32_t *args, uint32_t *result, char e[256]) {
  BkEndingCall c = {
      .operation = operation, .with_input = input != NULL, .count = count};
  if (input)
    c.input = *input;
  if (count)
    memcpy(c.args, args, count * sizeof(*args));
  uint32_t unused = 0;
  return ops->invoke(ops->context, &c, result ? result : &unused, e);
}
static int key(const BkEndingFrameOps *ops, unsigned code, int *pressed,
               char e[256]) {
  if (!ops->key)
    return fail(e, "missing key service");
  if (!ops->key(ops->context, code, pressed, e))
    return 0;
  if (*pressed != 0 && *pressed != 1)
    return fail(e, "invalid key result");
  return 1;
}
static int camera(BkEndingFrameState *s, const BkEndingFrameOps *ops,
                  char e[256]) {
  if (s->phase == 9)
    return 1;
  int special = s->phase == 8;
  uint32_t args[6] = {BK_ENDING_PLAYER};
  BkEndingOperation op;
  unsigned count = 0;
  if (s->camera_mode == 0 &&
      (special || !((s->state_721ee0 == 3 || s->state_721ee0 == 4) &&
                    s->state_721ee4 == 4))) {
    op = BK_ENDING_CAMERA_4BB0A4;
    memcpy(args + 1, s->camera_values, sizeof(s->camera_values));
    count = 4;
  } else if (s->camera_mode == 1) {
    if (!special && s->camera_manual == 1) {
      op = BK_ENDING_CAMERA_4E1D16;
      count = 4;
    } else {
      if (!special) {
        s->camera_request = 1;
        s->camera_cached = -1;
        s->camera_event = 0;
      }
      op = BK_ENDING_CAMERA_4E1711;
      count = 1;
    }
  } else if (s->camera_mode == 2) {
    if (s->group >= 5 || s->camera_clip < 0 || s->camera_clip >= 4)
      return fail(e, "camera table index outside recovered row");
    op = BK_ENDING_CAMERA_4E0ECB;
    args[1] = (uint32_t)s->camera_clip;
    memcpy(args + 2, s->camera_values, sizeof(s->camera_values));
    args[5] = s->camera_table[s->group][s->camera_clip];
    count = 6;
  } else if (!special && s->camera_mode == 3) {
    op = BK_ENDING_CAMERA_4BC444;
    args[1] = (uint32_t)s->camera_clip;
    memcpy(args + 2, s->camera_values, sizeof(s->camera_values));
    count = 5;
  } else if (s->camera_mode == 4) {
    op = BK_ENDING_CAMERA_4DF411;
    memcpy(args + 1, s->camera_values, sizeof(s->camera_values));
    count = 4;
  } else
    return 1;
  uint32_t result = 0;
  if (!call(ops, op, NULL, count, args, &result, e))
    return 0;
  if ((op == BK_ENDING_CAMERA_4E0ECB || op == BK_ENDING_CAMERA_4BC444) &&
      (result & 255))
    s->camera_mode = 0;
  return 1;
}
int bk_ending_frame_step(BkEndingFrameState *s, uint8_t working[5][8],
                         const BkEndingFrameInput *input,
                         const BkEndingFrameOps *ops, char e[256]) {
  if (!s || !working || !input || !ops || !ops->invoke || s->group >= 5)
    return fail(e, "invalid state/services/input");
  if (s->phase != 9 &&
      !call(ops, BK_ENDING_COMMON_4D7AC4, input, 0, NULL, NULL, e))
    return 0;
  /* The common callback can change phase/group before this dispatch. */
  if (s->phase >= 1 && s->phase <= 6) {
    if (s->group >= 5)
      return fail(e, "working progress group outside table");
    static const unsigned flag[] = {0, 1, 3, 4, 2, 5};
    static const BkEndingOperation first[] = {
        BK_ENDING_STAGE_4DB608, BK_ENDING_STAGE_47A5D0, BK_ENDING_STAGE_476720,
        BK_ENDING_STAGE_47DC79, BK_ENDING_STAGE_48D8E9, BK_ENDING_STAGE_48D8E9};
    static const BkEndingOperation second[] = {
        BK_ENDING_STAGE_4DF6C0, BK_ENDING_STAGE_47D3CB, BK_ENDING_STAGE_479137,
        BK_ENDING_STAGE_48181F, BK_ENDING_STAGE_494015, BK_ENDING_STAGE_494015};
    unsigned branch = (unsigned)s->phase - 1;
    working[s->group][flag[branch]] = 1;
    uint32_t args[] = {BK_ENDING_CONTROL, BK_ENDING_PRIMARY,
                       BK_ENDING_SECONDARY};
    /*4d763f has the two secondary pointers in reversed order. */
    if (branch == 2) {
      args[1] = BK_ENDING_SECONDARY;
      args[2] = BK_ENDING_PRIMARY;
    }
    if (!call(ops, first[branch], input, branch ? 1 : 0, args, NULL, e) ||
        !call(ops, second[branch], input, branch ? 3 : 0, args, NULL, e))
      return 0;
  } else if (s->phase == 7) {
    if (s->finish_fade_stage == 3 && !s->finish_blocked) {
      if (s->transition_action != 7) {
        if (!ops->clock)
          return fail(e, "missing end-wait clock");
        if (!ops->clock(ops->context, &s->clock_sample, e))
          return 0;
        /* Original unsigned compare against0 cannot detect wrap/underflow.
         * Use modulo32 arithmetic; only previous==0 suppresses elapsed. */
        uint32_t elapsed =
            s->previous_clock ? s->clock_sample - s->previous_clock : 0;
        s->finish_elapsed += elapsed;
        if (!ops->clock(ops->context, &s->previous_clock, e))
          return 0;
      }
      int pressed;
      if (!key(ops, 0, &pressed, e))
        return 0;
      if (pressed || s->finish_elapsed > 30000) {
        s->finish_elapsed = s->clock_sample = s->previous_clock = 0;
        s->curtain_wanted = 1;
        s->transition_action = 7;
      }
    }
  } else if (s->phase == 8) {
    const uint32_t args[] = {BK_ENDING_CONTROL, BK_ENDING_PRIMARY,
                             BK_ENDING_SECONDARY};
    int ignored;
    if (!call(ops, BK_ENDING_STAGE_48302B, input, 0, NULL, NULL, e) ||
        !call(ops, BK_ENDING_STAGE_48BCBB, NULL, 3, args, NULL, e) ||
        !key(ops, 0x70, &ignored, e))
      return 0;
  } else if (s->phase == 9 &&
             !call(ops, BK_ENDING_STAGE_4E2223, input, 0, NULL, NULL, e))
    return 0;
  /* Both tests use live state after the selected child controllers. */
  if (s->auxiliary_mode == 3 &&
      !call(ops, BK_ENDING_AUXILIARY_4965B9, NULL, 0, NULL, NULL, e))
    return 0;
  return camera(s, ops, e);
}
