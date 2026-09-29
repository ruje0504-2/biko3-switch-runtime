#include "game/ending_tertiary_presentation.h"
#include <math.h>
#include <stdio.h>

static int fail(char e[256], const char *why) {
  if (e) snprintf(e, 256, "tertiary presentation: %s", why);
  return 0;
}
int bk_ending_tertiary_presentation_step(
    const BkEndingTertiaryPresentationBindings *b, float seconds,
    const BkEndingTertiaryPresentationOps *o, char e[256]) {
  if (!b || !b->frame || !b->auxiliary || !b->face || !b->state ||
      !b->face_mode || !b->expression_override || !b->eye_lower ||
      !b->expression_latch || !b->toggles || !b->background_root ||
      !b->hidden_nodes || !b->secondary_node || !b->binding_count || !o ||
      !isfinite(seconds) || seconds < 0)
    return fail(e, "invalid live bindings/time");
#define CALL(member, ...) do { \
  if (!o->member) return fail(e, "missing " #member " service"); \
  if (!o->member(o->context, __VA_ARGS__)) return 0; \
} while (0)
  uint32_t timestamp;
  int32_t active;
  CALL(clock, &timestamp, e);
  CALL(advance, BK_ENDING_TERTIARY_BACKGROUND, seconds, e);
  CALL(hide, *b->background_root, b->toggles[0], e);
  for (unsigned i = 0; i < 3; ++i)
    if (b->hidden_nodes[i])
      CALL(hide, b->hidden_nodes[i], b->toggles[1], e);
  if (b->frame->group == 2) {
    CALL(active, BK_ENDING_TERTIARY_AUXILIARY_FIRST, &active, e);
    uint32_t disabled = active != 7 && active != 8;
    for (unsigned i = 0; i < 3; ++i) CALL(disable_bom, i, disabled, e);
    CALL(active, BK_ENDING_TERTIARY_AUXILIARY_SECOND, &active, e);
    disabled = active != 9 && active != 10;
    for (unsigned i = 3; i < 5; ++i) CALL(disable_bom, i, disabled, e);
  }
  if (*b->state != 3) {
    CALL(active, BK_ENDING_TERTIARY_PRIMARY, &active, e);
    /*53f43c=.6f;53f49c=.3f. Keep the selected branch across callbacks.*/
    float amount = (float)((double)seconds * (active == 3 ? .6f : .3f));
    CALL(advance, BK_ENDING_TERTIARY_PRIMARY, amount, e);
    if (b->frame->group == 2) {
      CALL(advance, BK_ENDING_TERTIARY_AUXILIARY_FIRST, amount, e);
      CALL(advance, BK_ENDING_TERTIARY_AUXILIARY_SECOND, amount, e);
    }
  } else {
    CALL(active, BK_ENDING_TERTIARY_PRIMARY, &active, e);
    if (active == 7 || active == 9) {
      CALL(controlled, BK_ENDING_TERTIARY_PRIMARY, BK_ENDING_TERTIARY_4E18AD, e);
      if (b->frame->group == 2) {
        CALL(active, BK_ENDING_TERTIARY_PRIMARY, &active, e);
        CALL(controlled, active == 7 ? BK_ENDING_TERTIARY_AUXILIARY_FIRST
                                     : BK_ENDING_TERTIARY_AUXILIARY_SECOND,
             BK_ENDING_TERTIARY_4E18AD, e);
      }
    } else {
      CALL(controlled, BK_ENDING_TERTIARY_PRIMARY, BK_ENDING_TERTIARY_4A9019, e);
    }
  }
  for (unsigned i = 0; i < 4; ++i) {
    if (b->frame->group >= 5) return fail(e, "material group outside table");
    CALL(material, b->frame->group, i, b->toggles[i < 2 ? 3 : 5],
         i == 1 ? .2f : 1.f, e);
  }
  if (b->frame->group == 2 && *b->secondary_node)
    CALL(hide, *b->secondary_node, !b->toggles[5], e);
  CALL(publish, e);
  if (b->frame->group == 2)
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
    if (b->face->blink_phase == 1 && !*b->expression_latch) {
      b->auxiliary->expression_b = 5;
      b->face->rapid_count = 1;
      *b->expression_override = b->auxiliary->expression_a;
      b->auxiliary->expression_a = 9;
      *b->eye_lower = 2;
      *b->expression_latch = 1;
    }
  }
  CALL(expression, b->auxiliary->expression_b, e);
  CALL(eye_range, (float)*b->eye_lower, (float)b->auxiliary->expression_a, e);
  CALL(gaze, .001f, .2f, e);
  CALL(blink, timestamp, e);
  float level;
  CALL(level, &level, e);
  CALL(mouth, level, timestamp, e);
#undef CALL
  return 1;
}
