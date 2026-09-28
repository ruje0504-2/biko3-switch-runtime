#include "scene/selection_ui.h"
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static int fail(char e[256], const char *why) {
  snprintf(e, 256, "selection UI: %s", why);
  return 0;
}
static int present(unsigned i, uint8_t special) {
  return special == 1 ? ((i >= 23 && i <= 26) || i == 41 || i == 47)
                      : (i >= 1 && i <= 41);
}
const char *bk_selection_image(unsigned i, uint8_t special) {
  static const char *const names[50] = {
      NULL,        "pr_36.tga", "pr_37.tga", "pr_38.tga", "pr_39.tga",
      "pr_40.tga", "pr_41.tga", "pr_03.tga", "pr_05.tga", "pr_08.tga",
      "pr_10.tga", "pr_13.tga", "pr_15.tga", "pr_18.tga", "pr_20.tga",
      "pr_23.tga", "pr_25.tga", "pr_26.tga", "pr_27.tga", "pr_28.tga",
      "pr_29.tga", "pr_30.tga", "pr_31.tga", "pr_32.tga", "pr_33.tga",
      "pr_34.tga", "pr_35.tga", "pr_01.tga", "pr_02.tga", "pr_06.tga",
      "pr_07.tga", "pr_11.tga", "pr_12.tga", "pr_16.tga", "pr_17.tga",
      "pr_21.tga", "pr_22.tga", "pr_42.tga", "pr_43.tga", "pr_44.tga",
      "pr_45.tga", "pr_46.tga", NULL,        NULL,        NULL,
      NULL,        NULL,        "pr_99.tga", "ma_00.tga", "ma_01.tga"};
  return i >= 50 || (i < 48 && !present(i, special)) ? NULL : names[i];
}
int bk_selection_ui_initialize(BkSelectionUi *s, unsigned width,
                               uint8_t special, int32_t master, char e[256]) {
  if (!s || !width || width > 16384 || master < -10000 || master > 0)
    return fail(e, "invalid initializer");
  float scale = (float)((double)width / 1280);
  for (unsigned i = 0; i < 48; ++i) {
    if (!present(i, special))
      continue;
    float rect[4];
    if (i == 1)
      memcpy(rect, (float[4]){24, 871, 176, 64}, sizeof(rect));
    else if (i <= 6)
      memcpy(rect, (float[4]){16, 576, 432, 360}, sizeof(rect));
    else if (i <= 16)
      memcpy(rect, (float[4]){48, 99 + 67 * ((i - 7) / 2), 200, 56},
             sizeof(rect));
    else if (i <= 22)
      memcpy(rect, (float[4]){1080, i <= 19 ? 32 : 90, 176, 56}, sizeof(rect));
    else if (i <= 26)
      memcpy(rect, (float[4]){i <= 24 ? 704 : 976, 880, 256, 56}, sizeof(rect));
    else if (i <= 36)
      memcpy(rect, (float[4]){20, 88 + 67 * ((i - 27) / 2), 72, 72},
             sizeof(rect));
    else if (i <= 40)
      memcpy(rect, (float[4]){i <= 38 ? 83 : 337, 619, 40, 40}, sizeof(rect));
    else if (i == 41)
      memcpy(rect, (float[4]){16, 20, 440, 72}, sizeof(rect));
    else
      memcpy(rect, (float[4]){0, 640, 1280, 64}, sizeof(rect));
    BkSelectionSprite *p = &s->sprites[i];
    *p = (BkSelectionSprite){.uv = {0, 0, 1, 1}};
    bk_zoom_sprite_initialize(&p->transform);
    p->transform.pivot[0] = p->transform.pivot[1] = 0;
    p->transform.fade.stage = 1;
    for (unsigned j = 0; j < 4; ++j)
      p->rect[j] = rect[j] * scale;
    s->loaded |= UINT64_C(1) << i;
  }
  s->info = 1;
  s->music_volume = master;
  s->music_mode = 1;
  return 1;
}
static int basic(const BkSelectionUi *s, const BkSelectionBindings *b,
                 const BkSelectionInput *in, const BkSelectionOps *o,
                 char e[256]) {
  if (!s || !b || !b->common || !b->flow || !b->cursor || !in || !o ||
      !isfinite(in->seconds) || in->seconds < 0 || !isfinite(in->scale) ||
      in->scale <= 0 || in->scale > 16 || b->flow->current != 0x38)
    return fail(e, "invalid frame/bindings");
  return 1;
}
static int sprite(BkSelectionUi *s, unsigned i, float seconds,
                  BkSelectionFrame *f) {
  BkSelectionSprite *p = &s->sprites[i];
  if (!bk_fade_sprite_advance(&p->transform.fade, seconds))
    return 0;
  if (!(s->loaded & (UINT64_C(1) << i)))
    return 1;
  if (f->count >= BK_SELECTION_DRAWS)
    return 0;
  BkSelectionDraw *d = &f->draws[f->count++];
  d->slot = i;
  d->alpha = p->transform.fade.alpha;
  memcpy(d->uv, p->uv, sizeof(d->uv));
  return bk_zoom_sprite_rect(&p->transform, p->rect, d->corners);
}
static int flat(BkSelectionFrame *f, unsigned i, const float r[4],
                float alpha) {
  if (f->count >= BK_SELECTION_DRAWS)
    return 0;
  f->draws[f->count++] = (BkSelectionDraw){
      i, {r[0], r[1], r[0] + r[2], r[1] + r[3]}, {0, 0, 1, 1}, alpha};
  return 1;
}
int bk_selection_ui_view(BkSelectionUi *s, const BkSelectionBindings *b,
                         const BkSelectionInput *in, const BkSelectionOps *o,
                         BkSelectionFrame *out, char e[256]) {
  if (!basic(s, b, in, o, e))
    return 0;
  if (!out || !o->position || !o->motion)
    return fail(e, "missing view services");
  BkSelectionFrame f = {0};
#define DRAW(i)                                                                \
  do {                                                                         \
    if (!sprite(s, (i), in->seconds, &f))                                      \
      return fail(e, "invalid sprite");                                        \
  } while (0)
  if (in->special != 1) {
    if (s->info == 1) {
      DRAW(1);
    } else {
      if (s->selected >= 0 && s->selected < 5) {
        DRAW(2 + s->selected);
      }
      DRAW(s->action_hover == 38 ? 38 : 37);
      DRAW(s->action_hover == 40 ? 40 : 39);
    }
  }
  DRAW(s->action_hover == 23 ? 24 : 23);
  DRAW(s->action_hover == 25 ? 26 : 25);
  if (in->special != 1) {
    for (unsigned i = 7; i <= 15; i += 2) {
      DRAW(i);
    }
    for (unsigned i = 27; i <= 35; i += 2) {
      DRAW(i);
    }
    if (s->selected >= 0 && s->selected < 5) {
      DRAW(8 + 2 * s->selected);
      DRAW(28 + 2 * s->selected);
    }
    if (s->camera_hover == 20) {
      DRAW(s->camera_mode == 0 ? 22 : 21);
      DRAW(s->camera_mode == 0 ? 17 : 19);
    } else if (s->camera_hover == 17) {
      DRAW(s->camera_mode == 1 ? 20 : 22);
      DRAW(s->camera_mode == 1 ? 19 : 18);
    } else if (s->camera_mode == 0) {
      DRAW(22);
      DRAW(17);
    } else if (s->camera_mode == 1) {
      DRAW(20);
      DRAW(19);
    }
  } else {
    DRAW(47);
  }
  if (!o->position(o->context, s->pointer, e))
    return 0;
  if (!isfinite(s->pointer[0]) || !isfinite(s->pointer[1]))
    return fail(e, "invalid pointer");
  BkMenuCursor *c = b->cursor;
  memcpy(c->sprite.rect, s->pointer, sizeof(s->pointer));
  if (!bk_fade_sprite_advance(&c->sprite.fade, in->seconds) ||
      !flat(&f, BK_SELECTION_CURSOR, c->sprite.rect, c->sprite.fade.alpha) ||
      !bk_fade_sprite_request(&c->sprite.fade, c->wanted))
    return fail(e, "invalid cursor");
  if (!o->motion(o->context, s->motion, e))
    return 0;
  if (!isfinite(s->motion[0]) || !isfinite(s->motion[1]))
    return fail(e, "invalid motion");
  if (s->motion[0] == 0 && s->motion[1] == 0) {
    if (bk_timer_poll(&c->idle, in->now_ms))
      c->wanted = 0;
  } else {
    c->wanted = 1;
    c->idle.armed = 0;
  }
  DRAW(41);
  float full[4] = {0, 0, 1280.f * in->scale, 960.f * in->scale};
  if (!bk_fade_sprite_advance(&b->common->curtain, in->seconds) ||
      !flat(&f, BK_SELECTION_CURTAIN, full, b->common->curtain.alpha) ||
      !bk_fade_sprite_request(&b->common->curtain, b->common->blocked))
    return fail(e, "invalid curtain");
  *out = f;
  return 1;
#undef DRAW
}
static int inside(const BkSelectionSprite *s, const float p[2]) {
  return s->rect[0] <= p[0] && (double)s->rect[0] + s->rect[2] >= p[0] &&
         s->rect[1] <= p[1] && (double)s->rect[1] + s->rect[3] >= p[1];
}
static const int nav[2][6][4][2] = {
    {{{60, 125}, {-1, -1}, {-1, -1}, {1173, 60}},
     {{60, 187}, {-1, -1}, {-1, -1}, {1173, 115}},
     {{60, 250}, {-1, -1}, {-1, -1}, {-1, -1}},
     {{60, 312}, {-1, -1}, {-1, -1}, {-1, -1}},
     {{60, 375}, {-1, -1}, {-1, -1}, {-1, -1}},
     {{108, 889}, {-1, -1}, {832, 910}, {1109, 910}}},
    {{{60, 125}, {-1, -1}, {-1, -1}, {1173, 60}},
     {{60, 187}, {-1, -1}, {-1, -1}, {1173, 115}},
     {{60, 250}, {-1, -1}, {-1, -1}, {-1, -1}},
     {{60, 312}, {-1, -1}, {-1, -1}, {-1, -1}},
     {{60, 375}, {-1, -1}, {-1, -1}, {-1, -1}},
     {{105, 640}, {357, 640}, {832, 910}, {1109, 910}}}};
