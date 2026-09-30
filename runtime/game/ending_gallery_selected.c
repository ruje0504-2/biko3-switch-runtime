#include "game/ending_gallery_selected.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "game/ending_gallery_selected_tables.inc"
static int fail(char e[256], const char *why) {
  if (e) snprintf(e, 256, "gallery selected: %s", why);
  return 0;
}
BkEndingGallerySelectedState bk_ending_gallery_selected_initial(void) {
  BkEndingGallerySelectedState s = {0}; s.fov = 1.f; s.countdown = 5; return s;
}
#define CALL(name, ...) \
  (o->name ? o->name(o->context, __VA_ARGS__) : fail(e, "missing " #name " service"))
static void word(int32_t *p, uint32_t bits) { memcpy(p, &bits, 4); }
static void add(int32_t *p, uint32_t amount) { word(p, (uint32_t)*p + amount); }
static int playing(const BkEndingGallerySelectedOps *o, unsigned slot, int *busy, char e[256]) {
  int exists;
  if (!CALL(present, slot, &exists, e)) return 0;
  if (!exists) { *busy = 0; return 1; }
  return CALL(status, slot, busy, e);
}
static int stop_if_playing(const BkEndingGallerySelectedOps *o, unsigned slot, char e[256]) {
  int busy;
  return playing(o, slot, &busy, e) && (!busy || CALL(stop, slot, e));
}
static int stop_effects(const BkEndingGallerySelectedOps *o, char e[256]) {
  for (unsigned i = 2; i < 5; ++i) if (!stop_if_playing(o, i, e)) return 0;
  return 1;
}
static int target(const BkEndingGallerySelectedBindings *b,
    const BkEndingGallerySelectedOps *o, unsigned node, char e[256]) {
  uint32_t v[3];
  if (!CALL(target, node, v, e)) return 0;
  memcpy(b->frame->camera_values, v, 12); return 1;
}
static int active_clip(const BkEndingGallerySelectedOps *o, int32_t *active,
    BkEndingGallerySelectedClip *t, char e[256]) {
  return CALL(active, active, e) && CALL(clip, *active, t, e);
}
static int rewind_clip(const BkEndingGallerySelectedOps *o, int32_t slot, char e[256]) {
  BkEndingGallerySelectedClip t;
  return CALL(clip, slot, &t, e) && CALL(source, slot, t.start, e);
}
static int rewind_pair(const BkEndingGallerySelectedOps *o, int32_t slot, char e[256]) {
  return rewind_clip(o, slot, e) && rewind_clip(o, slot + 1, e);
}
static int chains(const BkEndingGallerySelectedOps *o, int value, char e[256]) {
  static const int slots[] = {5,6,7,9,10,11,13,14,15,17,18};
  for (unsigned i = 0; i < sizeof(slots)/sizeof(*slots); ++i) {
    if (slots[i] == 9 && !value) continue; /*native exit never writes slot9*/
    if (!CALL(chain, slots[i], slots[i] == 9 ? 0 : value, e)) return 0;
  }
  return 1;
}
static void auxiliary(BkEndingGallerySelectedState *s,
    const BkEndingGallerySelectedBindings *b, unsigned offset) {
  word(&b->auxiliary->index, (uint32_t)s->base + (uint32_t)b->auxiliary->selection * 6u + offset);
}
static int record(const BkEndingGallerySelectedBindings *b, uint32_t cursor,
    int32_t *out, char e[256]) {
  if (cursor >= b->workspace_capacity) return fail(e, "record cursor outside workspace");
  *out = b->workspace[cursor]; return 1;
}
static int row(const BkEndingGallerySelectedBindings *b, unsigned *out, char e[256]) {
  if (b->frame->group >= 5 || (uint32_t)b->auxiliary->selection >= 3 ||
      (uint32_t)b->auxiliary->variant >= 2) return fail(e, "camera configuration outside table");
  *out = b->frame->group * 6u + (unsigned)b->auxiliary->variant * 3u +
         (unsigned)b->auxiliary->selection;
  return 1;
}
static int camera_row(const BkEndingGallerySelectedBindings *b,
    const int32_t **out, char e[256]) {
  if (b->frame->group >= 5 || (uint32_t)b->auxiliary->selection >= 3)
    return fail(e, "live camera row outside table");
  *out = b->camera_words + b->frame->group * 15u + (unsigned)b->auxiliary->selection * 5u;
  return 1;
}
static void orbit(BkMenuCamera *c, const void *v) {
  /*No pointer arithmetic across distinct struct members.*/
  const unsigned char *p = v;
  memcpy(&c->yaw, p, 4); memcpy(&c->pitch, p+4, 4);
  memcpy(&c->radius, p+8, 4); memcpy(&c->height, p+12, 4);
}
static int live_orbit(const BkEndingGallerySelectedBindings *b, int full, char e[256]) {
  const int32_t *v;
  if (!camera_row(b, &v, e)) return 0;
  orbit(b->camera, v);
  if (full) memcpy(&b->camera->radius, v+4, 4);
  return 1;
}
static int table_orbit(const BkEndingGallerySelectedBindings *b, int finish,
    int32_t view, char e[256]) {
  unsigned r, count = finish ? 3 : 2;
  if (!row(b, &r, e)) return 0;
  if ((uint32_t)view >= count) return fail(e, "camera view outside table");
  orbit(b->camera, (finish ? finish_camera : repeat_camera) + (r * count + (unsigned)view) * 4);
  return 1;
}
static int finish_expression(const BkEndingGallerySelectedBindings *b,
    const BkEndingGallerySelectedOps *o, char e[256]) {
  if (!b->auxiliary->variant) return CALL(expression, 6, 6, 1, e);
  if (!CALL(expression, 5, 4, 1, e)) return 0;
  *b->expression_override = 5; *b->face_mode = 1; return 1;
}
static int blend(const BkEndingGallerySelectedBindings *b,
    const BkEndingGallerySelectedOps *o, uint32_t *done, char e[256]) {
  return CALL(camera, BK_ENDING_OPENING_PRESET, 0, b->frame->camera_values,
              UINT32_C(0x3dcccccd), done, e);
}
static int opening(BkEndingGallerySelectedState *s,
    const BkEndingGallerySelectedBindings *b, float seconds,
    const BkEndingGallerySelectedOps *o, char e[256]) {
  if (*b->opening == 0) {
    *b->opening = 1; s->fov = 1.f; s->reset = 0;
    memset(b->camera_words, 0, 300);
    memcpy(b->camera_words, opening_words + (b->auxiliary->variant != 0) * 75, 300);
    if (!CALL(expression, 6, 3, 1, e)) return 0;
    auxiliary(s, b, 0);
    return rewind_pair(o, 6, e) && rewind_pair(o, 10, e) && rewind_pair(o, 17, e);
  }
  if (*b->opening == 1) {
    int32_t choice = b->frame->camera_clip;
    if ((uint32_t)choice >= 3) return fail(e, "preset outside table");
    float v[4];
    for (unsigned i = 0; i < 4; ++i) v[i] = b->presets->active[i][choice];
    orbit(b->camera, v);
    static const unsigned nodes[] = {5,13,0};
    if ((uint32_t)b->control->target_choice >= 3) return fail(e, "target outside table");
    if (!target(b, o, nodes[b->control->target_choice], e) || !CALL(fov, s->fov, e)) return 0;
    if (seconds < 1.f) s->fov = (float)((double)s->fov - (double)seconds * .2f);
    int done = !(s->fov > .2f);
    if (done) s->fov = .2f;
    if (b->frame->group >= 5) return fail(e, "opening camera group outside table");
    uint32_t result;
    if (!CALL(camera, BK_ENDING_OPENING_PRESET, b->frame->camera_clip,
              b->frame->camera_values, b->frame->camera_table[b->frame->group][3], &result, e)) return 0;
    if (done && (result & 255u)) {
      s->fov = 1.f;
      char name[32];
      int group = b->frame->group < 128 ? b->frame->group : (int)b->frame->group - 256;
      snprintf(name, sizeof(name), "PH%d0103.wav", group + 1);
      strcpy(b->speech_name, name);
      if (!CALL(load, 0, name, e) || !CALL(play, 0, 1, *b->voice_volume, e)) return 0;
      *b->opening = 0; b->frame->camera_mode = 0; *b->substate = 1;
    }
  }
  return 1;
}
static int cycle(BkEndingGallerySelectedState *s,
    const BkEndingGallerySelectedBindings *b, const BkEndingGallerySelectedOps *o, char e[256]) {
  if ((uint32_t)*b->elapsed <= 10000) return 1;
  BkEndingGallerySelectedClip t;
  int32_t random;
  if (!s->alternate) {
    if (!CALL(clip, 7, &t, e)) return 0;
    if (!(t.source >= t.end)) return 1; /*unordered does not proceed*/
    add(&s->counter, 1); *b->elapsed = 0;
    if (s->counter == 2) {
      if (!CALL(random, &random, e)) return 0;
      s->countdown = (uint8_t)(random % 4 + 3); s->counter = 0;
      int32_t next;
      if (!record(b, (uint32_t)*b->cursor + 1u, &next, e)) return 0;
      if (next == 21) {
        *b->substate = 8;
        if (!CALL(request, 15, e) || !CALL(voice, 23, 0, 0, e)) return 0;
        auxiliary(s, b, 4);
        return CALL(fade, 0, e) && rewind_pair(o, 6, e) && rewind_pair(o, 10, e);
      }
      if (next >= 12 && next <= 14) {
        *b->substate = 7;
        if (!CALL(request, 8, e) || !CALL(voice, 6, 0, 0, e)) return 0;
        auxiliary(s, b, 1);
      }
    } else {
      if (!CALL(request, 13, e)) return 0;
      s->alternate = 1; auxiliary(s, b, 3);
      if (!CALL(random, &random, e)) return 0;
      s->countdown = (uint8_t)(random % 11 + 10);
      return CALL(voice, 9, 0, 0, e) && rewind_pair(o, 6, e);
    }
  } else {
    if (!CALL(clip, 11, &t, e)) return 0;
    if (t.source >= t.end) {
      if (!CALL(request, 14, e)) return 0;
      s->alternate = 0; auxiliary(s, b, 2);
      if (!CALL(random, &random, e)) return 0;
      s->countdown = (uint8_t)(random % 4 + 3);
      if (!CALL(voice, 10, 0, 0, e) || !rewind_pair(o, 10, e)) return 0;
      *b->elapsed = 0;
    }
  }
  return 1;
}
static int transition(BkEndingGallerySelectedState *s,
    const BkEndingGallerySelectedBindings *b, const BkEndingGallerySelectedOps *o, char e[256]) {
  int busy;
  BkEndingGallerySelectedClip t;
  if ((uint32_t)*b->elapsed > 15000) {
    if (!CALL(clip, 17, &t, e)) return 0;
    if (t.source == t.start || isnan(t.source) || isnan(t.start)) {
      b->frame->camera_request = 0; *b->open = 0;
      if (!playing(o, 1, &busy, e)) return 0;
      if (!busy) {
        *b->fade_stage = 3; *b->flash_wanted = 1;
        if (!CALL(request, 20, e) || !CALL(expression, 1, 4, 1, e) ||
            !CALL(voice, 26, 0, 0, e)) return 0;
        if (b->control->toggles[7] && !CALL(voice, 27, 1, 0, e)) return 0;
        int32_t current;
        if (!record(b, (uint32_t)*b->cursor, &current, e)) return 0;
        if (current >= 12 && current <= 14) b->auxiliary->selection = current - 12;
        unsigned r;
        if (!row(b, &r, e)) return 0;
        if (capability[r]) { s->view = 0; s->view_kind = 1; *b->substate = 9; }
        else { s->view = -1; s->view_kind = 0; *b->substate = 11; }
        if (!live_orbit(b, 0, e)) return 0;
        const int32_t *v;
        if (!camera_row(b, &v, e)) return 0;
        const unsigned source[] = {0,1,4,3};
        for (unsigned i = 0; i < 4; ++i) memcpy(&b->presets->active[i][0], v+source[i], 4);
        b->control->target_choice = 0; b->frame->camera_clip = 0;
        if (!target(b, o, 5, e)) return 0;
        memcpy(b->saved->target, b->frame->camera_values, 12); b->frame->camera_mode = 4;
        if (!stop_if_playing(o, 5, e)) return 0;
        s->counter = 0;
      }
    }
  }
  if (!playing(o, 0, &busy, e)) return 0;
  if (!busy && !b->counters[0]) {
    if (!CALL(voice, 24, 0, 1, e)) return 0;
    if (b->control->toggles[7] && !CALL(voice, 25, 1, 0, e)) return 0;
    b->counters[0] = 1;
  }
  return 1;
}
/*Return2 denotes a successful native early return, omitting the tail clock.*/
static int save_view(BkEndingGallerySelectedState *s,
    const BkEndingGallerySelectedBindings *b, const BkEndingGallerySelectedOps *o, char e[256]) {
  if (!target(b, o, 5, e) || !CALL(voice, 28, 0, 0, e)) return 0;
  b->frame->camera_mode = 4; s->view = 0; *b->substate = 11; s->view_kind = 1;
  b->saved->orbit[0] = b->camera->yaw; b->saved->orbit[1] = b->camera->pitch;
  b->saved->orbit[2] = b->camera->radius; b->saved->orbit[3] = b->camera->height;
  b->saved->toggle = b->control->toggles[1]; b->control->toggles[1] = 1;
  if (!table_orbit(b, 1, 0, e)) return 0;
  memcpy(b->saved->target, b->frame->camera_values, 12); return 2;
}
static int finishing(BkEndingGallerySelectedState *s,
    const BkEndingGallerySelectedBindings *b, const BkEndingGallerySelectedOps *o, char e[256]) {
  b->frame->camera_request = 0; *b->open = 0;
  if (!CALL(fov, .2f, e)) return 0;
  if (s->counter == 1) { b->frame->camera_mode = 0; add(&s->counter, 1); }
  if (b->frame->camera_mode && !s->counter) {
    uint32_t done;
    if (!blend(b, o, &done, e)) return 0;
    if (done & 255u) {
      if (!live_orbit(b, 1, e)) return 0;
      add(&s->counter, 1);
    }
  }
  int busy; int32_t active; BkEndingGallerySelectedClip t;
  switch (*b->opening) {
  case 0:
    if (!active_clip(o, &active, &t, e)) return 0;
    if (t.source >= t.end) {
      if (!CALL(source, active, t.end, e) || !playing(o, 1, &busy, e)) return 0;
      if (busy) break;
      if (!playing(o, 0, &busy, e)) return 0;
      if (busy) break;
      if (!finish_expression(b, o, e) || !CALL(request, 21, e)) return 0;
      *b->opening = 1;
      if (!stop_effects(o, e)) return 0;
    }
    break;
  case 1: {
    if (!CALL(active, &active, e)) return 0;
    if (active == 22) auxiliary(s, b, 5);
    int special = (b->frame->group == 0 || b->frame->group == 4) &&
                  b->auxiliary->variant == 0 &&
                  (b->auxiliary->selection == 0 || b->auxiliary->selection == 1);
    /*These are different native paths; the special path never examines
     *clip22 in this call, even if request21->22 has just completed.*/
    int32_t wanted = special ? 21 : 22;
    if (!CALL(active, &active, e)) return 0;
    if (active == wanted) {
      if (!CALL(clip, wanted, &t, e)) return 0;
      if (t.source >= t.end) {
        if (!playing(o, 0, &busy, e)) return 0;
        if (!busy) {
          if (!CALL(request, wanted + 1, e)) return 0;
          unsigned r;
          if (!row(b, &r, e)) return 0;
          if (capability[r]) return save_view(s, b, o, e);
        }
      }
    }
    if (!active_clip(o, &active, &t, e)) return 0;
    if (!t.chain && t.source >= t.end) {
      int32_t next; word(&next, (uint32_t)active + 1u);
      if (!CALL(request, next, e)) return 0;
    }
    if (!CALL(active, &active, e)) return 0;
    unsigned r;
    if (!row(b, &r, e)) return 0;
    if (active == terminal_clip[r]) {
      if (!b->auxiliary->variant && !CALL(expression, 6, 3, 1, e)) return 0;
      if (!CALL(voice, 29, 0, 1, e)) return 0;
      if (b->control->toggles[7] && !CALL(voice, 30, 1, 0, e)) return 0;
      b->auxiliary->pending = 29; *b->opening = 2;
    }
    break;
  }
  case 2:
    if (!playing(o, 1, &busy, e)) return 0;
    if (!busy) {
      if (!chains(o, 0, e)) return 0;
      *b->action = 0x31; *b->curtain_wanted = 1;
      *b->opening = 0; b->counters[0] = 0; s->counter = 0;
      *b->current_clock = 0; *b->previous_clock = 0; *b->elapsed = 0;
      return 2;
    }
    break;
  default: break;
  }
  return 1;
}
static int repeat(BkEndingGallerySelectedState *s,
    const BkEndingGallerySelectedBindings *b, const BkEndingGallerySelectedOps *o, char e[256]) {
  b->frame->camera_request = 0; *b->open = 0;
  int32_t active; BkEndingGallerySelectedClip t;
  if (!active_clip(o, &active, &t, e)) return 0;
  if (t.source >= t.end) {
    int busy;
    if (!playing(o, 0, &busy, e)) return 0;
    if (!busy && b->frame->camera_mode == 4) {
      b->control->toggles[1] = 1; add(&s->view, 1);
      if (s->view == (int)s->view_kind + 2) {
        *b->substate = 9; b->frame->camera_mode = 0;
        if (!CALL(active, &active, e)) return 0;
        int32_t next; word(&next, (uint32_t)active + 1u);
        if (!CALL(request, next, e)) return 0;
        if (s->view_kind == 0) {
          *b->opening = 0;
          if (!live_orbit(b, 1, e) || !finish_expression(b, o, e)) return 0;
          b->auxiliary->pending = 28;
          if (!stop_effects(o, e)) return 0;
        } else { *b->opening = 1; orbit(b->camera, b->saved->orbit); }
        memcpy(b->frame->camera_values, b->saved->target, 12);
        s->view = 0; b->control->toggles[1] = b->saved->toggle;
        return 2;
      }
      if (!CALL(active, &active, e) || !CALL(restart, active, e)) return 0;
      if (!s->view_kind) {
        if (!CALL(voice, 26, 0, 0, e) || !target(b, o, 13, e) ||
            !table_orbit(b, 0, s->view, e)) return 0;
      } else {
        if (!CALL(voice, 28, 0, 0, e) || !table_orbit(b, 1, s->view, e)) return 0;
      }
    }
  }
  if (!s->view_kind && b->frame->camera_mode && !s->counter) {
    uint32_t done;
    if (!blend(b, o, &done, e)) return 0;
    if (done & 255u) {
      if (!live_orbit(b, 1, e)) return 0;
      add(&s->counter, 1); b->frame->camera_mode = 4;
    }
  }
  return 1;
}
int bk_ending_gallery_selected_step(BkEndingGallerySelectedState *s,
    const BkEndingGallerySelectedBindings *b, float seconds,
    const BkEndingGallerySelectedOps *o, char e[256]) {
  if (!s || !b || !o || !b->frame || !b->control || !b->auxiliary ||
      !b->camera || !b->presets || !b->saved || !b->substate || !b->opening ||
      !b->cursor || !b->workspace || !b->counters || !b->camera_words ||
      !b->previous_clock || !b->current_clock || !b->elapsed || !b->open ||
      !b->expression_override || !b->face_mode || !b->fade_stage || !b->flash_wanted ||
      !b->action || !b->curtain_wanted || !b->speech_name ||
      !b->voice_volume || !b->effect_volume || !isfinite(seconds) || seconds < 0.f)
    return fail(e, "invalid bindings or elapsed time");
  uint32_t now;
  if (!CALL(clock, &now, e)) return 0;
  word(b->current_clock, now);
  add(b->elapsed, *b->previous_clock ? now - (uint32_t)*b->previous_clock : 0);
  s->base = b->auxiliary->variant ? 90 : 36;
  int result = 1, busy; int32_t active; BkEndingGallerySelectedClip t;
  switch (*b->substate) {
  case 0: result = opening(s, b, seconds, o, e); break;
  case 1:
    if ((uint32_t)*b->elapsed > 3000) {
      if (!CALL(request, 2, e) || !CALL(voice, 1, 0, 0, e)) return 0;
      *b->substate = 2; *b->elapsed = 0;
    }
    break;
  case 2:
    if (!playing(o, 0, &busy, e)) return 0;
    if (!busy) {
      if (!CALL(clip, 2, &t, e)) return 0;
      if (t.source >= t.end) {
        if (!CALL(request, 3, e) || !CALL(voice, 2, 0, 0, e)) return 0;
        *b->substate = 4;
      }
    }
    break;
  case 4:
    if (!playing(o, 0, &busy, e)) return 0;
    if (!busy) {
      if (b->control->toggles[7] && !CALL(voice, 3, 1, 0, e)) return 0;
      *b->substate = 5;
    }
    break;
  case 5:
    if (!playing(o, 1, &busy, e)) return 0;
    if (!busy) {
      if (!CALL(active, &active, e)) return 0;
      if (active == 4) {
        if (!CALL(request, 5, e) || !CALL(voice, 5, 0, 0, e) ||
            !chains(o, 1, e) || !CALL(play, 5, 1, *b->effect_volume, e)) return 0;
      } else { *b->substate = 6; *b->elapsed = 0; }
    }
    break;
  case 6: result = cycle(s, b, o, e); break;
  case 7:
    if (!playing(o, 0, &busy, e)) return 0;
    if (!busy) {
      if (!CALL(active, &active, e)) return 0;
      if (active == 4) { add(b->cursor, 1); b->frame->state_721ee0 = 0; }
    }
    break;
  case 8: result = transition(s, b, o, e); break;
  case 9: result = finishing(s, b, o, e); break;
  case 11: result = repeat(s, b, o, e); break;
  default: break;
  }
  if (!result) return 0;
  if (result == 2) return 1;
  if (!CALL(clock, &now, e)) return 0;
  word(b->previous_clock, now); return 1;
}
#undef CALL
