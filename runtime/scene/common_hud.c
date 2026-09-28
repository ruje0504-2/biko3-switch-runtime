#include "scene/common_hud.h"
#include <stdio.h>
void bk_common_hud_initialize(BkCommonHudState *s) {
  if (!s)
    return;
  bk_fade_sprite_initialize(&s->curtain);
  bk_fade_sprite_request(&s->curtain, 1);
  s->wait.duration = 1000;
}
void bk_common_hud_entry_reset(BkCommonHudState *s) {
  if (s) {
    s->wait.armed = 0;
    s->gate = 0;
  }
}
int bk_common_hud_step(BkCommonHudState *s, const BkCommonHudBindings *b,
                       const BkCommonHudOps *o, float seconds, uint32_t now,
                       BkCommonHudFrame *frame, char e[256]) {
  if (!s || !b || !b->outcome || !b->menu_request || !b->response ||
      !b->pause_overlay || !b->group || !b->area || !b->flow || !o ||
      !o->play_outcome || !o->play_response || !o->schedule || !o->load_pause ||
      !frame || !bk_fade_sprite_advance(&s->curtain, seconds)) {
    snprintf(e, 256, "common HUD: invalid bindings/services/time/state");
    return 0;
  }
  frame->curtain_alpha = s->curtain.alpha;
  if (s->gate == 0) {
    if (bk_timer_poll(&s->wait, now)) {
      bk_fade_sprite_request(&s->curtain, s->blocked);
      s->gate = 1;
    }
  } else if (s->gate == 1)
    bk_fade_sprite_request(&s->curtain, s->blocked);
  if (*b->outcome) {
    s->action = 1;
    s->blocked = 1;
    if (!o->play_outcome(o->context, e))
      return 0;
  } else {
    if (*b->menu_request == 1) {
      s->action = 2;
      *b->menu_request = 0;
      *b->pause_overlay = 0;
    }
    if (*b->response == 1) {
      s->action = 3;
      *b->response = 0;
      s->blocked = 1;
      *b->pause_overlay = 0;
      if (!o->play_response(o->context, e))
        return 0;
    } else if (*b->response >= 2 && *b->response <= 4) {
      s->action = *b->response + 2;
      s->blocked = 1;
      *b->pause_overlay = 0;
    }
  }
  if (s->blocked == 1 && s->curtain.stage == 3) {
    s->blocked = 0;
    uint8_t target = 0, mode = 0;
    int schedule = 1;
    switch (s->action) {
    case 1:
      target = 0x40;
      mode = 2;
      break;
    case 3:
      if (b->special_mode == 1) {
        if (*b->area == 4)
          target = 0x70;
        else {
          *b->group = 0;
          *b->area = 4;
          target = 2;
        }
        mode = 1;
      } else {
        target = 0x20;
        mode = 2;
      }
      break;
    case 4:
      target = 0x48;
      mode = 1;
      break;
    case 5:
    case 6:
      target = 8;
      break;
    default:
      schedule = 0;
      break;
    }
    if (schedule) {
      if (!o->schedule(o->context, target, mode, e))
        return 0;
      s->action = 0;
    }
  }
  if (s->action == 2) {
    if (!o->load_pause(o->context, e))
      return 0;
    *b->flow = 4;
    s->action = 0;
  }
  return 1;
}
