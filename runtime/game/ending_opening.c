#include "game/ending_opening.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
static int fail(char e[256], const char *why) {
  if (e)
    snprintf(e, 256, "ending opening: %s", why);
  return 0;
}
static int target(const BkEndingOpeningBindings *b,
                   const BkEndingOpeningOps *o, char e[256]) {
  float position[3];
  if (!o->target)
    return fail(e, "missing old target service");
  if (!o->target(o->context, position, e))
    return 0;
  memcpy(b->frame->camera_values, position, sizeof(position));
  return 1;
}
static int camera(const BkEndingOpeningBindings *b,
                   const BkEndingOpeningOps *o, BkEndingOpeningCamera kind,
                   uint32_t *result, char e[256]) {
  if (!o->camera)
    return fail(e, "missing camera service");
  uint32_t offset[3] = {0};
  uint32_t extra = 0;
  int32_t choice = 0;
  if (kind == BK_ENDING_OPENING_PRESET) {
    if (b->frame->group >= 5)
      return fail(e, "camera table group outside original range");
    choice = b->frame->camera_clip;
    memcpy(offset, b->frame->camera_values, sizeof(offset));
    extra = b->frame->camera_table[b->frame->group][3];
  }
  return o->camera(o->context, kind, choice, offset, extra, result, e);
}
static int playing(const BkEndingOpeningOps *o, unsigned slot, int *out,
                    char e[256]) {
  if (!o->present)
    return fail(e, "missing speech presence service");
  int exists = 0;
  if (!o->present(o->context, slot, &exists, e))
    return 0;
  *out = 0;
  if (!exists)
    return 1;
  if (!o->status)
    return fail(e, "missing speech status service");
  return o->status(o->context, slot, out, e);
}
static int speech(const BkEndingOpeningBindings *b,
                    const BkEndingOpeningOps *o, unsigned slot,
                    const char *name, char e[256]) {
  if (!o->load)
    return fail(e, "missing speech load service");
  if (!o->load(o->context, slot, name, e))
    return 0;
  if (!o->play)
    return fail(e, "missing speech play service");
  return o->play(o->context, slot, *b->voice_volume, e);
}
/* Float store after the original x87 product/subtraction, then its <= test.
 * The .2 operand is a binary32 constant, not the double literal0.2. */
static int decrease(BkEndingOpeningRetained *s, float seconds) {
  s->fov = (float)((double)s->fov - (double)seconds * (double).2f);
  int done = !(s->fov > .2f);
  if (done)
    s->fov = .2f;
  return done;
}
int bk_ending_opening_step(const BkEndingOpeningBindings *b, float seconds,
                            const BkEndingOpeningOps *o, char e[256]) {
  if (!b || !b->frame || !b->control || !b->auxiliary || !b->camera ||
      !b->substate || !b->voice_wait || !b->next_mode || !b->previous_flow ||
      !b->voice_volume || !b->speech_name || !b->retained || !o ||
      !isfinite(seconds) || seconds < 0 || b->frame->state_721ee0 != 4)
    return fail(e, "invalid live state4 bindings or time");
  uint32_t result = 0;
  switch (*b->substate) {
  case 0:
    if (*b->previous_flow == 0x18)
      *b->substate = 1;
    else if (*b->previous_flow == 8)
      *b->substate = *b->next_mode ? 4 : 1;
    b->retained->fov = 1;
    b->auxiliary->index = 0;
    break;
  case 1:
    b->camera->fov = b->retained->fov;
    decrease(b->retained, seconds);
    if (!camera(b, o, BK_ENDING_OPENING_TRACK, &result, e))
      return 0;
    if (result & 255u) {
      b->camera->fov = .2f;
      *b->substate = 2;
      memcpy(b->control->saved_camera, b->camera->matrix,
             sizeof(b->control->saved_camera));
      if (!target(b, o, e))
        return 0;
    }
    break;
  case 2:
    if (!camera(b, o, BK_ENDING_OPENING_PRESET, &result, e))
      return 0;
    if (result & 255u) {
      *b->substate = 3;
      b->frame->camera_mode = 0;
      if (!*b->next_mode) {
        char name[32];
        if (b->frame->group >= 5)
          return fail(e, "speech group outside original range");
        snprintf(name, sizeof(name), "PH%u0001.wav", b->frame->group + 1u);
        memcpy(b->speech_name, name, strlen(name) + 1);
        if (!speech(b, o, 0, name, e))
          return 0;
        b->auxiliary->pending = 0;
        *b->voice_wait = 1;
        b->camera->fov = .2f;
      }
    }
    break;
  case 3:
    if (*b->voice_wait) {
      int active = 0, exists = 0;
      if (!playing(o, 0, &active, e))
        return 0;
      if (active)
        break;
      if (!o->present(o->context, 1, &exists, e))
        return 0;
      if (!exists && b->control->toggles[7]) {
        char name[32] = {0};
        if (!o->voice_name)
          return fail(e, "missing contact speech selection service");
        if (!o->voice_name(o->context, name, e) ||
            !speech(b, o, 1, name, e))
          return 0;
      } else {
        if (!playing(o, 1, &active, e))
          return 0;
        if (!active) {
          *b->voice_wait = 0;
          b->frame->state_721ee0 = 0;
        }
      }
    } else
      b->frame->state_721ee0 = 0;
    break;
  case 4: {
    if (!target(b, o, e))
      return 0;
    b->camera->fov = b->retained->fov;
    int fov_done = decrease(b->retained, seconds);
    if (!camera(b, o, BK_ENDING_OPENING_PRESET, &result, e))
      return 0;
    if (fov_done && (result & 255u)) {
      b->retained->fov = 1;
      b->camera->fov = .2f;
      *b->substate = 3;
      b->frame->camera_mode = 0;
    }
    break;
  }
  default:
    break;
  }
  return 1;
}
