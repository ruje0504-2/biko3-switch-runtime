#include "game/ending_auxiliary_presentation.h"
#include <math.h>
#include <stdio.h>

static int fail(char e[256], const char *why) {
  if (e) snprintf(e, 256, "auxiliary presentation: %s", why);
  return 0;
}

int bk_ending_auxiliary_presentation_step(
    const BkEndingAuxiliaryPresentationBindings *b, float seconds,
    const BkEndingAuxiliaryPresentationOps *o, char e[256]) {
  if (!b || !b->frame || !b->control || !b->auxiliary || !b->face || !b->face_mode ||
      !b->expression_override || !b->eye_lower || !b->expression_latch ||
      !b->toggles || !b->primary_root || !b->background_root ||
      !b->hidden_nodes || !b->secondary_node || !o || !isfinite(seconds) ||
      seconds < 0)
    return fail(e, "invalid live bindings/time");
#define CALL(member, ...) do { \
  if (!o->member) return fail(e, "missing " #member " service"); \
  if (!o->member(o->context, __VA_ARGS__)) return 0; \
} while (0)
  uint32_t timestamp;
  int32_t active;
  CALL(clock, &timestamp, e);
  CALL(advance, BK_ENDING_AUX_PRESENT_BACKGROUND, seconds, e);

  /* 48181F first updates the loader-owned hidden branch. The three optional
   * nodes are already resolved by the scene, so an absent node is a native
   * no-op and cannot turn into an invented cross-actor lookup. */
  for (unsigned i = 0; i < 3; ++i)
    if (b->hidden_nodes[i])
      CALL(hide, b->hidden_nodes[i], b->toggles[0], e);

  if (b->control->state_721eec == 3) {
    CALL(active, BK_ENDING_AUX_PRESENT_PRIMARY, &active, e);
    if (active == 4) {
      CALL(advance, BK_ENDING_AUX_PRESENT_PRIMARY,
           (float)((double)seconds * .3), e);
    } else {
      CALL(plain, BK_ENDING_AUX_PRESENT_PRIMARY, 0,
           BK_CLIP_PLAIN_SCHEDULED, e);
    }
  } else if (b->control->state_721eec == 5) {
    float rate = b->frame->group == 4 ? .3f : .5f;
    CALL(advance, BK_ENDING_AUX_PRESENT_PRIMARY,
         (float)((double)seconds * rate), e);
  } else if (b->control->state_721eec == 7 && b->frame->group == 4) {
    CALL(advance, BK_ENDING_AUX_PRESENT_PRIMARY,
         (float)((double)seconds * .2), e);
  } else {
    CALL(advance, BK_ENDING_AUX_PRESENT_PRIMARY,
         (float)((double)seconds * .3), e);
  }

  for (unsigned i = 0; i < 4; ++i) {
    const char *name = bk_ending_presentation_material(b->frame->group, i);
    if (!name) return fail(e, "material group outside native table");
    CALL(material, name, b->toggles[i < 2 ? 3 : 5], i == 1 ? .2f : 1.f, e);
  }
  if (b->frame->group == 2 && *b->secondary_node)
    CALL(hide, *b->secondary_node, !b->toggles[5], e);
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
  CALL(eye_range, (float)*b->eye_lower,
       (float)b->auxiliary->expression_a, e);
  if (b->frame->group == 4)
    CALL(gaze, .001f, .2f, e);
  CALL(blink, timestamp, e);
  float level;
  CALL(level, &level, e);
  CALL(mouth, level, timestamp, e);
#undef CALL
  return 1;
}
