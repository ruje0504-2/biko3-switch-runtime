#include "scene/failure_hud.h"
#include <math.h>
#include <stdio.h>
int bk_failure_hud_step(BkFailureHudState *s, const BkFailureHudBindings *b,
                        const BkFailureHudOps *o, int advance, float seconds,
                        BkFailureHudFrame *f, char e[256]) {
  if (!s || !b || !b->common || !b->panel || !b->prompt || !b->text ||
      !b->group || !b->outcome || !b->pause_overlay || !b->npc_visible ||
      !b->player_script_phase || !b->player_completion_mode ||
      !b->npc_stimulus || !b->npc_behavior || !b->npc_wait_duration || !o ||
      !o->click || !o->message || !o->release || !o->schedule || !f ||
      (advance != 0 && advance != 1) || !isfinite(seconds) || seconds < 0) {
    snprintf(e, 256, "failure HUD: invalid bindings/services/time");
    return 0;
  }
  BkFadeSprite check[] = {*b->panel, *b->prompt, b->common->curtain};
  for (unsigned i = 0; i < 3; ++i)
    if (!bk_fade_sprite_advance(&check[i], seconds)) {
      snprintf(e, 256, "failure HUD: invalid sprite");
      return 0;
    }
  if (advance) {
    if (*b->outcome == 6) {
      if (s->advances == 0) {
        if (*b->group >= 5) {
          snprintf(e, 256, "failure HUD: second-message group out of range");
          return 0;
        }
        if (!o->click(o->context, e) ||
            !o->message(o->context, 10401 + *b->group * 10000, e))
          return 0;
        b->text->started = 0;
        b->text->enabled = 1;
      } else if (s->advances == 1) {
        if (!o->click(o->context, e))
          return 0;
        b->common->blocked = 1;
      }
      s->advances++;
    } else {
      if (!o->click(o->context, e))
        return 0;
      b->common->blocked = 1;
    }
  }
  bk_fade_sprite_advance(b->panel, seconds);
  f->panel_alpha = b->panel->alpha;
  bk_fade_sprite_request(b->panel, s->visible);
  bk_fade_sprite_request(b->prompt, s->visible);
  bk_fade_sprite_advance(b->prompt, seconds);
  f->prompt_alpha = b->prompt->alpha;
  f->draw_text = b->panel->stage == 2 || b->panel->stage == 3;
  bk_fade_sprite_advance(&b->common->curtain, seconds);
  f->curtain_alpha = b->common->curtain.alpha;
  bk_fade_sprite_request(&b->common->curtain, b->common->blocked);
  if (b->common->blocked == 1) {
    *b->pause_overlay = 0;
    if (b->common->curtain.stage == 3) {
      b->common->blocked = 0;
      if (!o->release(o->context, 0x40, e) || !o->release(o->context, 2, e) ||
          !o->schedule(o->context, 0x68, 2, e))
        return 0;
      s->visible = 0;
      s->advances = 0;
      *b->player_script_phase = 0;
      *b->outcome = 0;
      *b->player_completion_mode = 0;
      *b->npc_visible = 0;
      *b->npc_stimulus = 0;
      *b->npc_behavior = 1;
      *b->npc_wait_duration = 3000;
    }
  }
  return 1;
}
