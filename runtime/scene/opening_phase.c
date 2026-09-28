#include "scene/opening_phase.h"
#include <math.h>
#include <stdio.h>
int bk_opening_phase_step(const BkOpeningBindings *b, const BkOpeningOps *o,
                          int advance, float source, float end,
                          char error[256]) {
  if (!b || !b->camera || !b->notice_visible || !b->npc_hidden ||
      !b->player_hidden || !b->npc_behavior || !b->panel || !b->flow || !o ||
      !o->click || !o->next || !o->bind || !o->clear_font ||
      !o->close_dialogue || !o->prepare_items || !o->select_camera ||
      !isfinite(source) || !isfinite(end) || b->panel->stage > 5 ||
      (b->camera->phase != 0 && b->camera->phase != 2 &&
       b->camera->phase != 3)) {
    snprintf(error, 256, "opening: invalid bindings/services/phase/timing");
    return 0;
  }
  uint8_t phase = b->camera->phase;
  void *c = o->context;
  if (phase == 3) {
    if (!o->prepare_items(c, error))
      return 0;
    b->camera->stage = 0;
    b->camera->phase = 1;
    return 1;
  }
  if (phase == 0 && advance) {
    if (*b->notice_visible == 1 && !o->click(c, error))
      return 0;
    b->flow->scroll = b->flow->target;
    b->flow->started = 0;
    b->flow->enabled = 1;
    int done = 0;
    if (!o->next(c, &done, error))
      return 0;
    if (done)
      *b->notice_visible = 0;
    if (!o->bind(c, error))
      return 0;
  }
  if (phase == 2) {
    bk_fade_sprite_request(b->panel, 0);
    *b->notice_visible = 0;
  }
  *b->npc_hidden = 0;
  switch (b->camera->stage) {
  case 0:
    if ((phase == 2 || b->panel->stage == 0) && source >= end)
      b->camera->stage = 1;
    *b->player_hidden = 1;
    break;
  case 1:
    *b->player_hidden = 0;
    break;
  case 2:
    *b->npc_behavior = 1;
    if (phase == 0 &&
        (!o->clear_font(c, error) || !o->close_dialogue(c, error)))
      return 0;
    if (!o->prepare_items(c, error) || !o->select_camera(c, error))
      return 0;
    b->camera->stage = 0;
    b->camera->phase = 1;
    break;
  }
  return 1;
}
