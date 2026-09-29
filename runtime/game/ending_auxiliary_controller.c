#include "game/ending_auxiliary_controller.h"
#include <stdio.h>

static int fail(char e[256], const char *why) {
  if (e) snprintf(e, 256, "auxiliary controller: %s", why);
  return 0;
}

int bk_ending_auxiliary_controller_begin(
    const BkEndingAuxiliaryControllerBindings *b,
    const BkEndingAuxiliaryControllerOps *o, char e[256]) {
  if (!b || !b->frame || !b->control || !b->auxiliary || !o ||
      !o->status || !o->request || !o->expression || !o->group_sound ||
      !o->prepare_group || b->frame->group >= 5)
    return fail(e, "invalid state-4 bindings/services");
  if (b->control->state_721eec != 4)
    return fail(e, "state-4 entry called outside 721EEC state 4");

  int playing;
  if (!o->status(o->context, 0, &playing, e)) return 0;
  if (playing) return 1;
  if (!o->status(o->context, 1, &playing, e)) return 0;
  if (playing) return 1;

  unsigned group = b->frame->group;
  unsigned branch;
  int32_t expression_a, expression_b;
  unsigned eye;
  int32_t next_state;
  int32_t next_camera_mode;
  if (group == 2) {
    branch = 2;
    expression_a = 0;
    expression_b = 13;
    eye = 1;
    next_state = 5;
    next_camera_mode = 2;
  } else if (group == 3 || group == 4) {
    branch = group;
    expression_a = 3;
    expression_b = 4;
    eye = 0;
    next_state = group == 4 ? 6 : 5;
    next_camera_mode = 2;
  } else {
    branch = group;
    expression_a = 6;
    expression_b = 3;
    eye = 1;
    next_state = 7;
    next_camera_mode = 4;
  }

  if (!o->request(o->context, 5, e) ||
      !o->expression(o->context, expression_a, expression_b, eye, e))
    return 0;

  /* 47DC79 writes these process fields before its group-specific sound and
   * table setup. Keep the prefix visible if a later service fails. */
  b->control->state_721eec = next_state;
  b->auxiliary->index = -1;
  b->frame->camera_cached = -1;

  if ((group < 2 && !o->group_sound(o->context, group, e)) ||
      !o->prepare_group(o->context, group, branch, e))
    return 0;
  b->frame->camera_mode = next_camera_mode;
  return 1;
}
