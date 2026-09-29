#include "game/ending_auxiliary_state4.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static int fail(char e[256], const char *why) {
  if (e)
    snprintf(e, 256, "ending auxiliary state4: %s", why);
  return 0;
}

static int decrease_fov(BkEndingAuxiliaryState4Bindings *b, float seconds) {
  /* Native uses x87 subtraction with the binary32 .2 constant. */
  *b->fov = (float)((double)*b->fov - (double)seconds * (double).2f);
  if (!(*b->fov > .2f)) {
    *b->fov = .2f;
    return 1;
  }
  return 0;
}

static int target(BkEndingAuxiliaryState4Bindings *b,
                  const BkEndingAuxiliaryState4Ops *o, char e[256]) {
  float position[3];
  if (!o->target || !o->target(o->context, position, e))
    return 0;
  memcpy(b->frame->camera_values, position, sizeof(position));
  return 1;
}

static int camera(BkEndingAuxiliaryState4Bindings *b,
                  const BkEndingAuxiliaryState4Ops *o,
                  BkEndingOpeningCamera kind, uint32_t *result,
                  char e[256]) {
  if (!o->camera)
    return fail(e, "missing camera service");
  uint32_t offset[3] = {0};
  int32_t choice = 0;
  uint32_t extra = 0;
  if (kind == BK_ENDING_OPENING_PRESET) {
    if (b->frame->group >= 5 || b->frame->camera_clip < 0 ||
        b->frame->camera_clip >= 4)
      return fail(e, "camera table index outside recovered state4 range");
    choice = b->frame->camera_clip;
    memcpy(offset, b->frame->camera_values, sizeof(offset));
    extra = b->frame->camera_table[b->frame->group][3];
  }
  return o->camera(o->context, kind, choice, offset, extra, result, e);
}

static int media_present(const BkEndingAuxiliaryState4Ops *o, unsigned owner,
                         int *present, char e[256]) {
  if (!o->present)
    return fail(e, "missing media presence service");
  return o->present(o->context, owner, present, e);
}

static int media_busy(const BkEndingAuxiliaryState4Ops *o, unsigned owner,
                      int *busy, char e[256]) {
  if (!media_present(o, owner, busy, e))
    return 0;
  if (!*busy)
    return 1;
  if (!o->status)
    return fail(e, "missing media status service");
  return o->status(o->context, owner, busy, e);
}

int bk_ending_auxiliary_state4_step(
    const BkEndingAuxiliaryState4Bindings *bindings, float seconds,
    const BkEndingAuxiliaryState4Ops *o, char e[256]) {
  if (!bindings || !bindings->frame || !bindings->control ||
      !bindings->auxiliary || !bindings->camera || !bindings->substate ||
      !bindings->fov || !bindings->previous_flow || !o ||
      !isfinite(seconds) || seconds < 0 ||
      bindings->control->state_721eec != 4)
    return fail(e, "invalid state4 bindings/time");

  switch (*bindings->substate) {
  case 0:
    if (!o->prepare_actor || !o->expression)
      return fail(e, "missing state4 actor service");
    bindings->auxiliary->index = 0x48;
    if (!o->prepare_actor(o->context, e))
      return 0;
    if (*bindings->previous_flow == 0x18) {
      bindings->camera->fov = *bindings->fov;
      int done = decrease_fov((BkEndingAuxiliaryState4Bindings *)bindings,
                              seconds);
      (void)done;
      uint32_t complete = 0;
      if (!camera((BkEndingAuxiliaryState4Bindings *)bindings, o,
                  BK_ENDING_OPENING_TRACK, &complete, e))
        return 0;
      if (complete & 255u) {
        bindings->camera->fov = .2f;
        *bindings->fov = 1;
        if (!o->expression(o->context, 7, 3, 1, e))
          return 0;
        *bindings->substate = 1;
        memcpy(bindings->control->saved_camera, bindings->camera->matrix,
               sizeof(bindings->control->saved_camera));
        if (!target((BkEndingAuxiliaryState4Bindings *)bindings, o, e))
          return 0;
      }
    } else if (*bindings->previous_flow == 8) {
      *bindings->substate = 4;
    }
    return 1;

  case 1: {
    bindings->camera->fov = *bindings->fov;
    int done = decrease_fov((BkEndingAuxiliaryState4Bindings *)bindings,
                            seconds);
    uint32_t complete = 0;
    if (!camera((BkEndingAuxiliaryState4Bindings *)bindings, o,
                BK_ENDING_OPENING_PRESET, &complete, e))
      return 0;
    if (complete & 255u) {
      *bindings->substate = 2;
      bindings->camera->fov = .2f;
      if (!o->effect || !o->effect(o->context, 1, 0, 0, e))
        return 0;
      bindings->auxiliary->pending = 1;
      bindings->frame->camera_mode = 0;
    }
    (void)done;
    return 1;
  }

  case 2: {
    int busy = 0;
    if (!media_busy(o, 0, &busy, e))
      return 0;
    if (busy)
      return 1;
    int present = 0;
    if (!media_present(o, 1, &present, e))
      return 0;
    if (!present) {
      if (bindings->control->toggles[7]) {
        if (!o->effect || !o->effect(o->context, 2, 1, 0, e))
          return 0;
        return 1;
      }
      *bindings->substate = 3;
      return 1;
    }
    if (!media_busy(o, 1, &busy, e))
      return 0;
    if (!busy)
      *bindings->substate = 3;
    return 1;
  }

  case 3: {
    int present = 0, busy = 0;
    if (!media_present(o, 1, &present, e))
      return 0;
    if (present && !media_busy(o, 1, &busy, e))
      return 0;
    if (!present || !busy) {
      bindings->control->state_721eec = 0;
      *bindings->substate = 0;
    }
    return 1;
  }

  case 4: {
    if (!target((BkEndingAuxiliaryState4Bindings *)bindings, o, e))
      return 0;
    bindings->camera->fov = *bindings->fov;
    int done = decrease_fov((BkEndingAuxiliaryState4Bindings *)bindings,
                            seconds);
    uint32_t complete = 0;
    if (!camera((BkEndingAuxiliaryState4Bindings *)bindings, o,
                BK_ENDING_OPENING_PRESET, &complete, e))
      return 0;
    if (done && (complete & 255u)) {
      *bindings->substate = 2;
      *bindings->fov = 1;
      bindings->camera->fov = .2f;
      if (!o->effect || !o->effect(o->context, 1, 0, 0, e))
        return 0;
      bindings->auxiliary->pending = 1;
      bindings->frame->camera_mode = 0;
    }
    return 1;
  }
  default:
    return 1;
  }
}
