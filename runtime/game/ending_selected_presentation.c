#include "game/ending_selected_presentation.h"
#include <math.h>
#include <stdio.h>

static int fail(char e[256], const char *why) {
  if (e) snprintf(e, 256, "selected presentation: %s", why);
  return 0;
}
int bk_ending_selected_presentation_step(
    const BkEndingSelectedPresentationBindings *b, float seconds,
    const BkEndingSelectedPresentationOps *o, char e[256]) {
  if (!b || !b->frame || !b->auxiliary || !b->face || !b->mode ||
      !b->plain_scheduled || !b->face_mode || !b->expression_override ||
      !b->eye_lower || !b->expression_latch || !b->scale || !b->toggles ||
      !b->primary_root || !b->background_root || !b->hidden_nodes ||
      !b->secondary_node || !o || !isfinite(seconds) || seconds < 0)
    return fail(e, "invalid live bindings/time");
#define CALL(member, ...) do { \
  if (!o->member) return fail(e, "missing " #member " service"); \
  if (!o->member(o->context, __VA_ARGS__)) return 0; \
} while (0)
  uint32_t timestamp;
  int32_t active;
  CALL(clock, &timestamp, e);
  CALL(advance, BK_ENDING_PRESENT_BACKGROUND, seconds, e);
  if (b->frame->group == 2 && !b->auxiliary->variant) {
    uint32_t node = 0;
    CALL(find, *b->primary_root, "OYU", &node, e);
    if (node) CALL(hide, node, b->toggles[0], e);
  }
  CALL(hide, *b->background_root, b->toggles[0], e);
  if (b->frame->auxiliary_mode) {
    CALL(auxiliary_tick, e);
    CALL(active, &active, e);
    int32_t duration;
    CALL(duration, (unsigned)active, &duration, e);
    /*4940d6..e9: signed integer duration, then shared scale, then seconds;
     *keep both products at the original intermediate precision.*/
    CALL(advance, BK_ENDING_PRESENT_PRIMARY,
         (float)((double)duration * (double)*b->scale * (double)seconds), e);
  } else if (b->auxiliary->gate != 3) {
    CALL(advance, BK_ENDING_PRESENT_PRIMARY, (float)((double)seconds * .3f), e);
  } else if (*b->mode == 1 || *b->mode == 2 || *b->mode == 6 || *b->mode == 4) {
    CALL(active, &active, e);
    if (active == 13 || active == 14) {
      float rate = b->frame->group == 2 && b->frame->phase == 6 ? .7f
                   : b->frame->group == 1 && b->frame->phase == 6 ? .6f : .4f;
      CALL(advance, BK_ENDING_PRESENT_PRIMARY, (float)((double)seconds * rate), e);
    } else if (active == 15 || active == 16) {
      CALL(advance, BK_ENDING_PRESENT_PRIMARY, (float)((double)seconds * .7f), e);
    } else {
      CALL(controlled, *b->plain_scheduled ? BK_CLIP_PLAIN_SCHEDULED
                                          : BK_CLIP_PLAIN_SOURCE, e);
    }
  } else if (*b->mode == 0) {
    CALL(controlled, BK_CLIP_PLAIN_FORCE_CHAIN, e);
  } else {
    float rate = *b->mode == 5 ? .5f : .3f;
    CALL(advance, BK_ENDING_PRESENT_PRIMARY, (float)((double)seconds * rate), e);
  }
  for (unsigned i = 0; i < 4; ++i) {
    /*Original5554bc has the same five groups of four names as56e368.*/
    const char *name = bk_ending_presentation_material(b->frame->group, i);
    if (!name) return fail(e, "material group outside native table");
    CALL(material, name, b->toggles[i < 2 ? 3 : 5], i == 1 ? .2f : 1.f, e);
  }
  if (b->frame->group == 2 && b->auxiliary->variant && *b->secondary_node)
    CALL(hide, *b->secondary_node, !b->toggles[5], e);
  for (unsigned i = 0; i < 3; ++i)
    if (b->hidden_nodes[i]) CALL(hide, b->hidden_nodes[i], b->toggles[1], e);
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
  CALL(expression, b->auxiliary->expression_b, e);
  CALL(eye_range, (float)*b->eye_lower, (float)b->auxiliary->expression_a, e);
  if (b->frame->group == 4 && b->frame->phase == 6)
    CALL(gaze, .001f, .2f, e);
  CALL(blink, timestamp, e);
  float level;
  if (b->frame->group == 4 && b->auxiliary->variant) {
    CALL(active, &active, e);
    if (active == 9 || active == 10 || active == 11 || active == 13) {
      level = 9;
    } else if (b->auxiliary->selection == 0 || b->auxiliary->selection == 2) {
      if (active == 15 || active == 16 || active == 17 || active == 18 ||
          active == 20 || active == 21) level = 9;
      else { CALL(level, &level, e); }
    } else { CALL(level, &level, e); }
  } else { CALL(level, &level, e); }
  CALL(mouth, level, timestamp, e);
#undef CALL
  return 1;
}
