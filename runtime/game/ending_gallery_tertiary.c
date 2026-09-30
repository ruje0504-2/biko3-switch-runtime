#include "game/ending_gallery_tertiary.h"
#include "game/ending_tertiary.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
static int fail(char e[256], const char *why) {
  if (e) snprintf(e, 256, "gallery tertiary: %s", why);
  return 0;
}
#define CALL(name, ...) \
  (o->name ? o->name(o->context, __VA_ARGS__) : fail(e, "missing " #name " service"))
static int signed_byte(uint8_t x) { return x < 128 ? x : (int)x - 256; }
static void increment(int32_t *p) { uint32_t u = (uint32_t)*p + 1u; memcpy(p, &u, 4); }
static int equal(float a, float b) { return !(a < b || a > b); } /*x87 C3, unordered too*/
static int record(const BkEndingGalleryTertiaryBindings *b, int32_t *out, char e[256]) {
  if ((uint32_t)*b->cursor >= b->workspace_capacity) return fail(e, "record cursor outside workspace");
  *out = b->workspace[*b->cursor]; return 1;
}
static int playing(const BkEndingGalleryTertiaryOps *o, unsigned slot, int *busy, char e[256]) {
  int exists;
  if (!CALL(present, slot, &exists, e)) return 0;
  if (!exists) { *busy = 0; return 1; }
  return CALL(status, slot, busy, e);
}
static int sound(const BkEndingGalleryTertiaryBindings *b,
    const BkEndingGalleryTertiaryOps *o, unsigned slot, char e[256]) {
  return CALL(play, slot, slot < 2 ? *b->voice_volume : *b->effect_volume, e);
}
static int material(const BkEndingGalleryTertiaryBindings *b,
    const BkEndingGalleryTertiaryOps *o, BkEndingTertiaryMaterialTable table,
    unsigned index, uint32_t hidden, float alpha, char e[256]) {
  /*54fcb4/551b2c/552554 equal548f88/54ae00/54b828, including empty names.*/
  const char *name = bk_ending_tertiary_material_name(table, b->frame->group, index);
  return name ? CALL(material, name, hidden, alpha, e) : fail(e, "material group outside table");
}
#define SIX(index,hidden) material(b,o,BK_ENDING_TERTIARY_MATERIAL_SIX,index,hidden,1.f,e)
#define PAIR(index,hidden,alpha) material(b,o,BK_ENDING_TERTIARY_MATERIAL_PAIR,index,hidden,alpha,e)
#define FOUR(index,hidden,alpha) material(b,o,BK_ENDING_TERTIARY_MATERIAL_FOUR,index,hidden,alpha,e)
static int begin(const BkEndingGalleryTertiaryBindings *b,
    const BkEndingGalleryTertiaryOps *o, char e[256]) {
  int32_t action;
  if (!record(b, &action, e)) return 0;
  if (action >= 15 && action <= 17) {
    if (!*b->transition_latch) {
      *b->clip = 2;
      if (!CALL(voice, 3, 0, 0, 1, e) || !sound(b, o, 0, e)) return 0;
      *b->face_mode = 0; *b->expression_latch = 0;
      if (!CALL(expression, 5, 4, 0, e)) return 0;
      b->auxiliary->index = 56;
    } else if (action == 15) {
      *b->clip = 5;
      if (!CALL(voice, 4, 0, 0, 0, e) || !sound(b, o, 0, e)) return 0;
      *b->face_mode = 0; *b->expression_latch = 0;
      if (!CALL(expression, 0, 4, 0, e)) return 0;
      b->auxiliary->index = 57;
    } else if (action == 16) {
      *b->clip = 7;
      if (!CALL(voice, 7, 0, 0, 0, e) || !sound(b, o, 0, e)) return 0;
      if (b->frame->group == 2 &&
          (!CALL(hidden, 1, 0, e) || !CALL(request, 1, 7, e))) return 0;
      if (b->frame->group == 2) {
        if (!SIX(0, 0)) return 0;
      } else {
        int special = b->frame->group == 3;
        if (!SIX(0, !special) || !SIX(1, special) || !SIX(2, 1)) return 0;
      }
      *b->face_mode = 0; *b->expression_latch = 0;
      if (!CALL(expression, 5, 3, 1, e)) return 0;
      b->auxiliary->index = 58;
    } else {
      *b->clip = 9;
      if (!CALL(voice, 10, 0, 0, 0, e) || !sound(b, o, 0, e)) return 0;
      if (b->frame->group == 2 &&
          (!CALL(hidden, 2, 0, e) || !CALL(request, 2, 9, e))) return 0;
      if (b->frame->group == 2) { if (!SIX(3, 0)) return 0; }
      else if (!SIX(3, 0) || !SIX(4, 1) || !SIX(5, 1)) return 0;
      *b->face_mode = 0; *b->expression_latch = 0;
      if (!CALL(expression, 5, 3, 1, e)) return 0;
      b->auxiliary->index = 59;
    }
    increment(&b->counters[2]);
  }
  if (!CALL(request, 0, signed_byte(*b->clip), e)) return 0;
  *b->substate = 2; *b->cycles = 0; return 1;
}
static int ordinary(const BkEndingGalleryTertiaryBindings *b, int entry_effect,
    const BkEndingGalleryTertiaryOps *o, char e[256]) {
  int32_t active; BkEndingClipTiming t;
  if (!CALL(active, &active, e) || !CALL(timing, active, &t, e)) return 0;
  if (!equal(t.source, t.end)) return 1;
  int busy;
  if (!playing(o, 0, &busy, e)) return 0;
  if (busy) return 1;
  if (!CALL(active, &active, e)) return 0;
  if (active == 2) {
    if (!CALL(voice, 4, 0, 0, 1, e)) return 0;
    if (b->frame->group) {
      /*The group selecting this local was captured on function entry,
       *before callbacks. Group0's uninitialized local is normally unused.*/
      if (entry_effect < 0) return fail(e, "uninitialized entry effect selector");
      if (!sound(b, o, (unsigned)entry_effect + 2u, e)) return 0;
    }
    if (!CALL(expression, 0, 4, 0, e)) return 0;
    *b->expression_latch = 0; *b->face_mode = 0;
    if (!sound(b, o, 0, e)) return 0;
  } else {
    if (!CALL(active, &active, e)) return 0;
    if (active == 5) {
      if (b->frame->group != 4 && b->frame->group != 2 && !sound(b, o, 6, e)) return 0;
      if (!CALL(voice, 5, 0, 0, 0, e) || !sound(b, o, 0, e) ||
          !CALL(expression, 6, 4, 0, e)) return 0;
      *b->expression_override = 6; *b->expression_latch = 0; *b->face_mode = 1;
      if (b->frame->group == 1 && !b->control->toggles[5]) {
        if (!FOUR(2, 1, 1.f) || !FOUR(3, 1, .2f) ||
            !PAIR(0, 0, 1.f) || !PAIR(1, 0, .2f)) return 0;
        b->control->toggles[5] = 1;
      }
    }
  }
  ++*b->clip; b->auxiliary->index = 55;
  if (!CALL(request, 0, signed_byte(*b->clip), e)) return 0;
  *b->substate = 3; return 1;
}
static int reversible(const BkEndingGalleryTertiaryBindings *b,
    const BkEndingGalleryTertiaryOps *o, char e[256]) {
  BkEndingClipTiming t;
  if (!CALL(timing, signed_byte(*b->clip), &t, e)) return 0;
  if (!(t.source >= t.end)) {
    if (!CALL(timing, signed_byte(*b->clip), &t, e)) return 0;
    if (!(t.source > t.start)) *b->reverse = 0; /*C3|C0 includes unordered*/
    return 1;
  }
  ++*b->cycles;
  if (signed_byte(*b->cycles) != 3) { *b->reverse = 1; return 1; }
  *b->cycles = 0;
  int32_t active;
  if (!CALL(active, &active, e)) return 0;
  if (active == 7) {
    if (b->frame->group == 0) { if (!sound(b, o, 41, e)) return 0; }
    else if (b->frame->group >= 1 && b->frame->group <= 3) { if (!sound(b, o, 7, e)) return 0; }
    else if (b->frame->group == 4 && !sound(b, o, 40, e)) return 0;
    if (b->frame->group == 2 && !CALL(request, 1, 8, e)) return 0;
    if (!CALL(voice, 8, 0, 0, 0, e)) return 0;
    *b->face_mode = 0; *b->expression_latch = 0;
    if (!CALL(expression, 4, 3, 1, e) || !sound(b, o, 0, e)) return 0;
  } else {
    if (!CALL(active, &active, e)) return 0;
    if (active == 9) {
      if (b->frame->group == 0 || b->frame->group == 3) { if (!sound(b, o, 7, e)) return 0; }
      else if (b->frame->group == 1) { if (!sound(b, o, 40, e)) return 0; }
      else if (b->frame->group == 4 && !sound(b, o, 41, e)) return 0;
      if (b->frame->group == 2 && !CALL(request, 2, 10, e)) return 0;
      if (!CALL(voice, 11, 0, 0, 0, e) || !sound(b, o, 0, e)) return 0;
      *b->face_mode = 0; *b->expression_latch = 0;
      if (!CALL(expression, 0, 4, 0, e)) return 0;
    }
  }
  ++*b->clip;
  if (!CALL(request, 0, signed_byte(*b->clip), e)) return 0;
  b->auxiliary->index = 55; *b->substate = 3; return 1;
}
static int finish_materials(const BkEndingGalleryTertiaryBindings *b,
    const BkEndingGalleryTertiaryOps *o, char e[256]) {
  int32_t action;
  if (!record(b, &action, e)) return 0;
  if (action == 15 && signed_byte(*b->transition_latch) == 1) {
    if (b->frame->group == 1 && (!PAIR(0, 1, 1.f) || !PAIR(1, 1, 1.f))) return 0;
  } else if (action == 16) {
    if (b->frame->group == 1 && (!SIX(0, 0) || !SIX(1, 1) || !SIX(2, 1))) return 0;
  } else if (action == 17) {
    if (b->frame->group == 1 && (!SIX(3, 1) || !SIX(4, 0) || !SIX(5, 1))) return 0;
  }
  return 1;
}
static int finish_voice(const BkEndingGalleryTertiaryBindings *b,
    const BkEndingGalleryTertiaryOps *o, char e[256]) {
  if (!b->control->toggles[7]) return 1;
  int32_t action;
  if (!record(b, &action, e)) return 0;
  if (action == 15 && signed_byte(*b->transition_latch) == 1) {
    int32_t random;
    if (!CALL(random, &random, e)) return 0;
    int chosen = random % 1000 <= 25;
    if (!b->counters[0]) {
      if (b->control->toggles[7]) {
        if (!CALL(voice, 6, 1, 0, 0, e) || !sound(b, o, 1, e)) return 0;
        b->counters[0] = 1;
      }
    } else if (chosen && b->control->toggles[7]) {
      if (!CALL(voice, 6, 1, 0, 0, e) || !sound(b, o, 1, e)) return 0;
    }
  } else if (action == 16 && b->control->toggles[7]) {
    if (!CALL(voice, 9, 1, 0, 0, e) || !sound(b, o, 1, e)) return 0;
  } else if (action == 17 && b->control->toggles[7]) {
    if (!CALL(voice, 12, 1, 0, 0, e) || !sound(b, o, 1, e)) return 0;
  }
  return 1;
}
static int finishing(const BkEndingGalleryTertiaryBindings *b,
    const BkEndingGalleryTertiaryOps *o, char e[256]) {
  int32_t active;
  if (!CALL(active, &active, e)) return 0;
  if (active == 3 && b->frame->group == 0) {
    b->frame->camera_mode = 2;
    uint32_t v[3];
    if (!CALL(target, v, e)) return 0;
    memcpy(b->frame->camera_values, v, 12); b->control->target_choice = 0;
  }
  if (b->frame->group == 2) {
    float elapsed; memcpy(&elapsed, b->elapsed_bits, 4);
    if (equal(elapsed, 0.f)) {
      if (!CALL(active, &active, e)) return 0;
      if (active == 10) {
        BkEndingClipTiming t;
        if (!CALL(timing, 10, &t, e)) return 0;
        if (t.source > 370.f) {
          int32_t action;
          if (!record(b, &action, e)) return 0;
          if (action == 17) {
            if (!sound(b, o, 40, e)) return 0;
            *b->elapsed_bits = INT32_C(0x3f800000);
          }
        }
      }
    }
  }
  if (!CALL(active, &active, e)) return 0;
  if (active != 4) return 1;
  int busy;
  if (!playing(o, 0, &busy, e)) return 0;
  if (busy) return 1;
  if (!finish_materials(b, o, e) || !finish_voice(b, o, e)) return 0;
  if (!b->counters[1]) {
    if (b->counters[2] < 2) {
      if (!CALL(expression, 7, 3, 1, e)) return 0;
      b->counters[2] = 0; *b->expression_latch = 0;
    } else if (b->auxiliary->progress >= .19f) {
      if (!CALL(expression, 4, 4, 0, e)) return 0;
      *b->expression_override = 4; *b->face_mode = 1; *b->expression_latch = 0;
    }
    b->counters[1] = 1;
  }
  *b->substate = 4; return 1;
}
int bk_ending_gallery_tertiary_step(const BkEndingGalleryTertiaryBindings *b,
    const BkEndingGalleryTertiaryOps *o, char e[256]) {
  if (!b || !o || !b->frame || !b->control || !b->auxiliary || !b->substate ||
      !b->clip || !b->transition_latch || !b->cycles || !b->expression_latch ||
      !b->cursor || !b->workspace || !b->counters || !b->elapsed_bits ||
      !b->reverse || !b->face_mode || !b->expression_override ||
      !b->voice_volume || !b->effect_volume) return fail(e, "invalid live bindings");
  int entry_effect = -1;
  switch (b->frame->group) {
  case 1: entry_effect = 8; break;
  case 2: entry_effect = 9; break;
  case 3: entry_effect = 7; break;
  case 4: entry_effect = 6; break;
  default: break;
  }
  switch (signed_byte(*b->substate)) {
  case 1: return begin(b, o, e);
  case 2:
    return signed_byte(*b->clip) == 7 || signed_byte(*b->clip) == 9
        ? reversible(b, o, e) : ordinary(b, entry_effect, o, e);
  case 3: return finishing(b, o, e);
  case 4: {
    int busy;
    if (!playing(o, 1, &busy, e)) return 0;
    if (!busy) {
      if (!*b->transition_latch) { *b->transition_latch = 1; *b->substate = 1; }
      else { b->frame->state_721ee0 = 0; increment(b->cursor); }
      *b->elapsed_bits = 0; b->counters[1] = 0;
    }
    return 1;
  }
  default: return 1;
  }
}
#undef FOUR
#undef PAIR
#undef SIX
#undef CALL
