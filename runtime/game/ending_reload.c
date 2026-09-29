#include "game/ending_reload.h"
#include <stdio.h>
#include <string.h>
static int fail(char e[256], const char *why) {
  snprintf(e, 256, "ending reload: %s", why);
  return 0;
}
static int bound(const BkEndingReloadBindings *b, const BkEndingReloadOps *o,
                 char e[256]) {
  return (b && b->frame && b->control && b->auxiliary && b->previous_flow &&
          b->action && b->curtain_wanted && b->selected && b->next_mode &&
          b->saved_toggles && o) ||
         fail(e, "missing live bindings/services");
}
const char *bk_ending_final_image(unsigned group) {
  static const char *const names[] = {"g01_20.bmp", "g02_20.bmp", "g03_20.bmp",
                                      "g04_20.bmp", "g05_20.bmp"};
  return group < 5 ? names[group] : NULL;
}
static int load(const BkEndingReloadOps *o, BkEndingLoader kind, int32_t arg,
                char e[256]) {
  return o->load ? o->load(o->context, kind, arg, e)
                 : fail(e, "missing resource loader");
}
static int release(const BkEndingReloadOps *o, BkEndingLoader kind,
                   char e[256]) {
  return o->release ? o->release(o->context, kind, e)
                    : fail(e, "missing resource destructor");
}
static int final_image(const BkEndingReloadBindings *b,
                       const BkEndingReloadOps *o, int create, char e[256]) {
  if (create && !bk_ending_final_image(b->frame->group))
    return fail(e, "final image group outside table");
  return o->final_image ? o->final_image(o->context, create,
                                         create ? b->frame->group : 0, e)
                        : fail(e, "missing final image owner");
}
int bk_ending_reload_load(const BkEndingReloadBindings *b,
                          const BkEndingReloadOps *o, char e[256]) {
  if (!bound(b, o, e))
    return 0;
  switch (b->frame->phase) {
  case 1:
    return load(o, BK_ENDING_LOAD_4CF318, -1, e);
  case 2:
    return load(o, BK_ENDING_LOAD_4D00FA, -1, e);
  case 3:
    return load(o, BK_ENDING_LOAD_4D2320, -1, e);
  case 4:
    return load(o, BK_ENDING_LOAD_4D39E6, -1, e);
  case 5:
  case 6:
    return load(o, BK_ENDING_LOAD_4D1025, b->auxiliary->selection, e);
  case 7:
    return final_image(b, o, 1, e);
  case 8:
    if (*b->action == 7 || *b->action == 0x3f) {
      if (!load(o, BK_ENDING_LOAD_4D1025, b->auxiliary->selection, e))
        return 0;
      b->frame->state_721ee0 = 6;
    } else if (*b->action == 6) {
      if (*b->next_mode != 0) {
        if (!load(o, BK_ENDING_LOAD_4CF318, -1, e))
          return 0;
        b->frame->state_721ee0 = 4;
      } else {
        if (!load(o, BK_ENDING_LOAD_4D00FA, -1, e))
          return 0;
        b->frame->state_721ee0 = 5;
      }
    } else if (*b->action == 8) {
      if (!load(o, BK_ENDING_LOAD_4D39E6, -1, e))
        return 0;
      b->frame->state_721ee0 = 8;
    }
    break;
  }
  return 1;
}
static int playing(const BkEndingReloadOps *o, unsigned slot, int *out,
                   char e[256]) {
  int present = 0;
  if (!o->present)
    return fail(e, "missing sound presence query");
  if (!o->present(o->context, slot, &present, e))
    return 0;
  if (present != 0 && present != 1)
    return fail(e, "invalid sound presence");
  *out = 0;
  if (!present)
    return 1;
  if (!o->status)
    return fail(e, "missing sound status query");
  if (!o->status(o->context, slot, out, e))
    return 0;
  return (*out == 0 || *out == 1) || fail(e, "invalid sound status");
}
int bk_ending_reload_release(const BkEndingReloadBindings *b,
                             const BkEndingReloadOps *o, char e[256]) {
  if (!bound(b, o, e))
    return 0;
  for (unsigned i = 2; i < 47; ++i) {
    int active;
    if (!playing(o, i, &active, e))
      return 0;
    if (active) {
      if (!o->pause)
        return fail(e, "missing effect pause service");
      if (!o->pause(o->context, i, e))
        return 0;
    }
  }
  /*Phase is deliberately read AFTER sound callbacks. */
  switch (b->frame->phase) {
  case 1:
    return release(o, BK_ENDING_LOAD_4CF318, e);
  case 2:
    return release(o, BK_ENDING_LOAD_4D00FA, e);
  case 3:
    return release(o, BK_ENDING_LOAD_4D2320, e);
  case 4:
    return release(o, BK_ENDING_LOAD_4D39E6, e);
  case 5:
  case 6:
    return release(o, BK_ENDING_LOAD_4D1025, e);
  case 7:
    return final_image(b, o, 0, e);
  case 8:
    if (*b->action == 7) {
      if (b->auxiliary->variant == 0)
        return release(o, BK_ENDING_LOAD_4CF318, e);
      if (b->frame->state_721ee4 == 1000)
        return release(o, BK_ENDING_LOAD_4D39E6, e);
      if (b->frame->state_721ee4 == 1500)
        return release(o, BK_ENDING_LOAD_4D2320, e);
    } else if (*b->action == 6)
      return release(
          o, *b->next_mode ? BK_ENDING_LOAD_4D00FA : BK_ENDING_LOAD_4CF318, e);
    else if (*b->action == 0x3f)
      return release(o, BK_ENDING_LOAD_4D1025, e);
    else if (*b->action == 8)
      return release(o, BK_ENDING_LOAD_4D2320, e);
    break;
  }
  return 1;
}
static int light(const BkEndingReloadOps *o, BkEndingReloadLight op,
                 char e[256]) {
  return o->lighting ? o->lighting(o->context, op, e)
                     : fail(e, "missing lighting service");
}
static int relight(const BkEndingReloadOps *o, char e[256]) {
  return light(o, BK_ENDING_LIGHT_RESET, e) &&
         light(o, BK_ENDING_LIGHT_SELECT_BK3_L, e) &&
         light(o, BK_ENDING_LIGHT_ENABLE, e);
}
static int schedule(const BkEndingReloadOps *o, uint8_t target, char e[256]) {
  return o->schedule ? o->schedule(o->context, target, 0, e)
                     : fail(e, "missing transition scheduler");
}
static int leave(const BkEndingReloadOps *o, char e[256]) {
  if (!o->leave)
    return fail(e, "missing retained-state reset services");
  for (unsigned i = 0; i < 6; ++i)
    if (!o->leave(o->context, (BkEndingLeave)i, e))
      return 0;
  return 1;
}
static void save(const BkEndingReloadBindings *b, int second) {
  b->saved_toggles[0] = b->control->toggles[3];
  b->saved_toggles[1] = b->control->toggles[5];
  if (second)
    b->saved_toggles[2] = b->control->toggles[1];
  b->saved_toggles[3] = b->control->toggles[0];
}
static void restore(const BkEndingReloadBindings *b, int second) {
  b->control->toggles[3] = b->saved_toggles[0];
  b->control->toggles[5] = b->saved_toggles[1];
  if (second)
    b->control->toggles[1] = b->saved_toggles[2];
  b->control->toggles[0] = b->saved_toggles[3];
}
static int unload(const BkEndingReloadBindings *b, const BkEndingReloadOps *o,
                  char e[256]) {
  return bk_ending_reload_release(b, o, e) &&
         light(o, BK_ENDING_LIGHT_RESET, e);
}
static int load_gallery(const BkEndingReloadBindings *b,
                        const BkEndingReloadOps *o, char e[256]) {
  if (!bk_ending_reload_load(b, o, e))
    return 0;
  if (*b->previous_flow == 0x18)
    b->frame->phase = 8;
  return 1;
}
int bk_ending_reload_transition(const BkEndingReloadBindings *b,
                                uint8_t curtain_stage,
                                const BkEndingReloadOps *o, int *early,
                                char e[256]) {
  if (!bound(b, o, e) || !early)
    return fail(e, "invalid transition bindings");
  *early = 0;
  if (*b->curtain_wanted != 1 || curtain_stage != 3)
    return 1;
  *b->curtain_wanted = 0;
  switch (*b->action) {
  case 0x2f:
    if (!schedule(o, 0x58, e))
      return 0;
    *b->action = 0;
    break;
  case 0x2d:
    if (!schedule(o, 1, e) || !leave(o, e))
      return 0;
    memset(b->saved_toggles, 0, 4);
    *b->action = 0;
    break;
  case 7:
    if (b->auxiliary->variant == 0) {
      save(b, 0);
      if (!unload(b, o, e))
        return 0;
      if (*b->previous_flow != 0x18) {
        b->frame->phase = 5;
        *b->selected = 0;
      }
      if (!load_gallery(b, o, e))
        return 0;
      restore(
          b,
          1); /* Retained saved[2], even though this branch did not set it. */
    } else {
      save(b, b->frame->group == 2);
      if (!unload(b, o, e))
        return 0;
      if (*b->previous_flow != 0x18) {
        b->frame->phase = 6;
        *b->selected = 6;
      }
      if (!load_gallery(b, o, e))
        return 0;
      restore(b, b->frame->group == 2);
    }
    if (!relight(o, e))
      return 0;
    *b->action = 0;
    break;
  case 6:
    if (*b->next_mode != 0) {
      save(b, 1);
      if (!unload(b, o, e))
        return 0;
      if (*b->previous_flow != 0x18) {
        b->frame->phase = 1;
        *b->selected = 1;
      }
      b->control->variant = 0;
    } else {
      save(b, 0);
      if (!unload(b, o, e))
        return 0;
      if (*b->previous_flow != 0x18) {
        b->frame->phase = 2;
        *b->selected = 0;
      }
      b->control->variant = 1;
    }
    if (!load_gallery(b, o, e))
      return 0;
    restore(b, 0);
    if (!relight(o, e))
      return 0;
    *b->action = 0;
    break;
  case 0x3f: {
    int active;
    if (!playing(o, 0, &active, e))
      return 0;
    if (active) {
      *b->curtain_wanted = 1;
      break;
    }
    save(b, 1);
    if (!unload(b, o, e) || !bk_ending_reload_load(b, o, e))
      return 0;
    restore(b, 1);
    if (!relight(o, e))
      return 0;
    *b->action = 0;
    break;
  }
  case 8:
    save(b, b->frame->group == 2);
    if (!unload(b, o, e))
      return 0;
    if (*b->previous_flow != 0x18)
      b->frame->phase = 4;
    b->control->variant = 6;
    *b->selected = 5;
    if (!load_gallery(b, o, e))
      return 0;
    restore(b, b->frame->group == 2);
    if (!relight(o, e))
      return 0;
    *b->action = 0;
    break;
  case 0x4a:
    save(b, 0);
    if (!unload(b, o, e))
      return 0;
    b->frame->phase = 7;
    *b->selected = 0;
    if (!bk_ending_reload_load(b, o, e))
      return 0;
    restore(b, 1);
    *b->action = 0;
    break;
  case 0x31:
    if (!leave(o, e))
      return 0;
    b->control->variant = 255;
    *b->selected = -1;
    memset(b->saved_toggles, 0, 4);
    if (*b->previous_flow == 0x18) {
      if (!schedule(o, 0x18, e))
        return 0;
    } else if (*b->previous_flow == 8) {
      if (!schedule(o, 8, e))
        return 0;
    }
    *b->action = 0;
    *early = 1;
    break;
  }
  return 1;
}