static int warp(BkSelectionUi *s, const BkSelectionInput *in,
                const BkSelectionOps *o, int table, char e[256]) {
  if (s->row < 0 || s->row > 5 || s->column < 0 || s->column > 3)
    return fail(e, "navigation outside original table");
  const int *p = nav[table][s->row][s->column];
  if (in->special == 1) {
    static const int demo[2][2] = {{832, 910}, {1109, 910}};
    if (s->column > 1)
      return fail(e, "demo navigation outside original table");
    p = demo[s->column];
  }
  return o->warp(o->context, (float)((double)p[0] * in->scale),
                 (float)((double)p[1] * in->scale), e);
}
static int navigate(BkSelectionUi *s, const BkSelectionInput *in,
                    const BkSelectionOps *o, int changed, char e[256]) {
  if (!(in->buttons &
        (BK_PAUSE_UP | BK_PAUSE_DOWN | BK_PAUSE_LEFT | BK_PAUSE_RIGHT)) &&
      !changed)
    return 1;
  if (s->row < 0 || s->row > 5 || s->column < 0 || s->column > 3)
    return fail(e, "invalid navigation state");
  int table = s->info == 1 ? 0 : 1;
  if (in->buttons & (BK_PAUSE_UP | BK_PAUSE_DOWN)) {
    int d = in->buttons & BK_PAUSE_UP ? -1 : 1;
    s->row = (s->row + d + 6) % 6;
    if (s->column == 1)
      s->column = 0;
    else if (s->column == 2)
      s->column = 3;
    if (in->special != 1)
      for (unsigned j = 0; j < 6 && nav[table][s->row][s->column][0] == -1; ++j)
        s->row = (s->row + d + 6) % 6;
    if (!warp(s, in, o, table, e))
      return 0;
  }
  if (in->buttons & (BK_PAUSE_LEFT | BK_PAUSE_RIGHT)) {
    int d = in->buttons & BK_PAUSE_LEFT ? -1 : 1;
    if (in->special == 1) {
      if (s->column == 0)
        s->column = 1;
      else if (s->column == 1)
        s->column = 0;
    } else
      s->column = (s->column + d + 4) % 4;
    if (s->row == 2)
      s->row = 1;
    else if (s->row == 3 || s->row == 4)
      s->row = 5;
    if (in->special != 1)
      for (unsigned j = 0; j < 4 && nav[table][s->row][s->column][0] == -1; ++j)
        s->column = (s->column + d + 4) % 4;
    if (!warp(s, in, o, table, e))
      return 0;
  }
  if (in->special != 1 && changed && !warp(s, in, o, changed - 1, e))
    return 0;
  return 1;
}
typedef struct {
  BkSelectionUi *state;
  const BkSelectionOps *ops;
} Release;
static int release(void *p, uint8_t flow, char e[256]) {
  Release *r = p;
  if (!r->ops->release(r->ops->context, flow, e))
    return 0;
  if (flow == 0x38)
    r->state->loaded = 0;
  return 1;
}
int bk_selection_ui_control(BkSelectionUi *s, const BkSelectionBindings *b,
                            const BkSelectionInput *in, const BkSelectionOps *o,
                            char e[256]) {
  if (!basic(s, b, in, o, e))
    return 0;
  if (!b->hover_latched || !b->photos || !b->group || !b->area ||
      !b->photo_count || !o->sound || !o->music_gain || !o->warp ||
      !o->voice_stop || !o->voice_play || !o->voice_status ||
      !o->replace_actor || !o->release || in->music_master < -10000 ||
      in->music_master > 0 || in->voice_master < -10000 ||
      in->voice_master > 0 || s->music_volume < -10000 || s->music_volume > 0 ||
      (double)in->seconds * 800 > INT32_MAX - 10000 ||
      !isfinite(s->pointer[0]) || !isfinite(s->pointer[1]))
    return fail(e, "invalid control/services");
  for (unsigned i = 0; i < 5; ++i)
    if (!isfinite(s->reveal[i]) || s->reveal[i] < 0 || s->reveal[i] > 1)
      return fail(e, "invalid reveal");
  int old = s->selected, changed = 0,
      confirm = (in->buttons & BK_PAUSE_CONFIRM) != 0;
  s->character_hover = s->camera_hover = s->action_hover = 99;
#define SOUND(slot)                                                            \
  do {                                                                         \
    if (!o->sound(o->context, (slot), e))                                      \
      return 0;                                                                \
  } while (0)
  if (in->special == 1)
    s->camera_mode = 1;
  else {
    for (unsigned i = 0; i < 5; ++i) {
      BkSelectionSprite *p = &s->sprites[27 + 2 * i],
                        *label = &s->sprites[7 + 2 * i];
      float right = (float)((double)p->rect[0] + p->rect[2] +
                            (double)label->rect[2] * s->reveal[i]);
      if (p->rect[0] <= s->pointer[0] && right >= s->pointer[0] &&
          p->rect[1] <= s->pointer[1] &&
          (double)p->rect[1] + p->rect[3] >= s->pointer[1]) {
        s->character_hover = 7 + 2 * i;
        if (confirm) {
          SOUND(4);
          if (s->selected != (int)i && in->voice_present &&
              !o->voice_stop(o->context, e))
            return 0;
          s->selected = i;
        }
      }
    }
    for (unsigned i = 0; i < 2; ++i) {
      unsigned slot = i ? 17 : 20;
      if (inside(&s->sprites[slot], s->pointer)) {
        s->camera_hover = slot;
        if (confirm) {
          SOUND(4);
          s->camera_mode = i;
        }
      }
    }
  }
  for (unsigned i = 0; i < 2; ++i) {
    unsigned slot = i ? 25 : 23;
    if (inside(&s->sprites[slot], s->pointer)) {
      s->action_hover = slot;
      if (confirm) {
        SOUND(i ? 2 : 1);
        b->common->action = slot;
        b->common->blocked = 1;
        s->music_mode = 0;
      }
    }
  }
  if (in->special != 1) {
    if (s->info == 1) {
      if (confirm && inside(&s->sprites[1], s->pointer)) {
        SOUND(4);
        s->info = 100;
        changed = 2;
        s->row = 5;
        s->column = 0;
      }
    } else {
      if (inside(&s->sprites[38], s->pointer)) {
        s->action_hover = 38;
        if (confirm) {
          SOUND(4);
          s->info = 1;
          s->voice_active = 0;
          changed = 1;
          s->row = 5;
          s->column = 0;
        }
      }
      if (inside(&s->sprites[40], s->pointer)) {
        s->action_hover = 40;
        if (confirm) {
          if (!in->voice_present)
            return fail(e, "voice replay requires loaded voice");
          if (!o->voice_play(o->context, in->voice_master, e))
            return 0;
          s->voice_active = 1;
        }
      }
    }
    if (s->voice_active == 1) {
      int playing = 0;
      if (in->voice_present && !o->voice_status(o->context, &playing, e))
        return 0;
      if (!playing)
        s->voice_active = 0;
    }
  }
  int hover = 0;
  if (s->action_hover != 99 && s->action_hover != 38 && s->action_hover != 40)
    hover = s->action_hover;
  else if (s->character_hover != 99)
    hover = s->character_hover;
  if (hover) {
    if (hover != (int8_t)*b->hover_latched) {
      SOUND(3);
      *b->hover_latched = (uint8_t)hover;
    }
  } else
    *b->hover_latched = 0;
  if (!navigate(s, in, o, changed, e))
    return 0;
  static const double minimum[5] = {.55, .4, .3, .4, .55};
  for (unsigned i = 0; i < 5; ++i) {
    int over = s->character_hover == (int)(7 + 2 * i);
    double target = over ? 1 : minimum[i];
    float delta = (float)(target - s->reveal[i]);
    delta = (float)(2.0 * in->seconds * delta);
    s->reveal[i] += delta;
    if (over ? s->reveal[i] >= 1 : s->reveal[i] <= target)
      s->reveal[i] = (float)target;
    if (in->special != 1)
      for (unsigned j = 0; j < 2; ++j) {
        BkSelectionSprite *p = &s->sprites[7 + 2 * i + j];
        p->uv[0] = 1.f - s->reveal[i];
        p->uv[1] = 0;
        p->uv[2] = p->uv[3] = 1;
        p->transform.scale[0] = s->reveal[i];
        p->transform.scale[1] = 1;
      }
  }
  int32_t delta = (int32_t)(float)((double)in->seconds * 800);
  if (delta <= 1)
    delta = 1;
  if (s->music_mode == 1) {
    s->music_volume += delta;
    if (s->music_volume >= in->music_master)
      s->music_volume = in->music_master;
  } else if (s->music_mode == 0) {
    s->music_volume -= delta;
    if (s->music_volume <= -6000)
      s->music_volume = -6000;
  }
  if (!o->music_gain(o->context, s->music_volume, e))
    return 0;
  if (old != s->selected) {
    if (s->selected < 0 || s->selected > 4)
      return fail(e, "invalid actor selection");
    if (!o->replace_actor(o->context, (unsigned)s->selected, e))
      return 0;
  }
  if (in->special == 1) {
    s->scroll = (float)((double)s->scroll + .02 * in->seconds);
    if (s->scroll >= 1)
      s->scroll = 0;
    BkSelectionSprite *p = &s->sprites[47];
    p->rect[0] = 0;
    p->rect[1] = 816.f * in->scale;
    memcpy(p->uv, (float[4]){s->scroll, 0, 1.f + s->scroll, 1}, sizeof(p->uv));
  }
  if (b->common->blocked == 1 && b->common->curtain.stage == 3) {
    b->common->blocked = 0;
    s->scroll = 0;
    Release r = {s, o};
    BkFlowTransitionOps fo = {&r, release};
    if (b->common->action == 23) {
      if (in->special == 1) {
        if (!bk_flow_transition_schedule(b->flow, &fo, 2, 1, e))
          return 0;
        *b->group = 0;
        *b->area = 2;
      } else {
        if (s->selected < 0 || s->selected > 4)
          return fail(e, "start selection outside photo table");
        *b->photo_count = b->photos[s->selected];
        *b->area = 0;
        *b->group = s->selected;
        if (!bk_flow_transition_schedule(b->flow, &fo, 8, 1, e))
          return 0;
      }
      s->selected = s->camera_mode = 0;
    } else if (b->common->action == 25) {
      if (!bk_flow_transition_schedule(b->flow, &fo, 1, 0, e))
        return 0;
      s->selected = s->camera_mode = 0;
    }
    b->common->action = 0;
  }
  return 1;
#undef SOUND
}
