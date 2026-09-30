#include "game/ending_gallery_control.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static int fail(char e[256], const char *why) {
  if (e) snprintf(e, 256, "gallery control: %s", why);
  return 0;
}
BkEndingGalleryControlState bk_ending_gallery_control_initial(void) {
  return (BkEndingGalleryControlState){1.f};
}
#define CALL(name, ...) \
  (o->name ? o->name(o->context, __VA_ARGS__) : fail(e, "missing " #name " service"))
static int playing(const BkEndingGalleryControlOps *o, unsigned slot,
                    int *busy, char e[256]) {
  int exists;
  if (!CALL(present, slot, &exists, e)) return 0;
  if (!exists) { *busy = 0; return 1; }
  return CALL(status, slot, busy, e);
}
static int recorded(const BkEndingGalleryControlBindings *b, int32_t *out,
                      char e[256]) {
  if ((uint32_t)*b->cursor >= b->workspace_capacity)
    return fail(e, "workspace cursor outside capacity");
  *out = b->workspace[*b->cursor];
  return 1;
}
static int selection(const BkEndingGalleryControlBindings *b, char e[256]) {
  int32_t value;
  if (!recorded(b, &value, e)) return 0;
  if (value >= 12 && value <= 14) b->auxiliary->selection = value - 12;
  return 1;
}
static void classify(const BkEndingGalleryControlBindings *b, int32_t value) {
  *b->previous = *b->action;
  unsigned byte = (uint32_t)value & 255u;
  if (byte <= 8) *b->action = 0;
  else if (byte <= 10) *b->action = 1;
  else if (byte <= 14) *b->action = b->auxiliary->variant ? 5 : 2;
  else if (byte <= 18) *b->action = 3;
  else if (byte <= 20) *b->action = 4;
  else if (byte == 21) *b->action = 7;
  /*All remaining signed low-byte values retain the action.*/
}
static int named(const BkEndingGalleryControlBindings *b,
                   const BkEndingGalleryControlOps *o, const char *suffix,
                   char e[256]) {
  char name[32];
  int group = b->frame->group;
  if (group >= 128) group -= 256;
  snprintf(name, sizeof(name), "PH%d%s.wav", group + 1, suffix);
  memcpy(b->speech_name, name, strlen(name) + 1);
  return CALL(load, 0, name, e) && CALL(play, 0, *b->voice_volume, e);
}
static int opening(BkEndingGalleryControlState *s,
    const BkEndingGalleryControlBindings *b, unsigned record_group, float seconds,
    const BkEndingGalleryControlOps *o, char e[256]) {
  uint32_t result;
  int busy, exists;
  switch (*b->opening) {
  case 0:
    *b->opening = 1;
    if (b->auxiliary->variant && (b->frame->group == 2 || b->frame->group == 4)) {
      if (!CALL(expression, 9, 2, 0, e)) return 0;
    } else if (!CALL(expression, 9, 6, 1, e)) return 0;
    memset(b->counters, 0, 10 * sizeof(*b->counters));
    *b->opening_counter = 0;
    s->fov = 1.f;
    break;
  case 1:
    if (!CALL(fov, s->fov, e)) return 0;
    if (seconds < 1.f) s->fov = (float)((double)s->fov - (double)seconds * .2f);
    if (!(s->fov > .2f)) s->fov = .2f;
    if (!CALL(camera, BK_ENDING_OPENING_TRACK, 0, (uint32_t[3]){0}, 0, &result, e)) return 0;
    if (result & 255u) {
      if (!CALL(fov, .2f, e)) return 0;
      s->fov = 1.f;
      *b->opening = 2;
      unsigned lane = b->auxiliary->variant != 0;
      *b->previous = *b->action = lane ? 3 : 0;
      if (record_group >= BK_ENDING_RECORD_GROUPS)
        return fail(e, "captured record group outside table");
      if (b->workspace_capacity < BK_ENDING_RECORD_CAPACITY)
        return fail(e, "workspace cannot hold original record lane");
      memcpy(b->workspace, b->records->groups[record_group].retained[lane],
             BK_ENDING_RECORD_CAPACITY * sizeof(*b->workspace));
      b->auxiliary->index = lane ? 54 : 0;
      *b->cursor = 0;
      uint32_t position[3];
      if (!CALL(target, position, e)) return 0;
      memcpy(b->frame->camera_values, position, sizeof(position));
    }
    break;
  case 2:
    if (b->frame->group >= 5) return fail(e, "camera group outside table");
    if (!CALL(camera, BK_ENDING_OPENING_PRESET, b->frame->camera_clip,
              b->frame->camera_values, b->frame->camera_table[b->frame->group][3],
              &result, e)) return 0;
    if (result & 255u) {
      *b->opening = 3;
      b->frame->camera_mode = 0;
      if (!named(b, o, b->auxiliary->variant ? "3101" : "0001", e)) return 0;
      b->auxiliary->pending = 0;
    }
    break;
  case 3:
    if (!playing(o, 0, &busy, e)) return 0;
    if (busy) break;
    if (!CALL(present, 1, &exists, e)) return 0;
    if (!exists && b->control->toggles[7]) {
      if (!b->auxiliary->variant) {
        char name[32] = {0};
        if (!CALL(voice_name, name, e)) return 0;
        if (!memchr(name, 0, sizeof(name))) return fail(e, "unterminated voice name");
        if (!CALL(load, 1, name, e)) return 0;
      } else if (!CALL(voice, 2, 1, 0, 1, e)) return 0;
      if (!CALL(play, 1, *b->voice_volume, e)) return 0;
      break;
    }
    if (!playing(o, 1, &busy, e)) return 0;
    if (!busy) {
      *b->step_counter = 0;
      b->frame->state_721ee0 = 0;
      *b->opening = 0;
    }
    break;
  }
  return 1;
}
static int dispatch(const BkEndingGalleryControlBindings *b,
    const BkEndingGalleryControlOps *o, char e[256]) {
  int32_t value;
  if (!recorded(b, &value, e)) return 0;
  classify(b, value);
  if (*b->previous != *b->action) {
    b->frame->state_721ee0 = 2;
    memset(b->counters, 0, 10 * sizeof(*b->counters));
    switch (*b->action) {
    case 1: return named(b, o, "0261", e);
    case 2: return named(b, o, "0260", e);
    case 4:
      *b->transition_latch = 0;
      return named(b, o, "3213", e);
    case 5:
      if (!named(b, o, "3311", e)) return 0;
      *b->transition_latch = 0;
      break;
    }
  } else {
    switch (*b->action) {
    case 0: b->frame->state_721ee0 = 4; *b->normal = 1; break;
    case 1: b->frame->state_721ee0 = 5; *b->secondary = 1; break;
    case 3: b->frame->state_721ee0 = 7; *b->tertiary = 1; break;
    case 4: b->frame->state_721ee0 = 8; *b->special = 1; break;
    case 2:
    case 5: {
      b->frame->state_721ee0 = 2;
      if (!selection(b, e)) return 0;
      uint32_t raw = (uint32_t)b->auxiliary->selection + 1u;
      int32_t cue;
      memcpy(&cue, &raw, sizeof(cue));
      return CALL(cue, cue, 1, 0, 0, e);
    }
    }
  }
  return 1;
}
static int reload(const BkEndingGalleryControlBindings *b,
    const BkEndingGalleryControlOps *o, char e[256]) {
  switch (*b->action) {
  case 0: b->frame->transition_action = 6; *b->next_mode = 1; *b->normal = 0; break;
  case 1: b->frame->transition_action = 6; *b->next_mode = 0; *b->secondary = 0; break;
  case 4: b->frame->transition_action = 8; *b->special = 0; break;
  case 2:
  case 5:
    b->frame->transition_action = 7;
    if (b->auxiliary->variant) {
      if (*b->previous == 4) b->frame->state_721ee4 = 1000;
      else if (*b->previous == 3) b->frame->state_721ee4 = 1500;
    }
    if (!selection(b, e)) return 0;
    *b->selected = 0;
    break;
  case 7: b->frame->transition_action = 0x31; break;
  }
  int busy;
  if (!playing(o, 0, &busy, e)) return 0;
  if (!busy) b->frame->curtain_wanted = 1;
  return 1;
}
int bk_ending_gallery_control_step(BkEndingGalleryControlState *s,
    const BkEndingGalleryControlBindings *b, float seconds,
    const BkEndingGalleryControlOps *o, char e[256]) {
  if (!s || !b || !b->frame || !b->control || !b->auxiliary || !b->records ||
      !b->action || !b->previous || !b->opening || !b->normal || !b->secondary ||
      !b->tertiary || !b->special || !b->transition_latch || !b->selected ||
      !b->cursor || !b->workspace || !b->counters || !b->opening_counter ||
      !b->step_counter || !b->next_mode || !b->speech_name || !b->voice_volume ||
      !o || !isfinite(seconds) || seconds < 0)
    return fail(e, "invalid bindings/time");
  unsigned group = b->frame->group;
  b->frame->camera_request = 1;
  switch (b->frame->state_721ee0) {
  case 0: return dispatch(b, o, e);
  case 2: return reload(b, o, e);
  case 3: return opening(s, b, group, seconds, o, e);
  case 4: case 5: case 6: case 7: case 8:
    return CALL(child, (unsigned)b->frame->state_721ee0, e);
  }
  return 1;
}
#undef CALL
