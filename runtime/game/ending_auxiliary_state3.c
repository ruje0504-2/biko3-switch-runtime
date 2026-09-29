#include "game/ending_auxiliary_state3.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static int fail(char e[256], const char *why) {
  if (e)
    snprintf(e, 256, "ending auxiliary state3: %s", why);
  return 0;
}

int bk_ending_auxiliary_state3_step(
    const BkEndingAuxiliaryState3Bindings *b, const BkEndingFrameInput *input,
    const BkEndingAuxiliaryState3Ops *o, char e[256]) {
  if (!b || !b->frame || !b->control || !b->auxiliary ||
      !b->point_7220c8 || !b->menu_width_7389e8 || !input || !o ||
      !o->child || !o->key || !o->hit || !o->expression || !o->request ||
      !o->voice || b->control->state_721eec != 3 || b->frame->group >= 5)
    return fail(e, "invalid state3 bindings/services");

  /* 47E32E: 481EA5 runs before the parent reads the copied input words. */
  if (!o->child(o->context, input, e))
    return 0;

  uint32_t value = 0;
  if (!o->key(o->context, 0, 3, &value, e))
    return 0;
  if (!value)
    return 1;

  int32_t raw_x, raw_y;
  memcpy(&raw_x, &input->words[9], sizeof(raw_x));
  memcpy(&raw_y, &input->words[10], sizeof(raw_y));
  float pointer[2] = {(float)raw_x, (float)raw_y};
  float center[2] = {(float)b->point_7220c8[0],
                     (float)b->point_7220c8[1]};
  float radius = (float)((double)*b->menu_width_7389e8 / 2.0);
  int hit = 0;
  if (!isfinite(pointer[0]) || !isfinite(pointer[1]) ||
      !isfinite(center[0]) || !isfinite(center[1]) || isnan(radius) ||
      radius < 0)
    return fail(e, "state3 circle geometry is invalid");
  if (!o->hit(o->context, center, radius, pointer, &hit, e))
    return 0;
  if (hit) {
    b->control->state_721eec = 8;
    b->auxiliary->index = -1;
    b->frame->camera_mode = 0;
    return 1;
  }

  if (b->frame->group == 2)
    b->auxiliary->index = 0x48;
  b->frame->camera_mode = 0;
  if (!o->expression(o->context, 7, 3, 1, e) ||
      !o->request(o->context, 1, e) ||
      !o->voice(o->context, 1, 0, 0, o->voice_volume, e))
    return 0;
  b->auxiliary->pending = 1;
  b->control->state_721eec = 1;
  b->frame->camera_cached = -1;
  return 1;
}
