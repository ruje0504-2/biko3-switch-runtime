#include "game/ending_tertiary_action.h"
#include "core/random.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static int fail(char e[256], const char *why) {
  if (e) snprintf(e, 256, "tertiary ending action: %s", why);
  return 0;
}
#define CALL(name, ...)                                                        \
  do {                                                                        \
    if (!o->control.name) return fail(e, "missing " #name " service");         \
    if (!o->control.name(o->control.context, __VA_ARGS__)) return 0;             \
  } while (0)
#define EXTRA(name, ...)                                                       \
  do {                                                                        \
    if (!o->name) return fail(e, "missing " #name " service");                 \
    if (!o->name(o->control.context, __VA_ARGS__)) return 0;                     \
  } while (0)

static int point(const BkEndingTertiaryControlBindings *b,
                  int32_t out[2], char e[256]) {
  int32_t selected = b->frame->camera_cached;
  if (!b->targets || selected < 0 || selected >= 39)
    return fail(e, "selected target outside projected table");
  memcpy(out, b->targets[selected], sizeof(int32_t) * 2);
  return 1;
}
int bk_ending_tertiary_action_begin(const BkEndingTertiaryActionBindings *a,
                                    const BkEndingTertiaryActionOps *o,
                                    char e[256]) {
  if (!a || !a->control.frame || !a->choices || !o)
    return fail(e, "missing menu owners");
  const BkEndingTertiaryControlBindings *b = &a->control;
  int32_t active, choice = -1;
  CALL(active, &active, e);
  if (active == 2) choice = 0;
  else {
    if (!b->actions || !b->unavailable)
      return fail(e, "missing menu action table/availability");
    if (b->frame->camera_cached == b->actions[0]) choice = 1;
    else if (b->frame->camera_cached == b->actions[5] && !b->unavailable[0])
      choice = 2;
    else if (b->frame->camera_cached == b->actions[10] && !b->unavailable[1])
      choice = 3;
  }
  if (choice >= 0) {
    a->choices[0] = choice;
    a->choices[1] = a->choices[2] = -1;
  }
  int32_t center[2], zone;
  if (!point(b, center, e)) return 0;
  CALL(choose, center, &zone, e);
  if (zone >= 0 && zone <= 8) {
    static const int32_t angle[9] = {0, 180, 270, 90, 0, 225, 45, 135, 315};
    if (!point(b, center, e)) return 0;
    EXTRA(place_menu, angle[zone], 90, center, e);
  }
  return 1;
}
static int special(const BkEndingTertiaryControlBindings *b) {
  return b->frame->group == 2 || b->frame->group == 4;
}
static int expression(const BkEndingTertiaryControlBindings *b,
                       const BkEndingTertiaryActionOps *o, char e[256]) {
  CALL(expression, special(b) ? 3 : 0, 4, 0, e);
  return 1;
}
static int cancelled_expression(const BkEndingTertiaryControlBindings *b,
                                 const BkEndingTertiaryActionOps *o,
                                 char e[256]) {
  if (special(b)) {
    CALL(expression, 7, 6, 1, e);
  } else if (b->auxiliary->progress < .2f) {
    CALL(expression, 7, 3, 1, e);
    b->voice_latches[2] = 0;
  } else if (b->auxiliary->progress >= .19f) {
    CALL(expression, 5, 4, 0, e);
    *b->expression_override = 5;
    b->voice_latches[2] = 0;
    *b->face_mode = 1;
  }
  return 1;
}
static int progress(const BkEndingTertiaryActionBindings *a, char e[256]) {
  if (!a->gauge_y || !a->scale || !isfinite(*a->gauge_y) ||
      !isfinite(*a->scale) || !isfinite(a->control.auxiliary->progress))
    return fail(e, "invalid progress gauge");
  float value = (float)((double)a->control.auxiliary->progress + (double).1f);
  float y = (float)((double)*a->gauge_y - 20.0 * *a->scale);
  if (!isfinite(value) || !isfinite(y)) return fail(e, "progress overflow");
  a->control.auxiliary->progress = value;
  *a->gauge_y = y;
  return 1;
}
static int material(const BkEndingTertiaryControlBindings *b,
                     const BkEndingTertiaryActionOps *o,
                     BkEndingTertiaryActionMaterial table, unsigned slot,
                     int hidden, float alpha, char e[256]) {
  if (b->frame->group >= 5) return fail(e, "material group outside table");
  EXTRA(material, table, b->frame->group, slot, hidden, alpha, e);
  return 1;
}
static int appearance_pair(const BkEndingTertiaryControlBindings *b,
                            const BkEndingTertiaryActionOps *o, int hidden,
                            char e[256]) {
  return material(b, o, BK_ENDING_TERTIARY_ACTION_FOUR, 2, hidden, 1, e) &&
         material(b, o, BK_ENDING_TERTIARY_ACTION_FOUR, 3, hidden, .2f, e) &&
         material(b, o, BK_ENDING_TERTIARY_ACTION_PAIR, 0, !hidden, 1, e) &&
         material(b, o, BK_ENDING_TERTIARY_ACTION_PAIR, 1, !hidden, .2f, e);
}
static void decrement(BkEndingRecord *r) {
  uint32_t bits = (uint32_t)r->count - 1u;
  memcpy(&r->count, &bits, sizeof(bits));
}
static int replay(BkEndingTertiaryActionState *s,
                    const BkEndingTertiaryControlBindings *b, float seconds,
                    unsigned range, unsigned base,
                    const BkEndingTertiaryActionOps *o, char e[256]) {
  float elapsed = (float)((double)s->replay_elapsed + seconds);
  if (!isfinite(elapsed)) return fail(e, "replay clock overflow");
  s->replay_elapsed = elapsed;
  if ((double)s->replay_after <= elapsed) {
    CALL(play, 0, *b->voice_volume, e);
    s->replay_elapsed = 0;
    s->replay_after = (int32_t)(bk_random_next(b->random) % range + base);
  }
  return 1;
}
static int selection(BkEndingTertiaryActionState *s,
                       const BkEndingTertiaryActionBindings *a,
                       const BkEndingFrameInput *input, float seconds,
                       BkEndingRecord *record, int effect_index,
                       const BkEndingTertiaryActionOps *o, char e[256]) {
  const BkEndingTertiaryControlBindings *b = &a->control;
  if (!replay(s, b, seconds, 21, 30, o, e)) return 0;
  int32_t center[2], pointer[2], active;
  memcpy(pointer, &input->words[9], sizeof(pointer));
  if (!point(b, center, e)) return 0;
  EXTRA(manual, input, center, e);
  uint32_t key;
  CALL(key, 0, 3, &key, e);
  if (!(key & 255u)) return 1;
  b->frame->camera_event = 0;
  s->replay_elapsed = 0;
  s->replay_after = 10;
  for (unsigned i = 0; i < 3; ++i) {
    int hit;
    EXTRA(hit, i, pointer, &hit, e);
    if (!hit) continue;
    *b->state = b->frame->camera_mode = 0;
    *b->part_mode = 1;
    b->auxiliary->index = 55;
    if (!*b->return_ready && !progress(a, e)) return 0;
    CALL(active, &active, e);
    if (active == 2) {
      CALL(request, 0, 3, e);
      CALL(voice, 4, 0, 0, 1, e);
      if (b->frame->group != 0) {
        if (effect_index < 0) return fail(e, "uninitialized native group0 effect index");
        EXTRA(effect_slot, (unsigned)effect_index, *b->effect_volume, e);
      }
      if (!expression(b, o, e)) return 0;
      b->voice_latches[2] = *b->face_mode = 0;
      CALL(play, 0, *b->voice_volume, e);
    } else if (active == 5) {
      if (!special(b)) EXTRA(effect_slot, 4, *b->effect_volume, e);
      CALL(request, 0, 6, e);
      CALL(voice, 5, 0, 0, 0, e);
      CALL(play, 0, *b->voice_volume, e);
      if (special(b)) {
        CALL(expression, 3, 4, 0, e);
        b->voice_latches[2] = *b->face_mode = 0;
      } else {
        CALL(expression, 0, 4, 0, e);
        *b->expression_override = b->voice_latches[2] = 0;
        *b->face_mode = 1;
      }
      b->auxiliary->pending = 2;
      *b->return_ready = 1;
      if (b->frame->group == 1 && !b->control->toggles[3]) {
        if (!appearance_pair(b, o, 1, e)) return 0;
        b->control->toggles[3] = 1;
      }
      *b->pending_effect = 0;
      b->frame->camera_cached = -1;
    }
    return 1;
  }
  CALL(active, &active, e);
  if (active == 2) {
    b->auxiliary->index = 54;
    CALL(request, 0, 1, e);
    CALL(expression, 7, special(b) ? 6 : 3, 1, e);
  } else if (active == 5) {
    b->auxiliary->index = 55;
    CALL(request, 0, 4, e);
    if (!cancelled_expression(b, o, e)) return 0;
    decrement(record);
    if (!b->control->toggles[5] && b->frame->group == 1 &&
        !appearance_pair(b, o, 0, e)) return 0;
  }
  b->frame->camera_cached = -1;
  b->frame->camera_mode = *b->state = 0;
  return 1;
}
static int movement(BkEndingTertiaryActionState *s,
                      const BkEndingTertiaryActionBindings *a,
                      const BkEndingFrameInput *input, float seconds,
                      BkEndingRecord *record,
                      const BkEndingTertiaryActionOps *o, char e[256]) {
  const BkEndingTertiaryControlBindings *b = &a->control;
  if (!replay(s, b, seconds, 11, 10, o, e)) return 0;
  EXTRA(drag, 0, &input->words[6], 0, e);
  int32_t active;
  if (b->frame->group == 2) {
    CALL(active, &active, e);
    if (active == 7) EXTRA(drag, 1, &input->words[6], 1, e);
    else if (active == 9) EXTRA(drag, 2, &input->words[6], 1, e);
  }
  uint32_t key;
  CALL(key, 0, 3, &key, e);
  if (!(key & 255u)) return 1;
  s->replay_elapsed = 0;
  s->replay_after = 10;
  b->frame->camera_event = 0;
  b->auxiliary->index = 55;
  int hit = 0;
  if (*b->movement_ready) {
    int32_t pointer[2];
    memcpy(pointer, &input->words[9], sizeof(pointer));
    EXTRA(hit, 0, pointer, &hit, e);
  }
  if (hit) {
    CALL(active, &active, e);
    if (active == 7) {
      unsigned group = b->frame->group;
      if (group < 5) {
        unsigned slot = group == 0 ? 39 : group == 4 ? 38 : 5;
        EXTRA(effect_slot, slot, *b->effect_volume, e);
      }
      CALL(request, 0, 8, e);
      if (b->frame->group == 2) CALL(request, 1, 8, e);
      CALL(voice, 8, 0, 0, 0, e);
      *b->face_mode = b->voice_latches[2] = 0;
      CALL(expression, 2, 3, 1, e);
      CALL(play, 0, *b->voice_volume, e);
      b->auxiliary->pending = 3;
      b->unavailable[0] = 1;
      *b->pending_effect = 13;
    } else if (active == 9) {
      unsigned group = b->frame->group;
      if (group == 0 || group == 3) EXTRA(effect_slot, 5, *b->effect_volume, e);
      else if (group == 1) EXTRA(effect_slot, 38, *b->effect_volume, e);
      else if (group == 4) EXTRA(effect_slot, 39, *b->effect_volume, e);
      CALL(request, 0, 10, e);
      if (b->frame->group == 2) CALL(request, 2, 10, e);
      CALL(voice, 11, 0, 0, 0, e);
      CALL(play, 0, *b->voice_volume, e);
      *b->face_mode = b->voice_latches[2] = 0;
      if (!expression(b, o, e)) return 0;
      b->auxiliary->pending = 4;
      b->unavailable[1] = 1;
      *b->pending_effect = 5;
    }
    if (!progress(a, e)) return 0;
    *b->state = b->frame->camera_mode = 0;
    *b->part_mode = 1;
    b->frame->camera_cached = -1;
    return 1;
  }
  CALL(active, &active, e);
  if (active == 7 || active == 9) {
    unsigned first = active == 7 ? 0 : 3;
    for (unsigned i = 0; i < 3; ++i)
      if (!material(b, o, BK_ENDING_TERTIARY_ACTION_SIX,
                      first + i, i < 2, 1, e)) return 0;
  }
  CALL(request, 0, 4, e);
  decrement(record);
  if (b->frame->group == 2) {
    EXTRA(rewind, 1, 7, e);
    EXTRA(rewind, 2, 9, e);
    EXTRA(refresh, 1, e);
    EXTRA(refresh, 2, e);
    CALL(hidden, 1, 1, e);
    CALL(hidden, 2, 1, e);
  }
  if (!cancelled_expression(b, o, e)) return 0;
  b->frame->camera_mode = *b->state = 0;
  return 1;
}
int bk_ending_tertiary_action_step(BkEndingTertiaryActionState *s,
                                   const BkEndingTertiaryActionBindings *a,
                                   const BkEndingFrameInput *input, float seconds,
                                   const BkEndingTertiaryActionOps *o,
                                   char e[256]) {
  if (!s || !a || !input || !o || !isfinite(seconds) || seconds < 0)
    return fail(e, "invalid action input");
  const BkEndingTertiaryControlBindings *b = &a->control;
  if (!b->frame || !b->control || !b->auxiliary || !b->records ||
      b->frame->group >= 5 || !b->state || !b->face_mode || !b->part_mode ||
      !b->voice_latches || !b->return_ready || !b->unavailable ||
      !b->movement_ready || !b->expression_override || !b->pending_effect ||
      !b->voice_volume || !b->effect_volume || !b->random)
    return fail(e, "missing live action owners");
  BkEndingRecord *record = &b->records->groups[b->frame->group];
  static const int effect_index[5] = {-1, 8, 9, 7, 6};
  int captured_effect = effect_index[b->frame->group];
  int32_t mode = *b->part_mode;
  BkEndingFrameInput captured = *input;
  if (mode == 0)
    return selection(s, a, &captured, seconds, record, captured_effect, o, e);
  if (mode == 1)
    return movement(s, a, &captured, seconds, record, o, e);
  return 1;
}
