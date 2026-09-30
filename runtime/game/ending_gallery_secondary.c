#include "game/ending_gallery_secondary.h"
#include "game/ending_secondary_control.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
static int fail(char e[256], const char *why) {
  if (e) snprintf(e, 256, "gallery secondary: %s", why);
  return 0;
}
BkEndingGalleryCameraState bk_ending_gallery_camera_initial(void) {
  return (BkEndingGalleryCameraState){{0}, 0};
}
BkEndingGallerySecondaryState bk_ending_gallery_secondary_initial(void) {
  return (BkEndingGallerySecondaryState){5000, 1, 1.f, 0};
}
#define CALL(name, ...) \
  (o->name ? o->name(o->context, __VA_ARGS__) : fail(e, "missing " #name " service"))
static void word(int32_t *p, uint32_t bits) { memcpy(p, &bits, sizeof(bits)); }
static void add(int32_t *p, uint32_t amount) { word(p, (uint32_t)*p + amount); }
static int playing(const BkEndingGallerySecondaryOps *o, unsigned slot, int *busy, char e[256]) {
  int exists;
  if (!CALL(present, slot, &exists, e)) return 0;
  if (!exists) { *busy = 0; return 1; }
  return CALL(status, slot, busy, e);
}
static int timing(const BkEndingGallerySecondaryOps *o,
                  BkEndingGallerySecondaryClip *t, int32_t *active, char e[256]) {
  return CALL(active, active, e) && CALL(clip, *active, t, e);
}
static int target(const BkEndingGallerySecondaryBindings *b,
                  const BkEndingGallerySecondaryOps *o, char e[256]) {
  uint32_t position[3];
  if (!CALL(target, position, e)) return 0;
  memcpy(b->frame->camera_values, position, sizeof(position));
  return 1;
}
static int ordinary_expression(const BkEndingGallerySecondaryBindings *b,
    const BkEndingGallerySecondaryOps *o, char e[256]) {
  return b->frame->group == 1 ? CALL(expression, 6, 3, 1, e)
                             : CALL(expression, 5, 8, 1, e);
}
static int ending_expression(const BkEndingGallerySecondaryBindings *b,
    const BkEndingGallerySecondaryOps *o, char e[256]) {
  return CALL(expression, 7, b->frame->group == 3 || b->frame->group == 4 ? 1 : 3, 1, e);
}
static int preset(const BkEndingGallerySecondaryBindings *b, int32_t index, char e[256]) {
  /*553a80 and54e194 contain identical60float words; the oracle verifies
   *all bytes before reusing the existing checked table.*/
  const float *v = bk_ending_secondary_control_camera(b->frame->group, (unsigned)index);
  if (!v) return fail(e, "camera preset outside table");
  b->camera->yaw = v[0]; b->camera->pitch = v[1];
  b->camera->radius = v[2]; b->camera->height = v[3];
  return 1;
}
static void save_toggle(const BkEndingGallerySecondaryBindings *b) {
  b->saved->toggle = b->control->toggles[1];
  b->control->toggles[1] = 1;
}
static int save_view(const BkEndingGallerySecondaryBindings *b, char e[256]) {
  b->saved->orbit[0] = b->camera->yaw; b->saved->orbit[1] = b->camera->pitch;
  b->saved->orbit[2] = b->camera->radius; b->saved->orbit[3] = b->camera->height;
  return preset(b, 0, e);
}
static void use_view(const BkEndingGallerySecondaryBindings *b) {
  *b->substate = 7;
  b->frame->camera_mode = 4;
}
static int opening(BkEndingGallerySecondaryState *s,
    const BkEndingGallerySecondaryBindings *b, float seconds,
    const BkEndingGallerySecondaryOps *o, char e[256]) {
  if (*b->opening == 0) {
    *b->opening = 1;
    b->auxiliary->index = 18;
    s->fov = 1.f;
    return CALL(expression, 6, b->frame->group <= 1 ? 3 : 1, 1, e);
  }
  if (*b->opening == 1) {
    if (!target(b, o, e) || !CALL(fov, s->fov, e)) return 0;
    s->fov = (float)((double)s->fov - (double)seconds * .2f);
    int done = !(s->fov > .2f);
    if (done) s->fov = .2f;
    uint32_t result;
    if (b->frame->group >= 5) return fail(e, "camera group outside table");
    if (!CALL(camera, BK_ENDING_OPENING_PRESET, b->frame->camera_clip,
              b->frame->camera_values, b->frame->camera_table[b->frame->group][3],
              &result, e)) return 0;
    if (done && (result & 255u)) {
      *b->opening = 0;
      s->fov = 1.f;
      if (!CALL(voice, 1, 0, 0, e)) return 0;
      b->auxiliary->pending = 1;
      b->frame->camera_mode = 0;
      *b->substate = 1;
    }
  }
  return 1;
}
static int loop_voice(const BkEndingGallerySecondaryBindings *b,
    unsigned lane, const BkEndingGallerySecondaryOps *o, char e[256]) {
  int busy;
  int32_t random;
  if (!playing(o, 0, &busy, e)) return 0;
  if (busy) return 1;
  unsigned cue = lane ? 8 : 4;
  if (!CALL(voice, cue, 0, 1, e) || !CALL(random, &random, e)) return 0;
  if (!b->counters[lane]) {
    if (b->control->toggles[7] && !CALL(voice, cue + 1, 1, 0, e)) return 0;
    b->counters[lane] = 1;
  } else if (random % 1000 <= 25 && b->control->toggles[7]) {
    if (!CALL(voice, cue + 1, 1, 0, e)) return 0;
  }
  b->auxiliary->pending = lane ? 11 : 10;
  return 1;
}
static int cycle(BkEndingGallerySecondaryState *s,
    const BkEndingGallerySecondaryBindings *b,
    const BkEndingGallerySecondaryOps *o, char e[256]) {
  uint32_t now;
  if (!CALL(clock, &now, e)) return 0;
  word(b->current_clock, now);
  uint32_t delta = *b->previous_clock ? now - (uint32_t)*b->previous_clock : 0;
  add(b->elapsed, delta);
  if ((uint32_t)*b->elapsed > (uint32_t)s->remaining) {
    int cycles = s->cycles < 128 ? s->cycles : (int)s->cycles - 256;
    if (cycles >= 2) {
      if (!CALL(request, 11, e) || !CALL(expression, 0, 12, 1, e)) return 0;
      b->auxiliary->index = 21;
      *b->substate = 4;
      if (!CALL(voice, 13, 0, 0, e)) return 0;
      *b->elapsed = 0;
      s->cycles = 0;
      s->remaining = 20;
    } else {
      if (s->alternate) {
        if (!CALL(request, 8, e)) return 0;
        s->alternate = 0;
        if (!CALL(voice, 11, 0, 0, e) || !CALL(expression, 0, 9, 1, e)) return 0;
        b->auxiliary->index = 20;
        b->auxiliary->pending = 2;
        s->cycles++;
      } else {
        if (!CALL(request, 9, e)) return 0;
        s->alternate = 1;
        if (!CALL(voice, 12, 0, 0, e) || !ordinary_expression(b, o, e)) return 0;
        b->auxiliary->index = 19;
        b->auxiliary->pending = 3;
      }
      int32_t random;
      if (!CALL(random, &random, e)) return 0;
      /*sprintf("%d000", rand()%11+5), atoi: -5..15, no overflow.*/
      s->remaining = (random % 11 + 5) * 1000;
      *b->elapsed = 0;
    }
  }
  if (b->auxiliary->pending == 0 || b->auxiliary->pending == 3) {
    if (!loop_voice(b, 0, o, e)) return 0;
  } else if (b->auxiliary->pending == 1 || b->auxiliary->pending == 2) {
    if (!loop_voice(b, 1, o, e)) return 0;
  }
  if (!CALL(clock, &now, e)) return 0;
  word(b->previous_clock, now);
  return 1;
}
static int finishing(BkEndingGallerySecondaryState *s,
    const BkEndingGallerySecondaryBindings *b, float seconds,
    const BkEndingGallerySecondaryOps *o, char e[256]) {
  int32_t active;
  int busy;
  if (!CALL(active, &active, e)) return 0;
  if (active != 12) return 1;
  if (!playing(o, 0, &busy, e)) return 0;
  if (!busy) {
    if (!CALL(voice, 14, 0, 1, e)) return 0;
    return !b->control->toggles[7] || CALL(voice, 15, 1, 0, e);
  }
  BkEndingGallerySecondaryClip t;
  if (!timing(o, &t, &active, e)) return 0;
  double threshold = (double)t.end - (double)seconds * .3f * 60.f * 2.;
  if (threshold > (double)t.source) return 1; /*unordered proceeds*/
  add(&s->remaining, UINT32_MAX);
  if (s->remaining) return 1;
  if (!CALL(voice, 16, 0, 0, e) || !CALL(request, 13, e)) return 0;
  *b->opening = 0;
  s->remaining = 0;
  *b->substate = 5;
  if (b->frame->group == 1 || b->frame->group == 2) {
    if (!CALL(expression, 5, 3, 1, e)) return 0;
  } else if (!CALL(expression, 0, 13, 1, e)) return 0;
  if (b->frame->group == 0 || b->frame->group == 4) {
    save_toggle(b);
    if (!save_view(b, e)) return 0;
    use_view(b);
  } else *b->substate = 5;
  return 1;
}
static int ending(BkEndingGallerySecondaryState *s,
    const BkEndingGallerySecondaryBindings *b,
    const BkEndingGallerySecondaryOps *o, char e[256]) {
  BkEndingGallerySecondaryClip t;
  int32_t active;
  int busy;
  switch (*b->opening) {
  case 0:
    if (!timing(o, &t, &active, e)) return 0;
    if (!(t.source >= t.end)) break;
    if (!playing(o, 0, &busy, e)) return 0;
    if (busy) break;
    if (!CALL(voice, 17, 0, 0, e)) return 0;
    *b->opening = 1;
    if (!CALL(request, 14, e)) return 0;
    if (b->frame->group == 1 || b->frame->group == 2) {
      save_toggle(b);
      if (!CALL(expression, b->frame->group == 1 ? 5 : 0, 3, 1, e)) return 0;
      if (!save_view(b, e)) return 0;
      use_view(b);
    }
    break;
  case 1:
    if (!CALL(active, &active, e)) return 0;
    if (active == 14) {
      if (!CALL(clip, 14, &t, e)) return 0;
      if (t.source >= t.end) {
        if (!ending_expression(b, o, e) || !CALL(request, 15, e)) return 0;
        if (b->frame->group == 3) {
          save_toggle(b);
          if (!save_view(b, e) || !target(b, o, e)) return 0;
          use_view(b);
          return 1;
        }
      }
    }
    if (!timing(o, &t, &active, e)) return 0;
    if (!t.chain && !t.loop && t.source >= t.end) {
      int32_t next = active;
      add(&next, 1);
      if (!CALL(request, next, e)) return 0;
    }
    if (!CALL(active, &active, e)) return 0;
    if (active == 15 && !ending_expression(b, o, e)) return 0;
    if (!CALL(active, &active, e)) return 0;
    if (active == 16) {
      if (!playing(o, 0, &busy, e)) return 0;
      if (!busy) {
        *b->opening = 2;
        if (!CALL(voice, 18, 0, 0, e)) return 0;
      }
    }
    break;
  case 2:
    if (!playing(o, 0, &busy, e)) return 0;
    if (!busy) {
      s->remaining = 5000;
      s->alternate = 1;
      *b->opening = 0;
      memset(b->counters, 0, 10 * sizeof(*b->counters));
      add(b->cursor, 1);
      b->frame->state_721ee0 = 0;
      memset(b->saved->orbit, 0, sizeof(b->saved->orbit));
    }
    break;
  }
  return 1;
}
static int views(BkEndingGallerySecondaryState *s,
    const BkEndingGallerySecondaryBindings *b,
    const BkEndingGallerySecondaryOps *o, char e[256]) {
  BkEndingGallerySecondaryClip t;
  int32_t active;
  int busy;
  if (!timing(o, &t, &active, e)) return 0;
  if (!(t.source >= t.end)) return 1;
  if (!playing(o, 0, &busy, e)) return 0;
  if (busy) return 1;
  add(&s->remaining, 1);
  if (s->remaining == 3) {
    if (b->frame->group == 0 || b->frame->group == 4) {
      if (!CALL(voice, 17, 0, 0, e)) return 0;
      *b->opening = 1;
    } else if (b->frame->group == 1 || b->frame->group == 2) {
      if (b->frame->group == 2 && !CALL(expression, 5, 3, 1, e)) return 0;
      *b->opening = 1;
    } else {
      *b->opening = 2;
      if (!CALL(voice, 18, 0, 0, e)) return 0;
    }
    b->control->toggles[1] = b->saved->toggle;
    *b->substate = 5;
    if (!CALL(active, &active, e)) return 0;
    add(&active, 1);
    if (!CALL(request, active, e)) return 0;
    b->frame->camera_mode = 0;
    b->camera->yaw = b->saved->orbit[0]; b->camera->pitch = b->saved->orbit[1];
    b->camera->radius = b->saved->orbit[2]; b->camera->height = b->saved->orbit[3];
  } else {
    if (!CALL(active, &active, e) || !CALL(restart, active, e)) return 0;
    if (b->frame->group == 0 || b->frame->group == 4) {
      if (!CALL(voice, 16, 0, 0, e)) return 0;
    } else if (b->frame->group == 1 || b->frame->group == 2) {
      if (!CALL(voice, 17, 0, 0, e)) return 0;
    }
    return preset(b, s->remaining, e);
  }
  return 1;
}
int bk_ending_gallery_secondary_step(BkEndingGallerySecondaryState *s,
    const BkEndingGallerySecondaryBindings *b, float seconds,
    const BkEndingGallerySecondaryOps *o, char e[256]) {
  if (!s || !b || !b->frame || !b->control || !b->auxiliary || !b->camera ||
      !b->saved || !b->substate || !b->opening || !b->cursor || !b->counters ||
      !b->previous_clock || !b->current_clock || !b->elapsed || !o ||
      !isfinite(seconds) || seconds < 0) return fail(e, "invalid bindings/time");
  int busy;
  switch (*b->substate) {
  case 0: return opening(s, b, seconds, o, e);
  case 1:
    if (!playing(o, 0, &busy, e)) return 0;
    if (!busy && b->auxiliary->pending == 1) {
      if (b->control->toggles[7] && !CALL(voice, 2, 1, 0, e)) return 0;
      b->auxiliary->pending = 3;
    }
    if (!playing(o, 0, &busy, e)) return 0;
    if (busy) break;
    if (!playing(o, 1, &busy, e)) return 0;
    if (busy) break;
    if (!CALL(voice, 3, 0, 0, e) || !CALL(request, 2, e) || !ordinary_expression(b, o, e)) return 0;
    b->auxiliary->index = 19;
    *b->substate = 2;
    break;
  case 2:
    if (!playing(o, 0, &busy, e)) return 0;
    if (!busy) {
      if (!CALL(voice, 4, 0, 1, e)) return 0;
      b->auxiliary->pending = 4;
      if (b->control->toggles[7] && !CALL(voice, 5, 1, 0, e)) return 0;
      b->counters[0] = 1;
      *b->substate = 3;
    }
    break;
  case 3: return cycle(s, b, o, e);
  case 4: return finishing(s, b, seconds, o, e);
  case 5: return ending(s, b, o, e);
  case 7: return views(s, b, o, e);
  }
  return 1;
}
#undef CALL
