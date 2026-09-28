#include "game/ending_special.h"
#include "game/ending_special_data.inc"
#include <math.h>
#include <stdio.h>
#include <string.h>
static int fail(char e[256], const char *why) {
  snprintf(e, 256, "ending special: %s", why);
  return 0;
}
int bk_ending_special_cameras(float out[BK_ENDING_SPECIAL_CAMERAS][4],
                              unsigned group) {
  if (!out || group >= 5)
    return 0;
  memcpy(out, camera_bits[group], sizeof(camera_bits[group]));
  return 1;
}
const char *bk_ending_special_hidden_name(unsigned group, unsigned variant,
                                          unsigned slot) {
  return group < 5 && variant < 10 && slot < 3
             ? hidden_names[group][variant][slot]
             : NULL;
}
static int profile(const BkEndingSpecialBindings *b, char e[256]) {
  return (b->frame->group < 5 && *b->camera_variant < 10) ||
         fail(e, "invalid live group/camera variant");
}
static int render(const BkEndingSpecialOps *o, BkEndingSpecialRenderEvent event,
                  unsigned arg, char e[256]) {
  return o->render_event ? o->render_event(o->context, event, arg, e)
                         : fail(e, "missing render event service");
}
static int hide(const BkEndingSpecialOps *o, uint32_t node, uint32_t hidden,
                char e[256]) {
  if (!node)
    return 1;
  return o->hide_node ? o->hide_node(o->context, node, hidden, e)
                      : fail(e, "missing node visibility service");
}
static int find_hide(const BkEndingSpecialBindings *b,
                     const BkEndingSpecialOps *o, const char *name,
                     uint32_t *node, char e[256]) {
  if (!o->find_node || !*b->primary_root)
    return fail(e, "missing primary root/name lookup service");
  if (!o->find_node(o->context, *b->primary_root, name, node, e))
    return 0;
  return hide(o, *node, 1, e);
}
static int reset_camera(const BkEndingSpecialOps *o, char e[256]) {
  static const float zero[3] = {0}, forward[3] = {0, 0, 1}, up[3] = {0, 1, 0};
  if (!o->camera_position || !o->camera_orientation)
    return fail(e, "missing camera setter service");
  return o->camera_position(o->context, zero, e) &&
         o->camera_orientation(o->context, forward, up, e);
}
static int group1_first(const BkEndingFrameState *f) {
  return f->phase == 1 || (f->phase == 8 && f->state_721ee0 == 4);
}
static int group1_three(const BkEndingSpecialBindings *b) {
  const BkEndingFrameState *f = b->frame;
  return f->phase == 3 || f->phase == 4 || f->phase == 6 ||
         (*b->action_variant && f->phase == 8 &&
          (f->state_721ee0 == 6 || f->state_721ee0 == 7 ||
           f->state_721ee0 == 8));
}
static int group4(const BkEndingSpecialBindings *b) {
  return b->frame->phase == 6 || (b->frame->phase == 8 && *b->action_variant &&
                                  b->frame->state_721ee0 == 6);
}
static int material(const BkEndingSpecialOps *o, int mode, char e[256]) {
  /*576010 and576024 contain the same name, distinct native literals. */
  return o->material ? o->material(o->context, "Om_syokusyu_maki", mode, 1, e)
                     : fail(e, "missing material service");
}
int bk_ending_special_draw(const BkEndingSpecialBindings *b, BkDrawDispatch *d,
                           const BkEndingSpecialOps *o, char e[256]) {
  if (!b || !b->frame || !b->action_variant || !b->camera_variant ||
      !b->restore_hidden || !b->camera_index || !b->offset_mode ||
      !b->cameras || !b->target || !b->primary_root || !b->auxiliary_root ||
      !d || !o)
    return fail(e, "missing live bindings");
  if (!o->draw || !o->camera_read)
    return fail(e, "missing draw/camera read service");
  if (!o->draw(o->context, d, e))
    return 0;
  float position[3], forward[3], up[3];
  if (!o->camera_read(o->context, position, forward, up, e) ||
      !render(o, BK_ENDING_SPECIAL_END, 0, e) ||
      !render(o, BK_ENDING_SPECIAL_VIEWPORT, 1, e) ||
      !render(o, BK_ENDING_SPECIAL_CLEAR, 2, e) ||
      !render(o, BK_ENDING_SPECIAL_BEGIN, 0, e) || !reset_camera(o, e))
    return 0;
  uint32_t base[3] = {0}, extra[3] = {0};
  for (unsigned i = 0; i < 3; i++) {
    if (!profile(b, e) ||
        !find_hide(b, o, hidden_names[b->frame->group][*b->camera_variant][i],
                   &base[i], e))
      return 0;
  }
  const BkEndingFrameState *f = b->frame;
  if (f->group == 0 &&
      (f->phase == 5 || (f->phase == 8 && f->state_721ee0 == 6))) {
    if (!find_hide(b, o, "M_pantsu", &extra[0], e) ||
        !find_hide(b, o, "M_pantsuU", &extra[1], e))
      return 0;
  }
  if (f->group == 1) {
    if (group1_first(f)) {
      if (!find_hide(b, o, "oisu_005B_Layer1", &extra[0], e))
        return 0;
    } else if (group1_three(b)) {
      static const char *const names[3] = {"bunben_semotare", "bunben_dai",
                                           "bunben_te"};
      for (unsigned i = 0; i < 3; i++)
        if (!find_hide(b, o, names[i], &extra[i], e))
          return 0;
    }
  }
  if (f->group == 2 && !*b->action_variant &&
      !find_hide(b, o, "OYU", &extra[0], e))
    return 0;
  if (f->group == 4 && group4(b)) {
    if (!find_hide(b, o, "M_syokusyu_kuch_gawa", &extra[0], e) ||
        !find_hide(b, o, "M_syokusyu_kuch_moza", &extra[1], e) ||
        !material(o, 1, e))
      return 0;
  }
  if (*b->camera_index < 0 || *b->camera_index >= BK_ENDING_SPECIAL_CAMERAS)
    return fail(e, "invalid secondary camera index");
  float eye[3];
  memcpy(eye, b->cameras[*b->camera_index], sizeof(eye));
  if (!reset_camera(o, e))
    return 0;
  /* XYZ were copied before the second reset; target/offset are read after. */
  if (*b->camera_index < 0 || *b->camera_index >= BK_ENDING_SPECIAL_CAMERAS)
    return fail(e, "invalid live target offset index");
  float target[3];
  memcpy(target, b->target, sizeof(target));
  int axis = 1;
  if (f->group == 4 && *b->offset_mode == 1 && *b->camera_index == 47)
    axis = f->phase == 5 || (f->phase == 8 && !*b->action_variant) ? 0 : 1;
  else if (f->group == 0 && *b->offset_mode == 2 && *b->camera_index == 104)
    axis = f->phase == 6 || (f->phase == 8 && *b->action_variant) ? 2 : -1;
  if (axis >= 0)
    target[axis] =
        (float)((double)target[axis] - b->cameras[*b->camera_index][3]);
  for (unsigned i = 0; i < 3; i++)
    if (!isfinite(eye[i]) || !isfinite(target[i]))
      return fail(e, "non-finite secondary camera");
  if (!o->camera_aim || !o->camera_publish)
    return fail(e, "missing camera aim/publication service");
  if (!o->camera_position(o->context, eye, e) ||
      !o->camera_aim(o->context, target, e) ||
      !o->camera_publish(o->context, e))
    return 0;
  d->mode = 2;
  memset(d->objects + 20, 0, 32 * sizeof(*d->objects));
  d->objects[20] = *b->primary_root;
  if (f->phase == 1 || f->phase == 8)
    d->objects[21] = *b->auxiliary_root;
  if (!o->draw(o->context, d, e))
    return 0;
  for (unsigned i = 0; i < 3; i++)
    if (!hide(o, base[i], *b->restore_hidden, e))
      return 0;
  if (f->group == 1) {
    unsigned count = group1_first(f) ? 1 : group1_three(b) ? 3 : 0;
    for (unsigned i = 0; i < count; i++)
      if (!hide(o, extra[i], 0, e))
        return 0;
  }
  if (f->group == 2 && !*b->action_variant && !hide(o, extra[0], 0, e))
    return 0;
  if (f->group == 4 && group4(b)) {
    for (unsigned i = 0; i < 2; i++)
      if (!hide(o, extra[i], 0, e))
        return 0;
    if (!material(o, 0, e))
      return 0;
  }
  return render(o, BK_ENDING_SPECIAL_VIEWPORT, 0, e) &&
         o->camera_position(o->context, position, e) &&
         o->camera_orientation(o->context, forward, up, e) &&
         o->camera_publish(o->context, e);
}
