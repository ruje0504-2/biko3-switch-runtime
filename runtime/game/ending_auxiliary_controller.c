#include "game/ending_auxiliary_controller.h"
#include <string.h>
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
      !o->voice || !o->camera_setup || !o->target || !b->delay_54f8e0 ||
      !b->saved_toggle_6bbe4c ||
      b->frame->group >= 5)
    return fail(e, "invalid state-8 bindings/services");
  if (b->control->state_721eec != 8)
    return fail(e, "state-8 entry called outside 721EEC state 8");

  int playing;
  if (!o->status(o->context, 0, &playing, e)) return 0;
  if (playing) return 1;
  if (!o->status(o->context, 1, &playing, e)) return 0;
  if (playing) return 1;

  unsigned group = b->frame->group;
  int32_t expression_a, expression_b;
  unsigned eye;
  int32_t next_state;
  if (group == 2) {
    expression_a = 0;
    expression_b = 13;
    eye = 1;
    next_state = 5;
  } else if (group == 3 || group == 4) {
    expression_a = 3;
    expression_b = 4;
    eye = 0;
    next_state = group == 4 ? 6 : 5;
  } else {
    expression_a = 6;
    expression_b = 3;
    eye = 1;
    next_state = 7;
  }

  if (!o->request(o->context, 5, e) ||
      !o->expression(o->context, expression_a, expression_b, eye, e))
    return 0;
  *b->delay_54f8e0 = 0;

  /* 47DC79 writes these process fields before its group-specific sound and
   * table setup. Keep the prefix visible if a later service fails. */
  b->control->state_721eec = next_state;
  b->auxiliary->index = -1;
  b->frame->camera_cached = -1;
  if (!o->voice(o->context, 7, 0, 0, o->voice_volume, e))
    return 0;

  /* Native group4 returns immediately after cue7. It does not touch the
   * camera table, target choice, or camera mode. */
  if (group == 4)
    return 1;

  static const float group01_camera[2][4] = {
      {77.8300018f, -33.6100006f, 85.0699997f, 2.24f},
      {328.0f, 2.0f, 28.0f, 5.0f}};
  static const float group234_camera[5][4] = {
      {140.0f, 9.0f, 120.0f, -3.5f},
      {0.0f, 6.0f, 108.0f, -1.0f},
      {112.0f, 21.0f, 80.0f, 2.0f},
      {328.0f, 26.0f, 59.0f, 1.0f},
      {345.0f, 2.0f, 120.0f, -3.0f}};

  if (group < 2 && !o->group_sound(o->context, group, e))
    return 0;
  if (group < 2) {
    *b->saved_toggle_6bbe4c = b->control->toggles[1];
    b->control->toggles[1] = 1;
  }
  const float *values = group < 2 ? group01_camera[group]
                                  : group234_camera[group];
  if (!o->camera_setup(o->context, group, values, group >= 2, e))
    return 0;
  float target[3];
  if (!o->target(o->context, target, e))
    return 0;
  memcpy(b->frame->camera_values, target, sizeof(target));
  b->control->target_choice = 1;
  b->frame->camera_clip = 0;
  b->frame->camera_table[group][0] = 0x40000000u;
  b->frame->camera_mode = group < 2 ? 4 : 2;
  return 1;
}
