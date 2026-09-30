#include "game/ending_gallery_presentation.h"
#include <math.h>
#include <stdio.h>

static int fail(char e[256], const char *why) {
  if (e) snprintf(e, 256, "gallery presentation: %s", why);
  return 0;
}
static int choice(const BkEndingGalleryPresentationBindings *b, int32_t *out,
                  char e[256]) {
  if (*b->cursor < 0 || (uint32_t)*b->cursor >= b->workspace_capacity)
    return fail(e, "record cursor outside available workspace");
  *out = b->workspace[*b->cursor];
  return 1;
}
int bk_ending_gallery_presentation_step(
    const BkEndingGalleryPresentationBindings *b, float seconds,
    const BkEndingGalleryPresentationOps *o, char e[256]) {
  if (!b || !b->frame || !b->auxiliary || !b->face || !b->area ||
      !b->action || !b->requested || !b->cursor || !b->workspace ||
      !b->reverse || !b->face_mode || !b->expression_override ||
      !b->eye_lower || !b->expression_latch || !b->mouth_falling ||
      !b->mouth_level || !b->toggles || !b->primary_root ||
      !b->background_root || !b->secondary_root || !b->third_root ||
      !b->hidden_nodes || !b->secondary_node || !b->binding_count || !o ||
      !isfinite(seconds) || seconds < 0)
    return fail(e, "invalid live bindings/time");
#define CALL(member, ...) do { \
  if (!o->member) return fail(e, "missing " #member " service"); \
  if (!o->member(o->context, __VA_ARGS__)) return 0; \
} while (0)
#define CHOICE() do { if (!choice(b, &selected, e)) return 0; } while (0)
  uint32_t timestamp;
  int32_t selected, active;
  CALL(clock, &timestamp, e);
  CALL(advance, BK_ENDING_GALLERY_BACKGROUND, seconds, 0, e);
  if (b->frame->group == 2 && !b->auxiliary->variant) {
    uint32_t node = 0;
    CALL(find, *b->primary_root, "OYU", &node, e);
    if (node) CALL(hide, node, b->toggles[0], e);
  }
  CALL(hide, *b->background_root, b->toggles[0], e);
  for (unsigned i = 0; i < 4; ++i) {
    if (b->frame->group >= 5) return fail(e, "material group outside table");
    CALL(material, b->frame->group, i, b->toggles[i < 2 ? 3 : 5],
         i == 1 ? .2f : 1.f, e);
  }
  if (b->frame->group == 2 && b->auxiliary->variant && *b->secondary_node)
    CALL(hide, *b->secondary_node, !b->toggles[5], e);
  CHOICE();
  if (selected == 1) {
    CALL(disable_bom, 0, 0, e);
  } else if (selected == 2) {
    CALL(disable_bom, 0, 0, e);
    CALL(disable_bom, 1, 0, e);
  } else if (b->frame->group == 1 && selected == 4) {
    CALL(disable_bom, 1, 0, e);
  } else {
    CALL(disable_bom, 0, 1, e);
    CALL(disable_bom, 1, 1, e);
  }
  if ((*b->action == 3 ||
       (b->frame->state_721ee0 == 2 && *b->requested == 3)) &&
      b->frame->group == 2) {
    CALL(active, BK_ENDING_GALLERY_SECONDARY, &active, e);
    uint32_t disabled = active != 7 && active != 8;
    for (unsigned i = 0; i < 3; ++i) CALL(disable_bom, i, disabled, e);
    CALL(active, BK_ENDING_GALLERY_THIRD, &active, e);
    disabled = active != 9 && active != 10;
    for (unsigned i = 3; i < 5; ++i) CALL(disable_bom, i, disabled, e);
  }
  if (b->frame->state_721ee0 == 6) CALL(effect, e);
  int directed = 0;
  if (*b->action == 3) { CHOICE(); directed = selected != 15; }
  if (directed) {
    int reverse = *b->reverse == 1;
    float rate = .5f;
    if (b->frame->group == 0) { CHOICE(); if (selected != 16) rate = .3f; }
    float amount = (float)((double)seconds * rate);
    CALL(advance, BK_ENDING_GALLERY_PRIMARY, reverse ? -amount : amount,
         reverse, e);
  } else {
    CALL(advance, BK_ENDING_GALLERY_PRIMARY, (float)((double)seconds * .5), 0, e);
  }
  if (b->frame->state_721ee0 == 6 &&
      ((b->frame->group == 0 && *b->area == 2) ||
       (b->frame->group == 2 && *b->area == 8))) {
    for (unsigned slot = 17; slot <= 18; ++slot) {
      BkEndingClipTiming timing;
      CALL(timing, slot, &timing, e);
      /*Unordered source/end does not reset (x87 C0 is set).*/
      if (timing.source >= timing.end) CALL(rewind, slot, e);
    }
  }
  if (*b->secondary_root) {
    directed = 0;
    if (b->frame->group == 2 && *b->action == 3) {
      CHOICE(); directed = selected != 15;
    }
    if (directed) {
      int reverse = *b->reverse == 1;
      CHOICE();
      if (selected == 16 || (selected == 17 && *b->third_root)) {
        float amount = (float)((double)seconds * .5);
        CALL(advance, selected == 16 ? BK_ENDING_GALLERY_SECONDARY
                                     : BK_ENDING_GALLERY_THIRD,
             reverse ? -amount : amount, reverse, e);
      }
    } else {
      CALL(advance, BK_ENDING_GALLERY_SECONDARY,
           (float)((double)seconds * .5), 0, e);
    }
  }
  for (unsigned i = 0; i < 3; ++i)
    if (b->hidden_nodes[i]) CALL(hide, b->hidden_nodes[i], b->toggles[1], e);
  CALL(publish, e);
  if (*b->secondary_root)
    for (int32_t i = 0; i < *b->binding_count; ++i) {
      if ((uint32_t)i >= b->binding_capacity)
        return fail(e, "BOM pair outside available bindings");
      CALL(follow, (unsigned)i, 0, e);
      CALL(follow, (unsigned)i, 1, e);
    }
  CALL(publish, e);
  if (*b->face_mode) {
    if (b->auxiliary->expression_b == 5 && b->face->blink_phase == 0) {
      b->auxiliary->expression_b = 4;
      b->auxiliary->expression_a = *b->expression_override;
      *b->eye_lower = 0;
      *b->expression_latch = 0;
    }
    if (b->face->blink_phase == 1 && b->face->rapid_count == 1 &&
        !*b->expression_latch) {
      b->auxiliary->expression_b = 5;
      *b->expression_override = b->auxiliary->expression_a;
      b->auxiliary->expression_a = 9;
      *b->eye_lower = 2;
      *b->expression_latch = 1;
    }
  }
  CALL(eye_range, (float)*b->eye_lower, (float)b->auxiliary->expression_a, e);
  if (b->frame->state_721ee0 != 5 && b->frame->state_721ee0 != 6 &&
      b->frame->state_721ee0 != 8)
    CALL(gaze, .001f, .2f, e);
  CALL(expression, b->auxiliary->expression_b, e);
  CALL(blink, timestamp, e);
  float level;
  if (b->frame->state_721ee0 == 5) {
    int group1 = b->frame->group == 1;
    CALL(active, BK_ENDING_GALLERY_PRIMARY, &active, e);
    if (!group1) {
      if (active == 1 || active == 16) {
        CALL(level, &level, e);
        *b->mouth_level = level;
      } else if (active == 2 || active == 14 || active == 15 || active == 3 || active == 9) {
        if (!*b->mouth_falling) {
          *b->mouth_level = (float)((double)*b->mouth_level + (double)seconds * 4);
          if (*b->mouth_level > 9) { *b->mouth_level = 9; *b->mouth_falling = 1; }
        } else {
          *b->mouth_level = (float)((double)*b->mouth_level - (double)seconds * 4);
          if (!(*b->mouth_level > 0)) { *b->mouth_level = 0; *b->mouth_falling = 0; }
        }
      } else {
        *b->mouth_level = 9;
      }
      level = *b->mouth_level;
    } else if (active == 1 || active == 2 || active == 3 || active == 9 ||
               active == 16 || active == 14 || active == 15) {
      CALL(level, &level, e);
    } else { level = 9; }
  } else if (b->frame->state_721ee0 == 6 && b->frame->group == 4 &&
             b->auxiliary->variant) {
    CALL(active, BK_ENDING_GALLERY_PRIMARY, &active, e);
    if (active == 9 || active == 10 || active == 11 || active == 13 ||
        ((b->auxiliary->selection == 0 || b->auxiliary->selection == 2) &&
         (active == 15 || active == 16 || active == 17 || active == 18 ||
          active == 20 || active == 21))) {
      level = 9;
    } else { CALL(level, &level, e); }
  } else { CALL(level, &level, e); }
  CALL(mouth, level, timestamp, e);
#undef CHOICE
#undef CALL
  return 1;
}
